#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <arpa/inet.h>
#include <time.h>

#include <algorithm>
#include <cassert>
#include <chrono>  // 时间库
#include <functional>
#include <queue>
#include <unordered_map>

#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;  // 时钟
typedef std::chrono::milliseconds MS;              // 以毫秒为单位的时间量的类型
typedef Clock::time_point TimeStamp;               // 时间戳/时间点

struct TimerNode {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<(const TimerNode& t) const { return expires < t.expires; }
};

struct HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { Clear(); }
    void Adjust(int id, int timeout);
    void Add(int id, int timeout, const TimeoutCallBack& cb);
    void DoWork(int id);
    void Clear();
    void Tick();
    void Pop();
    int GetNextTick();

private:
    void Del_(size_t i);
    void SiftUp_(size_t i);
    bool SiftDown_(size_t index, size_t n);
    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;  // 小根堆
    std::unordered_map<int, size_t> ref_;
};

#endif