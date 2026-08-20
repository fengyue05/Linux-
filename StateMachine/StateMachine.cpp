#include "StateMachine.h"

StateMachine::StateMachine(int readIndex, int cheakIndex, int startLine) 
    : m_readIndex(readIndex)
    , m_checkIndex(cheakIndex)
    , m_startLine(startLine)
{
    m_LineState = LINE_OK;
    m_httpCode = NO_REQUEST;
}

StateMachine::~StateMachine()
{

}

StateMachine::LINE_STATE StateMachine::parse_line(std::string& buffer, int & checked_index, int & read_index)
{
    char temp;
    for ( ; checked_index < read_index; checked_index++) {
        temp = buffer[checked_index];
        if (temp == '\r') { // 可能到结束了
            if (checked_index + 1 == read_index) { // 其实还少了一个\n并没有结束
                return LINE_OPEN;
            }
            else if (buffer[checked_index + 1] == '\n') { // 已经结束了
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0'; 
                return LINE_OK;
            }
            return LINE_BAD; // 否则语法就有问题
        }
        else if (temp == '\n') { // 这里其实没有必要判断，但是为了保险起见，我们还是兜底一下
            if ((checked_index > 1 && buffer[checked_index - 1] == '\r')) {
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0'; 
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    // 一个循环还没有结束，说明还没有读到结束
    return LINE_OPEN;
}

StateMachine::HTTP_CODE StateMachine::parse_request(std::string& temp, CHECK_STATE& m_checkState)
{    
    int methodEnd = temp.find_first_of(" \t");
    if (methodEnd == std::string::npos) {
        return BAD_REQUEST;
    }

    std::string method = temp.substr(0, methodEnd);
    if (!equalIgnoreCase(method, "GET")) {
        std::cout << "Only GET method is supported" << std::endl;
        return BAD_REQUEST;
    }

    int urlBegin = temp.find_first_not_of(" \t", methodEnd);
    int urlEnd = temp.find_first_of(" \t", urlBegin);
    if (urlBegin == std::string::npos) {
        return BAD_REQUEST;
    }
    if (urlEnd == std::string::npos) {
        return BAD_REQUEST;
    }

    std::string url = temp.substr(urlBegin, urlEnd - urlBegin);
    
    int versionBegin = temp.find_first_not_of(" \t", urlEnd);
    if (versionBegin == std::string::npos) {
        return BAD_REQUEST;
    }
    int versionEnd = temp.find_first_of (" \t", versionBegin);
    std::string version;
    if (versionEnd == std::string::npos) {
        version = temp.substr(versionBegin);
    }
    else {
        version = temp.substr(versionBegin, versionEnd - versionBegin);
    }

    if (!equalIgnoreCase(version, "HTTP/1.1")) {
        return BAD_REQUEST;
    }

    if (versionEnd != std::string::npos) {
        int extra = temp.find_first_not_of(" \t\r\n", versionEnd);
        if (extra != std::string::npos) {
            return BAD_REQUEST;
        }
    }

    // 7. 处理绝对URL
    // http://www.example.com/index.html
    const std::string httpPrefix = "http://";

    if (url.size() >= httpPrefix.size() && equalIgnoreCase(url.substr(0, httpPrefix.size()), httpPrefix)) {

        std::size_t pathPosition = url.find('/', httpPrefix.size());

        if (pathPosition == std::string::npos) {
            return BAD_REQUEST;
        }

        url = url.substr(pathPosition);
    }

    if (url.empty() || url[0] != '/') {
        return BAD_REQUEST;
    }

    std::cout << "method:  " << method << '\n';
    std::cout << "url:     " << url << '\n';
    std::cout << "version: " << version << '\n';

    // 请求行解析完成，进入请求头解析状态
    m_checkState = CHECK_STATE_HEADER;

    return NO_REQUEST;
}

StateMachine::HTTP_CODE StateMachine::parse_headers(std::string& temp)
{
    // 遇到了一个空行，说明我们得到了一个正确的HTTP请求
    if (temp.empty()) { 
        return GET_REQUEST;
    }
    std::string host = temp.substr(0, 5); // Host:
    if (equalIgnoreCase(host, "Host:")) {
        std::string ret = temp.substr(5);
        std::cout << "the request host is " << ret << std::endl;
    }
    else {
        std::cout << "I can't handle this header" << std::endl;
    }
    return NO_REQUEST;
}

StateMachine::HTTP_CODE StateMachine::parse_content(std::string& buffer, int& checked_index, int& read_index, int& startLine)
{
    m_LineState = LINE_OK;
    m_httpCode = NO_REQUEST;
    while ((m_LineState = parse_line(buffer, checked_index, read_index)) == LINE_OK) {
        // checked_index已经越过\r\n，因此减2得到\r的位置
        int lineEnd = checked_index - 2;

            // 第二个参数是长度
        std::string temp = buffer.substr(startLine, lineEnd - startLine);

        // 下一行从\r\n后面开始
        startLine = checked_index;

        switch (m_checkState) {
        case CHECK_STATE_REQUESTLINE:
            m_httpCode = parse_request(temp, m_checkState);

            if (m_httpCode == BAD_REQUEST) {
                return BAD_REQUEST;
            }
            break;

        case CHECK_STATE_HEADER:
            m_httpCode = parse_headers(temp);

            if (m_httpCode == BAD_REQUEST) {
                return BAD_REQUEST;
            }

            if (m_httpCode == GET_REQUEST) {
                return GET_REQUEST;
            }

            break;

        default:
            return INTERNAL_REQUEST;
        }
    }
    if (m_LineState == LINE_OPEN) {
        return NO_REQUEST;
    }

    return BAD_REQUEST;
}

bool StateMachine::equalIgnoreCase(const std::string& left, const std::string& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (int i = 0; i < left.size(); i++) {
        char leftChar = left[i];
        char rightChar = right[i];
        if (std::tolower(leftChar) != std::tolower(rightChar)) {
            return false;
        }
    }
    return true;
}