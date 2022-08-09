#include "../include/buffer.h"

// 先判断是否需要扩容, 再移动数据
void Buffer::makeSpace(size_t nBytes) {
    if (writableBytes() + prependBytes() < nBytes + PREPEND_SPACE_SIZE) {
        buffer_.resize(writeIdx_ + nBytes);
    }

    if (readIdx_ < PREPEND_SPACE_SIZE)
        throw runtime_error("Buffer::makeSpace: it is not in the readable region.");

    if (readIdx_ == PREPEND_SPACE_SIZE)
        return;

    // 把所有可读数据移动到起始点
    size_t readable = readableBytes();
    std::copy(buffer_.begin() + readIdx_, 
              buffer_.begin() + writeIdx_, 
              buffer_.begin() + PREPEND_SPACE_SIZE);  
    readIdx_ = PREPEND_SPACE_SIZE;
    writeIdx_ = readIdx_ + readable;

    if (readableBytes() != readable)
        throw runtime_error("Buffer::makeSpace: data has been lost.");
}

ssize_t Buffer::readFd(int fd, int* saveErrno) {
    const size_t writable = writableBytes();
    if (writable <= 0)
        throw runtime_error("Buffer::readFd: this is no available bytes to read.");

    char extraStk[65536];       // 64KB
    struct iovec vec[2];

    vec[0].iov_base = writeStartPtr();
    vec[0].iov_len = writable;
    vec[1].iov_base = extraStk;
    vec[1].iov_len = sizeof(extraStk);

    // 只有当buffer容量不够时, 才会使用extraStk
    ssize_t len = readv(fd, vec, 2);
    if (len < 0) {
        if (saveErrno != nullptr) *saveErrno = errno;
    }
    else if (len > writable) {
        writeIdx_ = buffer_.size();
        writeBytes(extraStk, len - writable);
    }
    else {
        writeIdx_ += len;
    }

    return len;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno) {
    size_t readable = readableBytes();
    if (readable <= 0)
        throw runtime_error("Buffer::writeFd: this is no available bytes to write.");

    ssize_t len = write(fd, beginPtr() + readIdx_, readable);
    if (len < 0) {
        if (saveErrno != nullptr) *saveErrno = errno;
    }
    else {
        readBytes(len);
    }

    return len;
}
