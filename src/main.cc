#include <iostream>
#include <string>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>  // 添加这一行来包含 <windows.h> 头文件
#else
#include <unistd.h>  // 添加这一行来包含 <unistd.h> 头文件
#endif

#include "logger.h"  // 添加这一行来包含 logger.h 头文件

int main() {
    Logger::getInstance("./logs", "test", LV_INFO, true)->start();

    while(1) {
        LOG_TRACE("This is a trace log message.");
        LOG_DEBUG("This is a debug log message.");
        LOG_INFO("This is an info log message.");
        LOG_WARN("This is a warning log message.");
        LOG_ERROR("This is an error log message.");
        LOG_FATAL("This is a fatal log message.");
#if defined(_WIN32) || defined(_WIN64)
        Sleep(1000);
#else
        usleep(1000*1000);
#endif
    }
    return 0;
}