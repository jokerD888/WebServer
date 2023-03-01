#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::Parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() <= 0) {  // 没有数据
        return false;
    }
    while (buff.ReadableBytes() && state_ != FINISH) {  // 有数据 且 解析没有结束
        // 从缓冲区中找到\r\n
        const char* line_end = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), line_end);
        switch (state_) {
            case REQUEST_LINE:  // 解析请求行
                if (!ParseRequestLine_(line)) return false;
                ParsePath_();
                break;
            case HEADERS:  // 解析请求头
                ParseHeader_(line);
                // 若只剩下两个字节，即\r\n，说明请求头解析完毕 且 没有请求体，状态转移为FINISH
                if (buff.ReadableBytes() <= 2) {
                    state_ = FINISH;
                }
                break;
            case BODY:  // 解析请求体
                ParseBody_(line);
                break;
            default:
                break;
        }
        if (line_end == buff.BeginWrite()) {
            if (method_ == "POST" && state_ == FINISH) {
                buff.RetrieveUntil(line_end);
            }
            break;
        }
        buff.RetrieveUntil(line_end + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

std::string HttpRequest::Path() const { return path_; }

std::string& HttpRequest::Path() { return path_; }

std::string HttpRequest::Method() const { return method_; }

std::string HttpRequest::Version() const { return version_; }

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key.size());
    if (post_.count(key) == 1) return post_.find(key)->second;
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const {
    if (header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}
// 解析请求行
bool HttpRequest::ParseRequestLine_(const std::string& line) {
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");  // 正则表达式
    smatch sub_match;
    if (regex_match(line, sub_match, patten)) {
        method_ = sub_match[1];
        path_ = sub_match[2];
        version_ = sub_match[3];
        state_ = HEADERS;  // 状态转移到请求头
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const std::string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch sub_match;
    if (regex_match(line, sub_match, patten)) {
        header_[sub_match[1]] = sub_match[2];
    } else {  // 匹配不到了，说明是空行，请求头解析结束
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 解析路径，将路径转换为具体文件路径
void HttpRequest::ParsePath_() {
    if (path_ == "/") {
        path_ = "/index.html";
    } else {
        for (auto& item : DEFAULT_HTML) {
            if (item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}
// 解析POST请求，即解析请求体
void HttpRequest::ParsePost_() {
    // 这里的POST请求只处理了application/x-www-form-urlencoded格式的请求，对于本项目来说只有登录和注册需要提交表单数据
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if (DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (0 == tag || 1 == tag) {  // 0表示注册，1表示登录
                bool is_login = (tag == 1);
                // 验证用户名和密码
                if (UserVerify(post_["username"], post_["password"], is_login)) {
                    path_ = "/welcome.html";
                } else {
                    path_ = "/error.html";
                }
            }
        }
    }
}
// 解码，分离出键值对
void HttpRequest::ParseFromUrlencoded_() {
    if (body_.empty()) return;

    string key, value;
    int num = 0, n = body_.size();
    int i = 0, j = 0;

    for (; i < n; ++i) {
        char ch = body_[i];
        switch (ch) {
            case '=':  // 遇到=，说明key解析完毕
                key = body_.substr(j, i - j);
                j = i + 1;
                break;
            case '+':  // 遇到+，说明空格
                body_[i] = ' ';
                break;
            case '%':  // 遇到%，说明是十六进制数
                num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':  // 遇到&，说明value解析完毕
                value = body_.substr(j, i - j);
                j = i + 1;
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    assert(j <= i);
    if (post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool is_login) {
    if (name.empty() || pwd.empty()) return false;
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());

    MYSQL* sql;

    SqlConnRAII sql_raii(&sql, SqlConnPool::Instance());
    assert(sql);

    bool not_used = false;      // 用户名未被使用过
  
    char order[256]{0};
    MYSQL_RES* res = nullptr;

    if (!is_login) not_used = true;
    // 查询用户及密码SQL语句
    snprintf(order, 256, "select username,password from user where username='%s' limit 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 查询失败
    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);    // 获取结果集

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        if (is_login) {     // 登录
            if (pwd == password)
                not_used = true;
            else {
                not_used = false;
                LOG_DEBUG("pwd error!");
            }
        } else {        // 注册
            not_used = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    if (!is_login && not_used) {        // 注册 且 用户名未被注册
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        // 将用户及密码插入数据库
        snprintf(order, 256, "insert into user(username,password) values('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            not_used = false;
        }
        not_used = true;
    }

    LOG_DEBUG("UserVerify success!");

    return not_used;
}

int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}
