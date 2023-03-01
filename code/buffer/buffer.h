#ifndef BUFFER_H
#define BUFFER_H

#include <sys/uio.h>  // readv
#include <unistd.h>   // write

#include <atomic>
#include <cassert>
#include <cstring>  // perror
#include <iostream>
#include <vector>

class Buffer {
public:
    // 使用init_buff_size初始化数组大小
    Buffer(int init_buff_size = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;  // 返回缓冲区中可写数据的长度
    size_t ReadableBytes() const;  // 返回缓冲区中可读数据的长度
    // 缓存区头部预留字节数，即read_pos_之前读过的字节数，用于MakeSpace_()中，避免不必要的扩容操作
    size_t PrependableBytes() const;

    // 返回缓冲区读索引read_pos_位置的指针
    const char* Peek() const;
    // 确保有足够的空间读len个char，若空间不足，内部调用MakeSpace_进行空间调整
    void EnsureWriteable(size_t len);
    // 已经写了len个字节，用于更新写索引write_pos_
    void HasWritten(size_t len);

    // 已读了len个char，更新读索引
    void Retrieve(size_t len);
    // 更新读索引，直到读索引指向end
    void RetrieveUntil(const char* end);
    // 清空缓冲区。
    void RetrieveAll();
    // 将缓冲区内可读的数据全部读出，并清空缓冲区
    std::string RetrieveAllToStr();

    // 返回读索引所指内存的地址
    const char* BeginWriteConst() const;
    char* BeginWrite();

    // 往缓冲区写数据
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    // 从fd中读取数据并写入缓冲区，返回读取到的数据的长度，如果发生错误，则将错误码保存在
    // save_errno指向的变量中。
    ssize_t ReadFd(int fd, int* save_errno);
    // 将缓冲区中的数据写入fd中，回写入的数据的长度。如果发生错误，则将错误码保存在 save_errno
    // 指向的变量中。
    ssize_t WriteFd(int fd, int* save_errno);

private:
    // 返回缓冲区的起始位置的地址
    char* BeginPtr_();
    const char* BeginPtr_() const;

    // 调整空间，以保证能够向其中写入len个char
    void MakeSpace_(size_t len);

    std::vector<char> buffer_;  // char 数组，用于存储数据
    // 使用对应的原子类型，避免竞态条件
    std::atomic<std::size_t> read_pos_;   // 读索引
    std::atomic<std::size_t> write_pos_;  // 写索引
};

#endif