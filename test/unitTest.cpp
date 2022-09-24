#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../include/buffer.h"
#include "../include/http_connection.h"

void testBuffer() {
    // TestBuffer tf;      // tf()报错: request for member ‘writeBuffer_’ in ‘tf’, which is of non-class type ‘TestBuffer()’
    Buffer buffer;

    cout << "---------------------- initialize --------------------" << endl;
    cout << "buffer readable: " << buffer.readableBytes() << endl;
    cout << "buffer writable: " << buffer.writableBytes() << endl;
    cout << "buffer prepend: " << buffer.prependBytes() << endl;
    cout << "---------------------- initialize --------------------" << endl;

    // 写入数据
    int fd = open("buffer.txt", O_RDWR);
    char content[] = "GET /sdsa/sd HTTP/1.1 \r\nHost: 192.168.146.131:8080 \r\nConnection: keep-alive \r\nDNT: 1 \r\nUpgrade-Insecure-Requests: 1 \r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/103.0.5060.114 Safari/537.36 Edg/103.0.1264.49 \r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,* /*;q=0.8,application/signed-exchange;v=b3;q=0.9 \r\nAccept-Encoding: gzip, deflate \r\nAccept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6";
    write(fd, content, sizeof(content));

    for (int i = 0; i < 1; ++i) {
        lseek(fd, 0, SEEK_SET);

        cout << "start to write bytes into buffer." << endl;
        cout << "readFromFd: " << buffer.readFromFd(fd) << endl;
        cout << "---------------------- buffer --------------------" << endl;
        cout << "buffer readable: " << buffer.readableBytes() << endl;
        cout << "buffer writable: " << buffer.writableBytes() << endl;
        cout << "buffer prepend: " << buffer.prependBytes() << endl;
        cout << "---------------------- buffer --------------------" << endl;

        const char* start = buffer.findCRLF();
        if (start != nullptr) {
            // *start = 'a';
            cout << "find CRLF. Last line: " << start << endl;
        }
        else
            cout << "not find CRLF." << endl;

        // // 打印数据
        // cout << "start to read bytes from buffer." << endl;
        // cout << "writeToFd: " << buffer.writeToFd(STDOUT_FILENO) << endl;
        // cout << "---------------------- buffer --------------------" << endl;
        // cout << "buffer readable: " << buffer.readableBytes() << endl;
        // cout << "buffer writable: " << buffer.writableBytes() << endl;
        // cout << "buffer prepend: " << buffer.prependBytes() << endl;
        // cout << "---------------------- buffer --------------------" << endl;
    }
}

void createTxt() {
    int fd = open("buffer.txt", O_WRONLY | O_CREAT, 0777);
    cout << "fd: " << fd << endl;

    char bf[] = "GET /6 HTTP/1.1\r\nHost: 192.168.146.131:8080\r\nConnection: keep-alive\r\nContent-Length: 0\r\nDNT: 1\r\nUpgrade-Insecure-Requests: 1\r\n\r\n";
    write(fd, bf, sizeof(bf));
}

void testHttp() {
    int fd = open("buffer.txt", O_RDONLY);

    HttpConnection hc;
    hc.init(fd);

    hc.httpRead();

    hc.httpProcess();
}

int main() {
    // testBuffer();
    createTxt();
    testHttp();

    return 0;
}