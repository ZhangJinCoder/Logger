#ifndef GLOG_H
#define GLOG_H
#include <time.h>
#include <unistd.h>
#include <string.h>

#define PRINTF_LIKE(n) __attribute__ ((format(printf, n, n+1)))

// 日志级别枚举（必须放在函数声明前）
enum LogLevel {
    LV_TRACE,
    LV_DEBUG,
    LV_INFO,
    LV_WARN,
    LV_ERROR,
    LV_FATAL
};

void glog(const char *format, ...) PRINTF_LIKE(1);
void glog_format(enum LogLevel lv, const char *format, ...) PRINTF_LIKE(2);

inline static const char* my_basename(const char* path) {
    const char* base = strrchr(path, '/');
    return base ? base + 1 : path;
}
#define __FILENAME__ my_basename(__FILE__)

#define LOG(level, fmt, ...) \
    do { \
        glog_format(level, "[%s:%d] " fmt, __FILENAME__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#define LOG_TRACE(fmt, ...) LOG(LV_TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(LV_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LV_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LV_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LV_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG(LV_FATAL, fmt, ##__VA_ARGS__)

#endif // GLOG_H
