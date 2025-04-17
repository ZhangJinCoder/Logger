#include <iostream>
#include <string>
#include "logger.h"  // 添加这一行来包含 logger.h 头文件

int main() {
    // 初始化日志模块
    Logger::getInstance("./logs", "logger", LV_TRACE, true)->start();
    while(1) {
        LOG_TRACE("This is a trace log message.");
        LOG_DEBUG("This is a debug log message.");
        LOG_INFO("This is an info log message.");
        LOG_WARN("This is a warning log message.");
        LOG_ERROR("This is an error log message.");
        LOG_FATAL("This is a fatal log message.");
        LOG_SLEEP(1000);
    }
    return 0;
}