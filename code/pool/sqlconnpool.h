#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>

#include <algorithm>
#include <cassert>
#include <mutex>
#include <queue>
#include <semaphore.h>
#include <string>
#include <thread>

#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool& Instance();  // 单例模式,局部静态变量懒汉模式，线程安全

    MYSQL* GetConn();
    void FreeConn(MYSQL* conn);
    int GetFreeConnCount();

    void Init(const char* host, int port, const char* user, const char* pwd, const char* db_name, int conn_size);
    void ClosePool();

private:
    SqlConnPool();                             // 将构造函数设为 private，避免外部直接创建对象
    SqlConnPool(const SqlConnPool&) = delete;  // 禁止拷贝构造函数
    SqlConnPool& operator=(const SqlConnPool&) = delete;  // 禁止拷贝赋值运算符
    ~SqlConnPool();                                       // 析构函数直到程序结束时才被调用

    int max_conn_;
    int use_count_;
    int free_count_;

    std::queue<MYSQL*> conn_que_;
    std::mutex mtx_;
    sem_t sem_;
};

#endif