#ifndef _XGDLOG_H_
#define _XGDLOG_H_

#include <iostream>
#include <sstream>

enum LogLevel {
    None = 0,
    Error,
    Normal,
    Debug
};

class XGDLog 
{
public:
    enum Manip { Endl = 176 };

    XGDLog(LogLevel level = Normal) 
        : log_level(level) {}

    ~XGDLog() 
    {
        if (should_log()) 
        {
            std::cerr << oss.str();
        }
    }

    template <typename T>
    XGDLog& operator<<(const T& value) 
    {
        if (should_log()) 
        {
            oss << value;
        }
        return *this;
    }

    XGDLog& operator<<(Manip manip) 
    {
        if (manip == Manip::Endl && should_log()) 
        {
            oss << std::endl;
            std::cerr << oss.str();
            oss.str("");  // Clear the stream after flushing
            oss.clear();
        }
        return *this;
    }

    void set_log_level(LogLevel level) 
    {
        current_level = level;
    }

    bool should_log() const 
    {
        return current_level >= log_level;
    }

    LogLevel get_log_level() const 
    {
        return current_level;
    }

    void print_progress(uint64_t processed, uint64_t total);

private:
    static LogLevel current_level;
    std::ostringstream oss;
    LogLevel log_level;    
};

#endif // _XGDLOG_H_
