#ifndef EPOLLER_H
#define EPOLLER_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>      // fcntl()
#include <sys/epoll.h>  //epoll_ctl()
#include <unistd.h>     // close()

#include <vector>

class Epoller {
public:
    // 初始化，调用epoll_create创建epoll句柄
    explicit Epoller(int max_event = 1024);
    // 析构，close(epoll_fd_)
    ~Epoller();

    // 向epoll中添加fd，修改fd，删除fd
    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);

    // 调用epoll_wait等待事件发生，返回发生事件的个数
    int Wait(int timeout_ms = -1);

    // 获取发生事件的fd
    int GetEventFd(size_t i) const;
    // 获取发生事件的类型
    uint32_t GetEvents(size_t i) const;

private:
    int epoll_fd_;                            // epoll句柄
    std::vector<struct epoll_event> events_;  // 保存发生事件的数组
};

#endif