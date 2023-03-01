#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <errno.h>
#include <mysql/mysql.h>  //mysql

#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
public:
    // 解析状态
    enum PARSE_STATE {
        REQUEST_LINE,  // 请求行
        HEADERS,       // 请求头
        BODY,          // 请求体
        FINISH,        // 解析完成
    };
    // 解析结果
    enum HTTP_CODE {
        NO_REQUEST = 0,      // 请求不完整，需要继续读取
        GET_REQUEST,         // 获得了完整的请求
        BAD_REQUEST,         // 客户端请求有语法错误
        NO_RESOURSE,         // 没有资源
        FORBIDDENT_REQUEST,  // 客户端对资源没有足够的访问权限
        FILE_REQUEST,        // 文件请求
        INTERNAL_ERROR,      // 服务器内部错误
        CLOSED_CONNECTION,   // 客户端已经关闭连接
    };

    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool Parse(Buffer& buff);

    std::string Path() const;
    std::string& Path();
    std::string Method() const;
    std::string Version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

private:
    bool ParseRequestLine_(const std::string& line);  // 解析请求行
    void ParseHeader_(const std::string& line);       // 解析请求头
    void ParseBody_(const std::string& line);         // 解析请求体

    void ParsePath_();            // 解析路径
    void ParsePost_();            // 解析post请求
    void ParseFromUrlencoded_();  // 解析url编码

    static bool UserVerify(const std::string& name, const std::string& pwd, bool is_login);

    PARSE_STATE state_;                                    // 解析状态
    std::string method_, path_, version_, body_;           // 请求方法，请求路径，http版本，请求体
    std::unordered_map<std::string, std::string> header_;  // 请求头
    std::unordered_map<std::string, std::string> post_;    // post请求参数

    static const std::unordered_set<std::string> DEFAULT_HTML;           //  默认的html文件
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;  // 默认的html文件对应的tag
    static int ConverHex(char ch);                                       // 将十六进制转换为十进制
};

#endif