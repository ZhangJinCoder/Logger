#include <iostream>
#include <unistd.h>
#include "logger.hpp"

using namespace logger;
int main(int argc, char *argv[]) {
   
    Logger::getInstance("./logs", "vpnserver", TRACE, true)->start();
    printf("------------->> 测试开始\n");
    for (int i = 0; i < 100; i++) {
        LOG_TRACE("Hello World!");
        LOG_DEBUG("Hello World!");
        LOG_INFO("Hello World!");
        LOG_WARN("Hello World!");
        LOG_ERROR("Hello World!");
        LOG_FATAL("Hello World!");
        usleep(1000*1000);
    }
    printf("------------->> 测试结束\n");
    // usleep(1000*1000);
    return 0;
}