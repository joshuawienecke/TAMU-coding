#include "BoundedBuffer.h"

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

// 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    // use 6. refernce link (vector consturctor)    
// 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
// 3. Then push the vector at the end of the queue
// 4. Wake up threads that were waiting for push
    // for the function use the condition var consumer_ready
    // notifying data available
void BoundedBuffer::push (char* msg, int size) {
    vector<char> char_msg;
    for (int i = 0; i < size; i++) {
        char_msg.push_back(msg[i]);
    }
    unique_lock<std::mutex> lck(m);
    producer_ready.wait(lck, [this]{return (int)q.size() < cap;}); // wait on the return
    q.push(char_msg);
    lck.unlock();
    consumer_ready.notify_one();
}

    // 1. Wait until the queue has at least 1 item
        // waiting on data available    
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
        // use vector::data()
    // 4. Wake up threads that were waiting for pop
        // notifying slot available    
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
int BoundedBuffer::pop (char* msg, int size) {
    std::unique_lock<std::mutex> lck(m);
    consumer_ready.wait(lck, [this]{return (int)(q.size()) > 0;});
    std::vector<char> f_item = q.front();
    q.pop();
    int len = (int)f_item.size();
    assert(len <= size);
    char* item = f_item.data();
    memcpy(msg, item, len);
    lck.unlock();
    producer_ready.notify_one();
    return len;
}

size_t BoundedBuffer::size () {
    return q.size();
}