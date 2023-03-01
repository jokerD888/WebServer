

#include <unistd.h>

#include "server/webserver.h"

int main() {
    WebServer server(1316, 3, 60000, false,              // 端口     ET模式      timeout_ms      优雅退出
                     3306, "root", "root", "webserver",  // Mysql 配置
                    12, 6, true, 1, 1024);  // 数据库连接池数量  线程池数量  日志开关  日志等级 日志异步
    server.Start();
    return 0;
}