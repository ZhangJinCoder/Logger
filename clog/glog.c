#include <stdio.h>
#include <stdarg.h>
#include <threads.h>
#include "glog.h"
#include <pthread.h>

void glog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// 获取当前日期和时间，格式为YYYY-MM-DD HH:MM:SS
void getDateTime(char* buffer, int size) 
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    buffer[size - 1] = '\0';
}
void getLogLevelStr(enum LogLevel lv, char* buffer, int size) // 获取日志级别字符串
{
    switch(lv) {
        case LV_TRACE: strcpy(buffer, "\033[0;37mtrace\033[0m"); break;
        case LV_DEBUG: strcpy(buffer, "\033[0;36mdebug\033[0m"); break;
        case LV_INFO:  strcpy(buffer, "\033[0;32minfo \033[0m");  break;
        case LV_WARN:  strcpy(buffer, "\033[0;33mwarn \033[0m");  break;
        case LV_ERROR: strcpy(buffer, "\033[0;31merror\033[0m"); break;
        case LV_FATAL: strcpy(buffer, "\033[0;35mfatal\033[0m"); break;
    }
    buffer[size - 1] = '\0';
}

void glog_format(enum LogLevel lv, const char *format, ...)
{
    static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
    char datetime[22] = {0}, levelname[18] = {0};
    getDateTime(datetime, sizeof(datetime));
    getLogLevelStr(lv, levelname, sizeof(levelname));
    // 每条日志确保完整性和顺序性
    pthread_mutex_lock(&log_mutex);  // 加锁
    glog("[%s] [%s] ", datetime, levelname);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    pthread_mutex_unlock(&log_mutex);  // 解锁
}
