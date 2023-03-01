#include "log.h"

void Log::Init(int level, const char* path, const char* suffix, int max_queue_capacity) {
    is_open_ = true;
    level_ = level;
    if (max_queue_capacity > 0) {
        is_async_ = true;  // 异步
        if (!deque_) {
            std::unique_ptr<BlockDeque<std::string>> new_deque(new BlockDeque<std::string>);
            deque_ = move(new_deque);
            // FlushLogThread为回调函数,这里表示创建线程异步写日志
            std::unique_ptr<std::thread> new_thread(new std::thread(FlushLogThread));
            write_thread_ = move(new_thread);
        }
    } else {  // <=0 表示同步
        is_async_ = false;
    }

    line_count_ = 0;
    time_t timer = time(nullptr);             // 获取当前的系统时间。
    struct tm* sys_time = localtime(&timer);  // 转换为本地时间
    struct tm t = *sys_time;

    path_ = path;
    suffix_ = suffix;
    char file_name[LOG_NAME_LEN]{0};  // 日志文件名
    snprintf(file_name, LOG_NAME_LEN, "%s/%04d_%02d_%2d%s", path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    to_day_ = t.tm_mday;

    {
        std::lock_guard<std::mutex> locker(mtx_);  // 加锁，保证对共享数据的独占式访问
        buff_.RetrieveAll();                       // 清空buff_
        if (fp_) {                                 // 若已打开某个文件
            Flush();
            fclose(fp_);
        }

        fp_ = fopen(file_name, "a");
        if (fp_ == nullptr) {
            mkdir(path_, 0777);
            fp_ = fopen(file_name, "a");
        }
        assert(fp_ != nullptr);
    }
}

Log& Log::Instance() {
    static Log instance;
    return instance;
}

void Log::FlushLogThread() { Log::Instance().AsyncWrite_(); }

void Log::Write(int level, const char* format, ...) {
    // 获取时间信息
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t time_sec = now.tv_sec;
    struct tm* sys_time = localtime(&time_sec);
    struct tm t = *sys_time;

    va_list valist;

    // 若是新的一天，或单个文件行数达到的上限，新建一个文件
    if (to_day_ != t.tm_mday || (line_count_ && (line_count_ % MAX_LINES == 0))) {
        std::unique_lock<std::mutex> locker(mtx_);
        locker.unlock();

        char new_file[LOG_NAME_LEN];
        char tail[36]{0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (to_day_ != t.tm_mday) {  // 新的一天
            snprintf(new_file, LOG_NAME_LEN - 72, "%s%s%s", path_, tail, suffix_);
            to_day_ = t.tm_mday;  // 更新
            line_count_ = 0;      // 重置
        } else {
            snprintf(new_file, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (line_count_ / MAX_LINES), suffix_);
        }
        locker.lock();  // 上锁
        Flush();        // 刷新缓冲区
        fclose(fp_);    // 关闭旧文件
        fp_ = fopen(new_file, "a");
        assert(fp_ != nullptr);
    }
    {
        std::unique_lock<std::mutex> locker(mtx_);
        line_count_++;
        // 向buff_内写时间
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ", t.tm_year + 1900, t.tm_mon + 1,
                         t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.HasWritten(n);          // 更新，已写n个字符
        AppendLogLevelTitle_(level);  // 追加等级标志

        va_start(valist, format);
        // 写入日志正文
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, valist);
        va_end(valist);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        // 若是异步 且 队列未满，加入队列
        if (is_async_ && deque_ && !deque_->Full()) {
            deque_->PushBack(buff_.RetrieveAllToStr());
        } else {
            // 同步，直接写
            fputs(buff_.Peek(), fp_);
            buff_.RetrieveAll();  // 清空
        }
    }
}

void Log::Flush() {
    if (is_async_) {
        deque_->Flush();  // 唤醒deque_内部的条件变量
    }
    fflush(fp_);
}

int Log::GetLevel() {
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}

Log::Log() {
    line_count_ = 0;
    is_async_ = false;
    write_thread_ = nullptr;
    deque_ = nullptr;
    to_day_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    // 有写线程，且在运行中
    if (write_thread_ && write_thread_->joinable()) {
        //
        while (!deque_->Empty()) {
            deque_->Flush();
        }
        deque_->Close();        // 关闭
        write_thread_->join();  // 回收线程
    }
    if (fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        Flush();
        fclose(fp_);  // 记得close
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch (level) {
        case 0:
            buff_.Append("[Debug]: ", 9);
            break;
        case 1:
            buff_.Append("[Info] : ", 9);
            break;
        case 2:
            buff_.Append("[Warn] : ", 9);
            break;
        case 3:
            buff_.Append("[Error]: ", 9);
            break;
        default:
            buff_.Append("[Info] : ", 9);
            break;
    }
}

void Log::AsyncWrite_() {
    std::string str;
    // 不断的从队列中取出数据并写
    while (deque_->Pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}
