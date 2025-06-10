#include "glog.h"

int main()
{
    LOG_TRACE("Trace message\n");
    LOG_DEBUG("Debug message\n");
    LOG_INFO("Info message\n");
    LOG_WARN("Warn message\n");
    LOG_ERROR("Error message\n");
    LOG_FATAL("Fatal message\n");
    return 0;
}