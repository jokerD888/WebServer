#include "sqlconnpool.h"

SqlConnPool& SqlConnPool::Instance() {
    static SqlConnPool connpool;  // 静态局部变量，线程安全
    return connpool;
}
MYSQL* SqlConnPool::GetConn() {
    MYSQL* sql = nullptr;
    if (conn_que_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&sem_);

    {
        std::lock_guard<std::mutex> locker(mtx_);
        sql = conn_que_.front();
        conn_que_.pop();
    }
    return sql;
}

void SqlConnPool::Init(const char* host, int port, const char* user, const char* pwd, const char* db_name,
                       int conn_size = 10) {
    assert(conn_size > 0);
    // 创建conn_size个数据库连接
    for (int i = 0; i < conn_size; ++i) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);  // 如果sql是NULL指针，该函数将分配、初始化、并返回新对象。
        if (!sql) {
            LOG_ERROR("Mysql init error!");
        }
        sql = mysql_real_connect(sql, host, user, pwd, db_name, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySQL Connect error!");
        }
        conn_que_.push(sql);
    }
    max_conn_ = conn_size;
    sem_init(&sem_, 0, max_conn_);  // 设置信号量，中间参数为0，表只为当前进程的所有线程共享
}

void SqlConnPool::FreeConn(MYSQL* conn) {
    assert(conn);
    std::lock_guard<std::mutex> locker(mtx_);  // 保护对共享变量的访问
    conn_que_.push(conn);
    sem_post(&sem_);
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);
    return conn_que_.size();
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> locker(mtx_);
    // 关闭所有连接
    while (!conn_que_.empty()) {
        auto item = conn_que_.front();
        conn_que_.pop();
        mysql_close(item);
    }
    mysql_library_end();  // 终止使用MySQL库
}

SqlConnPool::SqlConnPool() : use_count_(0), free_count_(0) {}
SqlConnPool::~SqlConnPool() { ClosePool(); }
