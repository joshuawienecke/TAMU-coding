#include "TCPRequestChannel.h"
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // inet_pton()
#include <netdb.h> // getaddrinfo()

// data point transfer
// ./client -n <# data items> -w <# workers> -b <BoundedBuffer size>
// -p <# patients> -h <# histogram threads> -m <buffer capacity> -a <IP address> -r <port no>

// test1
// ./client -a 127.0.0.1 -r 8080 -n 1000 -p 5 -w 100 -h 20 -b 5

using namespace std;

    /*
    if server
        1. SOCKET() with domain, type, protocol        
        // AF_INET = IPv4, use (INADDR_ANY)
        // SOCK_STREAM = TCP
        2. BIND() - assign address to socket ot allow server to listen on port_no (get_addr_info maybe)
        3. (LISTEN(sockfd, int backlog)) - mark socket as listening 
        4. ACCEPT(sockfd, const struct sockaddr* addr, len)
        // normally only a single protocol exists to support a socket within
        // a protocol family, in this case protocol = 0.
    */

TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    if (_ip_address == "") { // server
        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in info;
        info.sin_addr.s_addr = INADDR_ANY;
        info.sin_family = AF_INET;
        info.sin_port = htons(atoi(_port_no.c_str()));

        if (bind(sockfd, (struct sockaddr*)&info, sizeof(info)) < 0) {
            perror("ERROR on binding.");
        }
        listen(sockfd, 5);

    }

    else { // client, socket and connect
        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in info;
        info.sin_family = AF_INET;
        info.sin_port = htons(atoi(_port_no.c_str()));
        inet_pton(AF_INET, _ip_address.c_str(), &info.sin_addr); // c str to binary

        if (connect(sockfd, (struct sockaddr*)&info, sizeof(info)) < 0) {
            perror("ERROR on connect.");
        }
    }
    //cout << "constructor done\n";
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    close(sockfd);
}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_in an_addr;
    socklen_t addr_len;
    int clientfd = accept(sockfd, (struct sockaddr*)&an_addr, &addr_len);
    if (clientfd < 0) {
        cerr << "Error accepting the socket" << endl;
    }
    return clientfd;
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    ssize_t numBytes;
    numBytes = read(sockfd, msgbuf, msgsize);
    return numBytes;
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    ssize_t numBytes;
    numBytes = write(sockfd, msgbuf, msgsize);
    return numBytes;
}
