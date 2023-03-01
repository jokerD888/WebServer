#ifndef LOG_H
#define LOG_H

#include <stdarg.h>    // vastart va_end
#include <sys/stat.h>  // mkdir
#include <sys/time.h>

#include <cassert>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "../buffer/buffer.h"
#include "blockqueue.h"

class Log {
public:
    // 初始化，指定日志参数
    void Init(int level, const char* path = "./log", const char* suffix = ".log", int max_queue_capacity = 1024);

    static Log& Instance();        // 单例模式
    static void FlushLogThread();  // 回调函数，线程异步写日志

    // 向缓冲区写
    void Write(int level, const char* format, ...);
    void Flush();  // 刷新对fp_的写

    int GetLevel();            // 获取日志等级
    void SetLevel(int level);  // 设置日志等级
    bool IsOpen() { return is_open_; }

private:
    Log();
    virtual ~Log();
    Log(const Log&) = delete;             // 禁止拷贝构造函数
    Log& operator=(const Log&) = delete;  // 禁止拷贝赋值运算符

    void AppendLogLevelTitle_(int level);  // 添加日志等级标志
    void AsyncWrite_();                    // 异步写

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* path_;    // 路径
    const char* suffix_;  // 文件名后缀

    int max_lines_;   // 单个日志文件最大行数
    int line_count_;  // 当前日志文件已写行数
    int to_day_;      // 截至日期，记录当前时间是那天
    int level_;       // 日志级别

    bool is_async_;  // 是否异步
    bool is_open_;   // 日志是否打开

    Buffer buff_;  // 读写数据缓冲区

    FILE* fp_;
    std::unique_ptr<BlockDeque<std::string>> deque_;  // 阻塞队列
    // 异步写入日志的线程
    std::unique_ptr<std::thread> write_thread_;
    std::mutex mtx_;
};

// 加上 while(0) 可以确保宏定义始终是一个语句块,避免在使用宏的时候出现语法错误。
#define LOG_BASE(level, format, ...)                   \
    do {                                               \
        Log& log = Log::Instance();                    \
        if (log.IsOpen() && log.GetLevel() <= level) { \
            log.Write(level, format, ##__VA_ARGS__);   \
            log.Flush();                               \
        }                                              \
    } while (0);

#define LOG_DEBUG(format, ...)             \
    do {                                   \
        LOG_BASE(0, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_INFO(format, ...)              \
    do {                                   \
        LOG_BASE(1, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_WARN(format, ...)              \
    do {                                   \
        LOG_BASE(2, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_ERROR(format, ...)             \
    do {                                   \
        LOG_BASE(3, format, ##__VA_ARGS__) \
    } while (0);

#endif