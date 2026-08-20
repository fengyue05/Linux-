#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <string>
#include <iostream>

static std::string szret = "I got a correct result\n";

class StateMachine
{
public:

    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE, 
        CHECK_STATE_HEADER
    };

    enum LINE_STATE {
        LINE_OK,
        LINE_BAD,
        LINE_OPEN
    };  

    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        FORBIDDEN_REQUEST,
        INTERNAL_REQUEST,
        CLOSED_REQUEST
    };
    StateMachine(int readIndex, int cheakIndex, int startLine);
    ~StateMachine();

    void setCheckState(CHECK_STATE checkState) {m_checkState = checkState;}
    void setReadIndex(int readIndex) {m_readIndex = readIndex;}
    void setStartLine(int startLine) {m_startLine = startLine;}
    void setCheckIndex(int checkIndex) {m_checkIndex = checkIndex;}
    int getReadIndex() {return m_readIndex;}
    int getCheckIndex() {return m_checkIndex;}
    int getStartLine() {return m_startLine;}
    CHECK_STATE getCheckState() {return m_checkState;}
    LINE_STATE parse_line (std::string& buffer, int& checked_index, int& read_index);
    HTTP_CODE parse_request(std::string& temp, CHECK_STATE& m_checkState);
    HTTP_CODE parse_headers(std::string& temp); // 这个只是简单的处理一下
    HTTP_CODE parse_content(std::string& buffer, int& checked_index, int& read_index, int& startLine);  

    StateMachine* getStateMachine() const {return m_StateMachine;}
private:
    bool equalIgnoreCase(const std::string& left, const std::string& right);

    StateMachine* m_StateMachine;
    CHECK_STATE m_checkState;
    HTTP_CODE m_httpCode;
    LINE_STATE m_LineState;
    int m_checkIndex;
    int m_readIndex;
    std::string m_buffer;
    std::string temp;
    int m_startLine;
};
#endif