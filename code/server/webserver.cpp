#include "webserver.h"

WebServer::WebServer(int port, int trig_mode, int timeout_ms, bool opt_linger, int sql_port, const char* sql_uesr_,
                     const char* sql_pwd, const char* db_name, int conn_pool_num, int thread_num, bool open_log,
                     int log_level, int log_que_size)
    : port_(port),
      open_linger_(opt_linger),
      timeout_ms_(timeout_ms),
      is_close_(false),
      timer_(new HeapTimer()),
      thread_pool_(new ThreadPool(thread_num)),
      epoller_(new Epoller()) {
    // getcwd()函数用于获取当前工作目录，即当前进程所在的目录
    src_dir_ = getcwd(nullptr, 256);
    assert(src_dir_);
    // 定位到资源文件目录下
    strncat(src_dir_, "/../resources/", 16);

    HttpConn::user_count_ = 0;
    HttpConn::src_dir_ = src_dir_;
    SqlConnPool::Instance().Init("localhost", sql_port, sql_uesr_, sql_pwd, db_name, conn_pool_num);

    InitEventMode_(trig_mode);
    if (!InitSocket_()) is_close_ = true;

    if (open_log) {
        Log::Instance().Init(log_level, "./log", ".log", log_que_size);
        if (is_close_) {
            LOG_ERROR("========== Server init error!==========");
        } else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, opt_linger ? "true" : "false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s", (listen_event_ & EPOLLET ? "ET" : "LT"),
                     (conn_event_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: %d", log_level);
            LOG_INFO("srcDir: %s", HttpConn::src_dir_);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", conn_pool_num, thread_num);
        }
    }
}

WebServer::~WebServer() {
    close(listen_fd_);
    is_close_ = true;
    free(src_dir_);
    SqlConnPool::Instance().ClosePool();
}

void WebServer::Start() {
    int time_ms = -1;  // -1表无限等待
    if (!is_close_) {
        LOG_INFO("========== Server start ==========");
    }

    while (!is_close_) {
        if (timeout_ms_ > 0) {
            time_ms = timer_->GetNextTick();
        }
        int event_cnt = epoller_->Wait(time_ms);
        for (int i = 0; i < event_cnt; ++i) {
            // 处理事件
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if (fd == listen_fd_) {  // 有新连接
                DealListen_();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 对端关闭写端，对端关闭，出错
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);    // 关闭连接
            } else if (events & EPOLLIN) {  // 客户端发送数据
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            } else if (events & EPOLLOUT) {  // 客户端可写
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    // 端口号必须在0-65535之间，但其中0-1023为系统保留端口号
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!", port_);
        return false;
    }

    addr.sin_family = AF_INET;                 // ipv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 任意ip
    addr.sin_port = htons(port_);

    struct linger opt_linger = {0};
    if (open_linger_) {
        // 优雅关闭，直到剩余数据发送完毕或超时
        opt_linger.l_onoff = 1;   // 开启优雅关闭
        opt_linger.l_linger = 3;  // 超时时间为3s
    }
    // ipv4 tcp
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }
    // 对listen_fd_设置优雅关闭
    ret = setsockopt(listen_fd_, SOL_SOCKET, SO_LINGER, &opt_linger, sizeof(opt_linger));
    if (ret < 0) {
        close(listen_fd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    // 设置端口复用
    // 防止服务器重启后，端口被占用
    int optval = 1;
    ret = setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_DEBUG("set socket setsockopt error!");
        close(listen_fd_);
        return false;
    }
    // 将套接字地址绑定到套接字描述符listen_fd_上
    ret = bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listen_fd_);
        return false;
    }
    // 将listen_fd_被动的监听套接字，开始监听客户端的连接请求。
    ret = listen(listen_fd_, 1024);
    if (ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listen_fd_);
        return false;
    }

    // 将listen_fd_添加到epoll中
    ret = epoller_->AddFd(listen_fd_, listen_event_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listen_fd_);
        return false;
    }
    // 设置listen_fd_为非阻塞
    SetFdNonblock(listen_fd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

void WebServer::InitEventMode_(int trig_mode) {
    // EPOLLHUP：对端关闭连接
    listen_event_ = EPOLLHUP;
    // EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
    // 设置EPOLLONESHOT
    // 是为了控制同一个套接字文件描述符在一个时间点只能被一个线程进行处理，避免多个线程同时对同一个套接字进行处理而出现错误的情况。
    // conn_event_为什么要设置EPOLLHUP？
    // 因为在ET模式下，如果不设置EPOLLHUP，当客户端关闭连接时，服务器端会一直收到EPOLLIN事件，导致服务器端的CPU占用率很高。
    conn_event_ = EPOLLONESHOT | EPOLLHUP;

    switch (trig_mode) {
        case 0:
            break;
        case 1:
            conn_event_ |= EPOLLET;
            break;
        case 2:
            listen_event_ |= EPOLLET;
            break;
        case 3:
            conn_event_ |= EPOLLET;
            listen_event_ |= EPOLLET;
            break;
        default:
            listen_event_ |= EPOLLET;
            conn_event_ |= EPOLLET;
            break;
    }
    HttpConn::is_ET_ = (conn_event_ & EPOLLET);
}
// 添加客户端到epoll中
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].Init(fd, addr);  // 为新用户http连接初始化
    if (timeout_ms_ > 0) {      // 如果设置了超时时间，就添加到定时器中
        timer_->Add(fd, timeout_ms_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | conn_event_);  // 添加到epoll中
    SetFdNonblock(fd);                           // 设置非阻塞
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    do {
        int fd = accept(listen_fd_, (struct sockaddr*)&addr, &len);
        if (fd <= 0)
            return;  // 从这里退出
        else if (HttpConn::user_count_ >= kMaxFd) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
        // 若listen_fd_是非阻塞的，accept可能会一次性返回多个连接,所以需要循环accept
    } while (listen_event_ & EPOLLET);
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);  // 更新定时器
    // 添加写任务
    thread_pool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);  // 更新定时器
    // 添加读任务
    thread_pool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

// 向对端发送错误信息
void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}
//  更新定时器，因为有新事件发生，所以需要更新定时器
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if (timeout_ms_ > 0) {
        timer_->Adjust(client->GetFd(), timeout_ms_);
    }
}
// 回调函数，当连接超时被调用。从epoll中删除，从定时器中删除
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

// 读取客户端数据
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int read_errno = 0;

    ret = client->Read(&read_errno);
    if (ret <= 0 && read_errno != EAGAIN) {  // 读取失败
        CloseConn_(client);
        return;
    }
    OnProcess(client);  
}
// 向客户端写数据
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int write_errno = 0;

    ret = client->Write(&write_errno);
    if (client->ToWriteBytes() == 0) {
        // 传输完成
        if (client->IsKeepAlive()) {  
            OnProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (write_errno == EAGAIN) {
            // 继续传输
            epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::OnProcess(HttpConn* client) {

    if (client->Process()) {
        epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), conn_event_ | EPOLLIN);
    }
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
