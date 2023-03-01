#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>  // sockaddr_in
#include <errno.h>
#include <stdlib.h>  // atoi()
#include <sys/types.h>
#include <sys/uio.h>  // readv/writev

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    HttpConn();
    ~HttpConn();
    void Init(int sock_fd, const sockaddr_in& addr);

    ssize_t Read(int* save_error);      // 读取数据
    ssize_t Write(int* save_error);     // 写入数据

    void Close();
    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;

    bool Process();                // 处理请求
    int ToWriteBytes() { return iov_[0].iov_len + iov_[1].iov_len; }        // 待写入的字节数
    bool IsKeepAlive() const { return request_.IsKeepAlive(); }        // 是否保持连接

    static bool is_ET_;                   // 是否是ET模式
    static const char* src_dir_;          // 资源目录
    static std::atomic<int> user_count_;  // 统计用户数量

private:
    int fd_;                   // socket文件描述符
    struct sockaddr_in addr_;  // 对方的socket地址
    bool is_close_;            // 是否关闭连接
    int iov_cnt_;              // writev的io向量数量
    struct iovec iov_[2];      // 用于writev的io向量

    Buffer read_buff_;   // 读缓冲区
    Buffer write_buff_;  // 写缓冲区

    HttpRequest request_;    // 请求报文
    HttpResponse response_;  // 响应报文
};

#endif