#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <cassert>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

// 线程池
class ThreadPool {
public:
    explicit ThreadPool(size_t thread_count = 8) : pool_(std::make_shared<Pool>()) {
        assert(thread_count > 0);
        for (size_t i = 0; i < thread_count; ++i) {
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx);  // 保证对pool的独占式访问
                while (true) {
                    if (!pool->tasks.empty()) {
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();  // 记得解锁
                        task();           // 执行任务
                        locker.lock();    // 加锁
                    } else if (pool->is_closed)
                        break;
                    else  // 无任务，等待
                        pool->cond.wait(locker);
                }
            }).detach();  // 线程分离
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool &&) = default;  // 移动构造函数

    ~ThreadPool() {
        if (static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->is_closed = true;
            }
            // 通知所有等待的线程，由于使用共享指针，所以析构函数不用释放资源，
            // 此外，每个线程都有一份共享指针的拷贝，当引用计数为0时，会自动释放资源
            pool_->cond.notify_all();
        }
    }

    template <class F>
    void AddTask(F &&task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();  // 通知一个等待条件的线程
    }

private:
    struct Pool {
        std::mutex mtx;                           // 互斥锁
        std::condition_variable cond;             // 条件变量
        std::queue<std::function<void()>> tasks;  // 任务队列
        bool is_closed;                           // 线程池是否关闭
    };
    std::shared_ptr<Pool> pool_;     // 线程池
};

#endif