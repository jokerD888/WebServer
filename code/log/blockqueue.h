#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <sys/time.h>

#include <condition_variable>
#include <mutex>
#include <queue>

// 阻塞队列，使用生产者消费者模型
template <class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t max_capacity = 1000);
    ~BlockDeque();
    void Clear();
    bool Empty();
    bool Full();
    void Close();
    size_t Size();
    size_t Capacity();
    T Front();
    T Back();
    void PushBack(const T& item);
    void PushFront(const T& itme);
    bool Pop(T& item);
    bool Pop(T& item, int time_out);
    void Flush();

private:
    std::deque<T> deq_;
    size_t capacity_;
    std::mutex mtx_;
    bool is_close_;
    std::condition_variable cond_consumer_;
    std::condition_variable cond_producer_;
};

#endif

template <class T>
inline void BlockDeque<T>::Flush() {
    cond_consumer_.notify_one();
}

template <class T>
inline BlockDeque<T>::BlockDeque(size_t max_capacity) {
    assert(max_capacity > 0);
    is_close_ = false;
}

template <class T>
inline BlockDeque<T>::~BlockDeque() {
    Close();
}

template <class T>
inline void BlockDeque<T>::Clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template <class T>
inline bool BlockDeque<T>::Empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template <class T>
inline bool BlockDeque<T>::Full() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template <class T>
inline void BlockDeque<T>::Close() {
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        is_close_ = true;
    }
    cond_producer_.notify_all();
    cond_consumer_.notify_all();
}

template <class T>
inline size_t BlockDeque<T>::Size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template <class T>
inline size_t BlockDeque<T>::Capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template <class T>
inline T BlockDeque<T>::Front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template <class T>
inline T BlockDeque<T>::Back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template <class T>
inline void BlockDeque<T>::PushBack(const T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 消费者进行消费，直到队列长度<capacity_
    while (deq_.size() >= capacity_) {
        cond_producer_.wait(locker);
    }
    deq_.push_back(item);
    cond_consumer_.notify_one();
}

template <class T>
inline void BlockDeque<T>::PushFront(const T& itme) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.size() >= capacity_) {
        cond_producer_.wait(locker);
    }
    deq_.push_front(itme);
    cond_consumer_.notify_one();
}

template <class T>
inline bool BlockDeque<T>::Pop(T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.empty()) {
        cond_consumer_.wait(locker);  // 队列为空，等待
        if (is_close_) return false;
    }
    item = deq_.front();
    deq_.pop_front();
    cond_producer_.notify_one();
    return true;
}

template <class T>
inline bool BlockDeque<T>::Pop(T& item, int time_out) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.empty()) {
        // 如果等待超时，false
        if (cond_consumer_.wait_for(locker, std::chrono::seconds(time_out)) == std::cv_status::timeout) {
            return false;
        }
        if (is_close_) return false;
    }
    item = deq_.front();
    deq_.pop_front();
    cond_producer_.notify_one();
    return true;
}
