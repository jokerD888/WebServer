#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>  // fcntl()
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>  // close()

#include <iostream>
#include <unordered_map>

#include "../http/httpconn.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/heaptimer.h"
#include "epoller.h"

class WebServer {
public:
    // 初始化数据库连接池，线程池，触发模式，监听端口，优雅关闭连接，日志
    WebServer(int port, int trig_mode, int timeout_ms, bool opt_linger, int sql_port, const char* sql_uesr_,
              const char* sql_pwd, const char* db_name, int conn_pool_num, int thread_num, bool open_log, int log_level,
              int log_que_size);

    ~WebServer();
    void Start();

private:
    // 初始化socket连接
    bool InitSocket_();
    // 初始化触发模式
    void InitEventMode_(int trig_mode);
    // 添加客户端
    void AddClient_(int fd, sockaddr_in addr);

    // 处理连接请求，并调用AddClient_
    void DealListen_();
    // 处理读写事件
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);

    void SendError_(int fd, const char* info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int kMaxFd = 65536;
    static int SetFdNonblock(int fd);

    int port_;          // 端口
    bool open_linger_;  // 优雅关闭连接
    int timeout_ms_;    //  超时时间
    bool is_close_;     //  是否关闭
    int listen_fd_;     //   监听套接字
    char* src_dir_;     //   资源目录

    uint32_t listen_event_;  //  监听事件
    u_int32_t conn_event_;   //  连接事件

    std::unique_ptr<HeapTimer> timer_;         //  定时器
    std::unique_ptr<ThreadPool> thread_pool_;  //  线程池
    std::unique_ptr<Epoller> epoller_;         //  epoll
    std::unordered_map<int, HttpConn> users_;  //  用户列表以及对应的http连接
};

#endif
