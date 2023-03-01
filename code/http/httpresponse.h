#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap, munmap
#include <sys/stat.h>  // stat
#include <unistd.h>    // close

#include <unordered_map>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& src_dir, std::string& path, bool is_keep_alive = false, int code = -1);
    void MakeResponse(Buffer& buff);                       // 根据请求报文生成响应报文
    void UnmapFile();                                      // 解除文件映射
    char* File();                                          // 返回文件映射内存起始地址
    size_t FileLen() const;                                // 返回文件大小
    void ErrorContent(Buffer& buff, std::string message);  // 返回错误信息
    int Code() const { return code_; }                     // 返回状态码

private:
    void AddStateLine_(Buffer& buff);  // 添加状态行
    void AddHeader_(Buffer& buff);     // 添加头部信息
    void AddContent_(Buffer& buff);    // 添加响应正文

    void ErrorHtml_();
    std::string GetFileType_();

    int code_;            // 状态码
    bool is_keep_alive_;  // 是否保持连接

    std::string path_;     // 资源文件路径
    std::string src_dir_;  // 资源文件根目录

    char* mm_file_;             // 文件映射内存起始地址
    struct stat mm_file_stat_;  // 文件信息

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 文件后缀与文件类型
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 状态码与状态信息
    static const std::unordered_map<int, std::string> CODE_PATH;            // 状态码与错误页面路径
};

#endif