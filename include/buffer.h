#ifndef _BUFFER_H
#define _BUFFER_H

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <sys/uio.h>        // readv
#include <unistd.h>         // write

using namespace std;

class Buffer {
public:
    Buffer(int initialSize = AVAILABLE_SPACE_SIZE) : buffer_(PREPEND_SPACE_SIZE + initialSize) {
        readIdx_ = PREPEND_SPACE_SIZE;
        writeIdx_ = PREPEND_SPACE_SIZE;

        // printStatus();
    }

    ~Buffer() {}

    void printBuffer() {
        cout << "--------------- buffer ---------------" << endl;
        string content(buffer_.begin() + readIdx_, buffer_.begin() + writeIdx_);
        cout << content << endl;
        cout << "--------------- buffer ---------------" << endl;
    }

    void printStatus() {
        cout << "--------------- buffer ---------------" << endl;
        cout << "prependBytes: " << prependBytes() << endl;
        cout << "readableBytes: " << readableBytes() << endl;
        cout << "writableBytes: " << writableBytes() << endl;
        cout << "--------------- buffer ---------------" << endl;
    }

    void reset() {
        std::fill(buffer_.begin(), buffer_.end(), 0);
        readIdx_ = PREPEND_SPACE_SIZE;
        writeIdx_ = PREPEND_SPACE_SIZE;
    }

    size_t readableBytes() const {
        return writeIdx_ - readIdx_;
    }

    size_t writableBytes() const {
        return buffer_.size() - writeIdx_;
    }

    size_t prependBytes() const {
        return readIdx_;
    }

    /*
        不可以修改buffer内容
    */
    const char* beginPtrConst() const {
        return &*buffer_.begin();
    }

    const char* readStartPtrConst() const {
        return beginPtrConst() + readIdx_;
    }

    const char* writeStartPtrConst() const {
        return beginPtrConst() + writeIdx_;
    }

    /*
        可以修改buffer内容
    */
    char* beginPtr() {
        return &*buffer_.begin();
    }

    char* readStartPtr() {
        return beginPtr() + readIdx_;
    }

    char* writeStartPtr() {
        return beginPtr() + writeIdx_;
    }

    const char* findCRLF() const {
        const char CRLF[] = "\r\n";
        const char* res = search(readStartPtrConst(), writeStartPtrConst(), CRLF, CRLF + 2);
        return (res == writeStartPtrConst()) ? nullptr : res;
    }

    const char* findCRLF(const char* start) const {
        if (start < readStartPtrConst() || start >= writeStartPtrConst())
            throw runtime_error("Buffer::findCRLF: it is not in the readable region.");

        const char CRLF[] = "\r\n";
        const char* res = search(start, writeStartPtrConst(), CRLF, CRLF + 2);
        return (res == writeStartPtrConst()) ? nullptr : res;
    }

    // buffer -> other
    void readBytes(size_t nBytes) {
        if (nBytes > readableBytes())
            throw runtime_error("Buffer::readBytes: this is no available bytes to read.");
        readIdx_ += nBytes;
    }
    
    void readBytesUntil(const char* end) {
        if (end > writeStartPtr())
            throw runtime_error("Buffer::readBytesUntil: it is not in the readable region.");
        
        readBytes(end - readStartPtr());
    }

    void readBytesAll() {
        readIdx_ = PREPEND_SPACE_SIZE;
        writeIdx_ = PREPEND_SPACE_SIZE;
    }

    // other -> buffer
    void writeBytes(char* data, size_t nBytes) {
        if (nBytes > writableBytes()) {
            makeSpace(nBytes);
        }
        std::copy(data, data + nBytes, buffer_.begin() + writeIdx_);  // 写入新数据
        writeIdx_ += nBytes;
    }

    ssize_t readFd(int fd, int* saveErrno = nullptr);   // fd -> buffer
    ssize_t writeFd(int fd, int* saveErrno = nullptr);  // buffer -> fd

private:
    void makeSpace(size_t nBytes);

private:
    static constexpr int PREPEND_SPACE_SIZE = 8;
    static constexpr int AVAILABLE_SPACE_SIZE = 1024;

    size_t readIdx_;
    size_t writeIdx_;
    vector<char> buffer_;
};

#endif