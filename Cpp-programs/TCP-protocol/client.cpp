#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// tests:
// ./client -n 5 -p 3 -w 10 -h 5 -b 20
// ./client -w 100 -b 30 -f 1.csv

// PA4 changes:
// change TCPRequestChannel with TCPRequestChannel and appropriate constructor
// add -a and -r to getopt


// ecgno to use for datamsgs
#define ECGNO 1
using namespace std;



 /*
    push datamsg into request buffer for each request
    push needs msg to be a byte sequence (char*) */
void patient_thread_function (int p_num, int num_req, BoundedBuffer* req_buf) {
    double t = 0.000;
    for (int i = 0; i < num_req; i++) {
        datamsg* msg = new datamsg(p_num, t, ECGNO);
            //         cout << endl;
            // cout << "person: " << msg->person << endl;
            // cout << "seconds: " << msg->seconds << endl;
        req_buf->push((char*)msg, sizeof(datamsg));
        t += 0.004;
        delete msg;
    }
}


/*
    get file size
    open output file
    allocate the memory; fseek
    close file
    while (offset < file_size), produce a filemsg(offset, m) + filename and push to request buffer
    - incrementing offset and care for final message
*/
void file_thread_function (int m, string filename, TCPRequestChannel* rc, BoundedBuffer* Req_Buffer) {
    int msg_size = sizeof(filemsg) + filename.size() + 1;
    int64_t file_size;

    filemsg msg(0, 0);
	char* buf = new char[msg_size];					    // length for buffer = length empty packet		 
    memcpy(buf, &msg, sizeof(filemsg));  			    // puts file message into buffer (packet)
    strcpy(buf + sizeof(filemsg), filename.c_str());    // copies the name of the file, + is to move pointer so we dont overwrite 
    rc->cwrite(buf, msg_size);  					    // writes the packet to the channel

    rc->cread(&file_size, sizeof(int64_t));        // receives the file length
    FILE* output_file = fopen(("received/" + filename).c_str(), "wb");  // maybe wb if binary
    fseek(output_file, file_size, SEEK_SET);         // reset position indicator?
    fclose(output_file);

    int64_t offset = 0;
    int64_t msg_len = 0;
    filemsg* fm = new filemsg(offset, msg_len);
    while (offset < file_size) {
        if (offset + m > file_size) {  // if last chunk
            msg_len = file_size - offset;
        }
        else {        
            msg_len = m;
        }
        fm->length = msg_len;
        fm->offset = offset;            // produce filemsg(offset, length)
        memcpy(buf, fm, sizeof(filemsg));                 
        Req_Buffer->push(buf, msg_size);                // push onto buffer

        offset += msg_len;
    }

    delete fm;
    delete[] buf;

}


/* worker_thread
    forever loop
    pop from request buf
    view line 120 in server (process request) for how to decide current message
    send msg across a TCP channel, collect response
    if DATA:
    - send request across a TCP channel
    - create pair of p_num and from message and data response (double) from server
    - push that pair to the response buffer 
    if FILE:
    - collect filename from the msg
    - open file in update mode
    - fseek(SEEK_SET) to offset of the filemsg
    - write buffer from server
    */
void worker_thread_function (int m, TCPRequestChannel* rc, BoundedBuffer* Req_Buffer, BoundedBuffer* Rsp_Buffer) {
    while(true) {
        char* req_buf = new char[m];
        Req_Buffer->pop(req_buf, m);
        MESSAGE_TYPE msg_type = *((MESSAGE_TYPE*)req_buf);

        if (msg_type == DATA_MSG) {
            datamsg* msg = (datamsg*)req_buf;
            rc->cwrite(req_buf, sizeof(datamsg));    // request data from server
            double data_rsp;
            rc->cread(&data_rsp, sizeof(double));   // server response
            pair<int, double> rsp_pair = make_pair(msg->person, data_rsp);  // std::pair 
            Rsp_Buffer->push((char*)&rsp_pair, sizeof(pair<int, double>));
        }

        else if (msg_type == FILE_MSG) {
            filemsg* msg = (filemsg*)req_buf;
            string filename = req_buf + sizeof(filemsg);    // initialize
            string file_dir = "received/" + filename;

            size_t msg_size = sizeof(filemsg) + filename.size() + 1;
            rc->cwrite(req_buf, msg_size);         // get file name
            char* rsp_buf = new char[m];                
            rc->cread(rsp_buf, m);

            FILE* file(fopen(file_dir.c_str(), "r+")); 
            fseek (file, msg->offset, SEEK_SET );       // open in update mode and write msg to server
            fwrite(rsp_buf, sizeof(char), msg->length, file);
            fclose(file);
            delete[] rsp_buf;
        }

        else if (msg_type == QUIT_MSG) {
            delete[] req_buf;
            break;
        }
        delete[] req_buf;
    }
}



/* hist thread
    forever loop
    pop response form response buf
    call HC::update(resp->p_num, resp->double) */
void histogram_thread_function (int m, HistogramCollection* hists, BoundedBuffer* Rsp_Buffer) {
    char* rsp_buf = new char[m]; 
    while (true) {
        Rsp_Buffer->pop(rsp_buf, m);
        pair<int, double>* rsp_pair = (pair<int,double>*)rsp_buf;  // convert to pair pls?
        //cout << "pair: " << rsp_pair->first << "  " << rsp_pair->second << endl;
        if ((rsp_pair->first == -1) && (rsp_pair->second == -1.11)) {
            break;      // quit msg 
        }
        hists->update(rsp_pair->first, rsp_pair->second);
    }
    delete[] rsp_buf;
}


int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    string a = "";
    string r = "";
    bool fileRequest = false;
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                fileRequest = true;
                break;
            case 'a':
				a = optarg;
                break;
            case 'r':
				r = optarg;
                break;
		}
	}
    
	// fork and exec the server
    // int pid = fork();
    // if (pid == 0) {
    //     execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    // }
    
	// initialize overhead (control channel no longer needed)
	// TCPRequestChannel* chan = new TCPRequestChannel("control", TCPRequestChannel::CLIENT_SIDE);
    TCPRequestChannel* chan = new TCPRequestChannel(a, r);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // create channel for each worker
    vector<TCPRequestChannel*> channels_vec;
    for (int i = 0; i < w+1; i++) {   
        TCPRequestChannel* rc = new TCPRequestChannel(a, r);
        channels_vec.push_back(rc);         // add new channel 
    }

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    // Thread Initialization:
    /*
        if data:
        - create p patient_threads (store in producer array)
        if file: 
        - create 1 file_thread (store in producer)
        create w worker threads (store in worker array)
        - create w channels (store TCP array)
        if data:  (order)
        - create h histogram_threads (store in hist array) */

    thread * p_threads = new thread[p];
    thread * w_threads = new thread[w];
    thread * h_threads = new thread[h];
    thread * f_thread = new thread[1];

    if (!fileRequest) {
        for (int i = 0; i < p; i++) {
            p_threads[i] = thread(patient_thread_function, i+1, n, &request_buffer);
        }
    }

    if (fileRequest) {
        f_thread[0] = thread(file_thread_function, m, f, channels_vec[0], &request_buffer);
    }

    for (int i = 1; i < w; i++) {   // start at i=1 so f_thread takes first channel
        w_threads[i] = thread(worker_thread_function, m, channels_vec[i], &request_buffer, &response_buffer);
    }

    if (!fileRequest) { // order matters, need to make hist threads last
        for (int i = 0; i < h; i++) {
            h_threads[i] = thread(histogram_thread_function, m, &hc, &response_buffer);
        }
    }


    // Joining Threads:
    /*
        join patients if needed
        join the file thread if needed
        quit then join workers
        quit then join hists */

    MESSAGE_TYPE q = QUIT_MSG;

    if (!fileRequest) {
        for (int i = 0; i < p; i++) {
            p_threads[i].join();
        }   
    }

    if (fileRequest) {
        f_thread[0].join();    
    }

    for (int i = 0; i < w; i++) {
        request_buffer.push((char *)&q, sizeof (MESSAGE_TYPE));
    }   


    for (int i = 0; i < w; i++) {
        w_threads[i].join();
    }

    for (int i = 0; i < h; i++) {
        pair<int, double> rsp_pair = make_pair(-1, -1.11);  // quit message
        response_buffer.push((char*)&rsp_pair, sizeof(pair<int, double>));
    }


    if (!fileRequest) {
        for (int i = 0; i < h; i++) {
            h_threads[i].join();
        }
    }

    delete[] p_threads;
    delete[] w_threads;
    delete[] h_threads;    
    delete[] f_thread;

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;


    // quit and close all channels in TCP array
    for(int i = 0; i < w; i++) {
        channels_vec[i]->cwrite((char*)&q, sizeof(MESSAGE_TYPE));
        delete channels_vec[i];
    }
    channels_vec.clear();


    // quit
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;


	// wait for server to exit
	// wait(nullptr);
}