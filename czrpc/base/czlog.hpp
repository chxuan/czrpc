#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <stdio.h>
#include <ctime>
#include <string>
#include <sstream>
#include <memory>

namespace czrpc
{
namespace base
{
enum class priority_level
{
    fatal = 0,
    alert = 1,
    crit = 2,
    error = 3,
    warn = 4,
    notice = 5,
    info = 6,
    debug = 7
};

#ifdef _WIN32
static void gettimeofday(struct timeval* tv)
{
    uint64_t intervals;
    FILETIME ft;

    GetSystemTimeAsFileTime(&ft);
    intervals = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    intervals -= 116444736000000000;
    tv->tv_sec = static_cast<long>(intervals / 10000000);
    tv->tv_usec = static_cast<long>((intervals % 10000000) / 10);
}
#endif

class logger_stream
{
public:
    logger_stream(const std::string& file_path, const std::string& func_name, unsigned long line, priority_level level) : level_(level)
    {
        buffer_ = std::make_shared<std::ostringstream>();
#ifdef _WIN32
        int pos = file_path.find_last_of("\\");
#else
        int pos = file_path.find_last_of("/");
#endif
        (*buffer_) << file_path.substr(pos + 1) << " " << func_name << "(" << line << ") ";
    }

    ~logger_stream()
    {
        print_log();
    }

    template<typename T>
    logger_stream& operator << (const T& t)
    {
        (*buffer_) << t;
        return *this;
    }

private:
    std::string current_time()
    {
        struct timeval now_tv;
#ifdef _WIN32
        gettimeofday(&now_tv);
#else
        gettimeofday(&now_tv, nullptr);
#endif
        const time_t seconds = now_tv.tv_sec;
        struct tm t;
#ifdef _WIN32
        localtime_s(&t, &seconds);
#else
        localtime_r(&seconds, &t);
#endif
        char time_str[27] = {'\0'};
        snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d.%06d",
                 t.tm_year + 1900,
                 t.tm_mon + 1,
                 t.tm_mday,
                 t.tm_hour,
                 t.tm_min,
                 t.tm_sec,
                 static_cast<int>(now_tv.tv_usec));
        return time_str;
    }

    std::string get_level_str()
    {
        switch (level_)
        {
        case priority_level::fatal: return "[FATAL]";
        case priority_level::alert: return "[ALERT]";
        case priority_level::crit: return "[CRIT]";
        case priority_level::error: return "[ERROR]";
        case priority_level::warn: return "[WARN]";
        case priority_level::notice: return "[NOTICE]";
        case priority_level::info: return "[INFO]";
        case priority_level::debug: return "[DEBUG]";
        default: return "[DEBUG]";
        }
    }

    void print_log()
    {
        std::string log_str;
        log_str += current_time();
        log_str += " ";
        log_str += get_level_str();
        log_str += " ";
        log_str += buffer_->str();
        printf("%s\n", log_str.c_str());
    }

private:
    std::shared_ptr<std::ostringstream> buffer_;
    priority_level level_;      
};

class logger
{
public:
    logger(const char* file_path, const char* func_name, unsigned long line, priority_level level)
        : file_path_(file_path), func_name_(func_name), line_(line), level_(level) {}

    logger_stream log() const
    {
        return logger_stream(file_path_, func_name_, line_, level_);
    }

private:
    std::string file_path_;             
    std::string func_name_;             
    unsigned long line_;          
    priority_level level_; 
};

#define LOCATION_INFO __FILE__, __FUNCTION__, __LINE__

#define log_fatal       logger(LOCATION_INFO, priority_level::fatal).log
#define log_alert       logger(LOCATION_INFO, priority_level::alert).log
#define log_crit        logger(LOCATION_INFO, priority_level::crit).log
#define log_error       logger(LOCATION_INFO, priority_level::error).log
#define log_warn        logger(LOCATION_INFO, priority_level::warn).log
#define log_notice      logger(LOCATION_INFO, priority_level::notice).log
#define log_info        logger(LOCATION_INFO, priority_level::info).log
#define log_debug       logger(LOCATION_INFO, priority_level::debug).log

}
}
