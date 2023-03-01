#include "heaptimer.h"

void HeapTimer::Del_(size_t index) {
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode_(i, n);
        if (!SiftDown_(i, n)) {
            SiftUp_(i);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::SiftUp_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    ssize_t child = i;
    // 父节点,这里的类型需要是有符号，否则会导致符号扩展，造成数据越界
    ssize_t parent = (child - 1) / 2;
    while (child > 0) {  // 这里child>0,才循环，同时parent并不会越界
        if (heap_[parent] < heap_[child]) break;
        SwapNode_(child, parent);
        child = parent;
        parent = (child - 1) / 2;
    }
}

bool HeapTimer::SiftDown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t parent = index;
    size_t child = parent * 2 + 1;  // 左孩子
    while (child < n) {
        // 如果右孩子小于左孩子，child指定右孩子
        if (child + 1 < n && heap_[child + 1] < heap_[child]) ++child;
        if (heap_[parent] < heap_[child]) break;
        SwapNode_(parent, child);
        parent = child;
        child = parent * 2 + 1;
    }
    return parent > index;
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());

    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = i;
}

void HeapTimer::Adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    SiftDown_(ref_[id], heap_.size());
}

void HeapTimer::Add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if (ref_.count(id) == 0) {
        // 新节点，入堆
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        SiftUp_(i);
    } else {
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if (!SiftDown_(i, heap_.size())) {
            SiftUp_(i);
        }
    }
}

void HeapTimer::DoWork(int id) {
    if (heap_.empty() || ref_.count(id) == 0) return;

    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    Del_(i);
}

void HeapTimer::Clear() {
    ref_.clear();
    heap_.clear();
}

void HeapTimer::Tick() {
    if (heap_.empty()) {
        return;
    }

    while (!heap_.empty()) {
        TimerNode node = heap_.front();
        if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        Pop();
    }
}

void HeapTimer::Pop() {
    assert(!heap_.empty());
    Del_(0);
}

int HeapTimer::GetNextTick() {
    Tick();
    int64_t res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if (res < 0) res = 0;
    }
    return res;
}
