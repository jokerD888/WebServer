#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

// MySQL资源在对象构造时初始化，析构时释放
class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool& conn_pool):conn_pool_(conn_pool) {
        *sql = conn_pool.GetConn();
        sql_ = *sql;
    }
    ~SqlConnRAII() {
        if (sql_) conn_pool_.FreeConn(sql_);
    }

private:
    MYSQL* sql_;
    SqlConnPool& conn_pool_;
};

#endif