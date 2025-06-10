#ifndef LOGGER_H
#define LOGGER_H
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <cstdarg>
#include <mutex>
#include <map>
#include <queue>
#include <thread>
#include <fstream>
#include <algorithm>
#include <condition_variable>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
// #include <sys/stat.h>
#include <io.h>
#include <direct.h>
#else
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <string.h>
#include <sys/types.h>
#include <iomanip>
#endif

// 日志级别枚举
enum LogLevel {
    LV_TRACE,  // 跟踪信息
    LV_DEBUG,  // 调试信息
    LV_INFO,   // 普通信息
    LV_WARN,   // 告警信息
    LV_ERROR,  // 错误信息
    LV_FATAL,  // 严重错误
    LV_CLOSE,  // 关闭日志
};

// 日志级别名称
const std::string LogLevelNames[] = {
#if 0
    "TRACE",    // 跟踪信息
    "DEBUG",    // 调试信息
    "INFO_",    // 普通信息
    "WARN_",    // 告警信息
    "ERROR",    // 错误信息
    "FATAL",    // 严重错误
    "CLOSE",    // 关闭日志
#else 
    "trace",    // 跟踪信息
    "debug",    // 调试信息
    "info·",    // 普通信息
    "warn·",    // 告警信息
    "error",    // 错误信息
    "fatal",    // 严重错误
    "close",    // 关闭日志
#endif 
};

// 日志级别颜色
const std::string LogLevelColors[] = {
    "\033[0;37m",  // 白色
    "\033[0;36m",  // 青色
    "\033[0;32m",  // 绿色
    "\033[0;33m",  // 黄色
    "\033[0;31m",  // 红色
    "\033[0;35m",  // 紫色
};  
// 日志级别颜色重置
const std::string LogLevelReset = "\033[0m";

// 日志内容结构体
struct LogMessage {
    LogLevel level;         // 日志级别
    std::string time;       // 日志时间
    std::string message;    // 日志内容
};

#if defined(_WIN32) || defined(_WIN64)  
#define LOG_SLEEP(n) Sleep(n);  // 单位为毫秒    
#else 
#define LOG_SLEEP(n) usleep(1000 * n);  // 单位为毫秒
#endif

class Logger {
private:
    bool running;           // 是否运行
    bool outputToTerminal;  // 是否输出到终端
    LogLevel logLevel;      // 日志级别
    std::string logDir;     // 日志目录
    std::string logModuleName;  // 日志模块名称
    std::string logFileName;    // 日志文件名
    std::string logCreateDate;  // 日志创建日期
    std::string logConfigFile;  // 日志配置文件

public:
    // 禁止拷贝构造函数和赋值运算符
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger() = delete;
    Logger(std::string logDir, std::string logModuleName, LogLevel logLevel, bool outputToTerminal) {
        this->logDir = logDir;
        this->logModuleName = logModuleName;
        this->logLevel = logLevel;
        this->outputToTerminal = outputToTerminal;
        logConfigFile = "./logger.conf";   // 日志配置文件名称
    }
    ~Logger() {
        running = false; 
    }
    inline static Logger* getInstance(std::string logDir = "./logs", std::string logModuleName = "default", LogLevel logLevel = LogLevel::LV_INFO, bool outputToTerminal = true) // 获取单例对象
    {
        if (instance == nullptr) {
            instance = new Logger(logDir, logModuleName, logLevel, outputToTerminal);
        }
        return instance;
    }
    inline void start() 
    {
        running = true;
        logCreateDate = getDate();  // 获取当前日期
        createLogDir();     // 创建日志目录
        createLogFile();    // 创建日志文件
        // 启动日志处理线程
        logThread = std::thread([this]() {
            // std::cout << "日志处理线程启动" << std::endl;
            while(running) {
                while(!logQueue.empty())  {
                    std::unique_lock<std::mutex> lock(logQueueMtx);  // 加锁
                    cv.wait(lock, [this]() { return !logQueue.empty() || !running; });
                    if (!running) break;
                    auto& msg = logQueue.front();
                    if(msg.level >= logLevel) writeLog(msg.level, msg.time, msg.message); // 写入日志
                    logQueue.pop();
                }
                LOG_SLEEP(10); // 等待10毫秒
            }
            // std::cout << "日志处理线程结束" << std::endl;
        });
        // 启动定时器线程,定时读取日志配置文件
        timerThread = std::thread([this]() {
            // std::cout << "定时器线程启动" << std::endl;
            while(running) {
                if (this->checkConfigFileChange()) {
                    this->loadConfig();
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
            // std::cout << "定时器线程结束" << std::endl;
        });
        LOG_SLEEP(100); // 等待日志处理线程启动
    }

public:
    inline void log(LogLevel level, const char* fmt, ...) 
    {
        if(!running) return;
        va_list args;
        va_start(args, fmt);
        std::string message = format(fmt, args);
        va_end(args);
        addLogQueue(level, message);
    }
    inline void createLogDir()  // 创建日志目录 
    {
#if defined(_WIN32) || defined(_WIN64)
        if (logDir.empty()) {
            logDir = "./logs";  // 默认日志目录为当前目录下的 logs 文件夹
        }
        if (logDir.back() != '\\') {
            logDir += "\\";  // 如果日志目录不以反斜杠结尾，则添加反斜杠
        }
        if (CreateDirectory(logDir.c_str(), NULL)) {
            std::cout << "目录创建成功: " << logDir.c_str() << std::endl;
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_ALREADY_EXISTS) {
                std::cout << "目录已存在: " << logDir.c_str() << std::endl;
            } else {
                std::cout << "创建目录失败，错误码: " << error << std::endl;
            }
        }
#else
        std::stringstream ss(logDir);
        std::string dir;
        std::string currentPath = "";
        while (std::getline(ss, dir, '/')) {
            if (dir.empty()) continue;
            if (currentPath.empty()) {
                currentPath = dir;
            } else {
                currentPath += "/" + dir;
            }
            if(logDir.c_str()[0] == '/') {
                currentPath = std::string("/") + currentPath;
            }
            if (access(currentPath.c_str(), F_OK) != 0) {
                if (mkdir(currentPath.c_str(), 0755) == -1) {
                    std::cerr << "Failed to create directory: " << currentPath << std::endl;
                    // 输出失败原因
                    std::cerr << "Error: " << strerror(errno) << std::endl;
                    return;
                }
            }
        }
#endif
    }
    inline void createLogFile()  // 创建日志文件
    {
#if defined(_WIN32) || defined(_WIN64)
        logFileName = getCurrentLogFileName();
        // 创建并打开日志文件
        logFileHandle = CreateFile(logFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (logFileHandle == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create log file: " << logFileName << std::endl;
            return;
        }
        // 设置文件指针到文件末尾
        SetFilePointer(logFileHandle, 0, NULL, FILE_END);
#else
        logFileName = getCurrentLogFileName();
        // 创建并打开日志文件
        logfp.open(logFileName, std::ios::app);
        if (!logfp.is_open()) {
            std::cerr << "Failed to create log file: " << logFileName << std::endl;
        }
#endif
    }
    inline void closeLogFile()  // 关闭日志文件
    {
#if defined(_WIN32) || defined(_WIN64)
        if (logFileHandle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(logFileHandle);
            CloseHandle(logFileHandle);
            std::cout << L"closeLogFile: " << logFileName << std::endl;
        }
#else
        if (logfp.is_open()) {
            logfp.flush();
            logfp.close();
            std::cout << "closeLogFile: " << logFileName << std::endl;
        }
#endif
    }
    inline void needCreateNewLogFile()  // 判断是否需要创建新的日志文件
    {
        if(getDate().compare(logCreateDate) != 0) {
            // 创建新的日志文件
            createLogFile();
            logCreateDate = getDate();
            // std::cout << "create new log file: " << logCreateDate << std::endl;
        }
    }
    inline void writeLog(LogLevel level, const std::string date, const std::string& message)  // 写入日志
    {
        needCreateNewLogFile();
#if defined(_WIN32) || defined(_WIN64)
        if (logFileHandle!= INVALID_HANDLE_VALUE) {      // 输出到文件
            if(level < LV_CLOSE) {
                std::string logMessage = "[" + date + "] [" + LogLevelNames[level] + "] " + message + "\r\n";
                DWORD bytesWritten = 0;
                WriteFile(logFileHandle, logMessage.c_str(), logMessage.length(), &bytesWritten, NULL);  
            }
        }
#else
        if (logfp.is_open()) {      // 输出到文件
            if(level < LV_CLOSE) logfp << "[" << date << "] [" << LogLevelNames[level] << "] " << message << std::endl;
            else logfp << "[" << date << "] " << message << std::endl;
            logfp.flush();
        }
#endif 
    }
    inline void addLogQueue(LogLevel level, const std::string& message) // 添加到日志队列中
    {
        std::string datetime = getDateTime();
        if (outputToTerminal) {     // 输出到终端
            if(level < LV_CLOSE) std::cout << "[" << datetime << "] [" << LogLevelColors[level] << LogLevelNames[level] << LogLevelReset << "] " << message << std::endl;
            else std::cout << "[" << datetime << "] " << message << std::endl;
        }
        std::unique_lock<std::mutex> lock(logQueueMtx);
        logQueue.push({level, datetime, message});
        cv.notify_one(); // 唤醒等待的线程
    }

protected:
    inline std::string format(const char *fmt, va_list args) // 格式化日志消息
    {
        va_list args_copy;
        va_copy(args_copy, args);
        int size = std::vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        
        std::string result(size+1, '\0');
        std::vsnprintf(&result.front(), size+1, fmt, args);

        return result;
    }
    inline bool checkConfigFileChange()  // 检查日志配置文件是否有变化
    {
#if defined(_WIN32) || defined(_WIN64)
        // 在 Windows 下使用 GetFileAttributesEx 和 FILETIME
        FILE_BASIC_INFO fileInfo;
        HANDLE hFile = CreateFileA(
            logConfigFile.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            // std::cerr << "日志配置文件不存在: " << logConfigFile << std::endl;
            return false;
        }
        FILETIME lastWriteTime;
        if (!GetFileTime(hFile, NULL, NULL, &lastWriteTime)) {
            CloseHandle(hFile);
            // std::cerr << "无法获取文件时间: " << logConfigFile << std::endl;
            return false;
        }
        CloseHandle(hFile);

        // 将 FILETIME 转换为 std::time_t
        ULARGE_INTEGER ull;
        ull.LowPart = lastWriteTime.dwLowDateTime;
        ull.HighPart = lastWriteTime.dwHighDateTime;
        // FILETIME 是 100 纳秒单位，转换为秒
        std::time_t lastModified = ull.QuadPart / 10000000ULL - 11644473600ULL;
        // 检查配置文件是否被修改
        static std::time_t configFileLastModified = 0;
        if (lastModified != configFileLastModified) {
            configFileLastModified = lastModified;
            // std::cout << "日志配置文件已修改，重新加载配置文件" << std::endl;
            return true;
        }
#else
        // 检查配置文件是否存在
        struct stat fileStat;
        if (stat(logConfigFile.c_str(), &fileStat) != 0) {
            // std::cerr << "日志配置文件不存在: " << logConfigFile << std::endl;
            return false;
        }
        // 检查配置文件是否被修改
        static std::time_t configFileLastModified = 0;
        std::time_t lastModified = fileStat.st_mtime;
        if (lastModified != configFileLastModified) {
            configFileLastModified = lastModified;
            // std::cout << "日志配置文件已修改，重新加载配置文件" << std::endl;
            return true;
        }
#endif
        return false;
    }
    inline void loadConfig()  // 加载日志配置文件
    {
        log_map.clear();
        std::ifstream configFile(logConfigFile);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                // 去除空格
                // line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
				line.erase(std::remove_if(line.begin(), line.end(), static_cast<int (*)(int)>(std::isspace)), line.end());
                // 跳过注释行和空行
                if(line.c_str()[0] == '#' || line.c_str()[0] == ';' || line.empty()) {
                    continue;
                }
                auto result = splitByEqual(line);    // 按"="拆分配置项
                if(result.first.empty() || result.second.empty()) continue;
                log_map.insert(std::pair<std::string, std::string>(result.first, result.second));
            }
            configFile.close();
            // 更新配置
            try {
                std::lock_guard<std::mutex> lock(configMtx); // 加锁，防止多线程同时修改配置
                int _logLevel = std::stoi(log_map["log_level"].c_str());
                if(_logLevel >= LV_TRACE && _logLevel <= LV_CLOSE) {
                    this->logLevel = static_cast<LogLevel>(_logLevel);
                    // log(LV_CLOSE, "日志输出级别变更为: %s", LogLevelNames[_logLevel].c_str()); // 记录日志级别变更日志
                }
            }
            catch(const std::exception& e) {
                std::cerr << e.what() << '\n';
            }            
        }
    }
    inline std::string getCurrentLogFileName()  // 获取当前日志文件名
    {
#if defined(_WIN32) || defined(_WIN64)
        return logDir + "\\" + logModuleName + "." + getDate() + ".log";
#else
        return logDir + "/" + logModuleName + "." + getDate() + ".log";
#endif
    }
    inline std::string getDate()   // 获取当前日期
    {
#if defined(_WIN32) || defined(_WIN64)
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        char buffer[16] = {0};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", localTime);
        return std::string(buffer);
#else
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        // std::stringstream ss;
        // ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
        // return ss.str();
        // C++11兼容版本
        std::tm tm_snapshot;
        localtime_r(&in_time_t, &tm_snapshot); // 线程安全版本
        char buffer[16] = {0};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm_snapshot);
        return std::string(buffer);
#endif
    }
    inline std::string getDateTime() // 获取当前日期和时间
    {
#if defined(_WIN32) || defined(_WIN64)
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);
        return std::string(buffer);
#else
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        // std::stringstream ss;
        // ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        // return ss.str();
        // C++11兼容版本
        std::tm tm_snapshot;
        localtime_r(&in_time_t, &tm_snapshot); // 线程安全版本
        char buffer[20] = {0};  // 扩大缓冲区大小
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_snapshot); // 添加时间格式
        return std::string(buffer);
#endif
    }
    inline std::pair<std::string, std::string> splitByEqual(const std::string& str) {
        size_t pos = str.find('=');
        if (pos == std::string::npos) {
            // 没有找到等号，返回空字符串作为key和value
            return std::make_pair(std::string(), std::string());
        }
        return std::make_pair(str.substr(0, pos), str.substr(pos + 1));
    }

private:
    inline static Logger* instance = nullptr;    // 单例实例指针(需要gcc4.8以上版本支持)

#if defined(_WIN32) || defined(_WIN64)
    HANDLE logFileHandle;  // 日志文件句柄
#else
    std::ofstream logfp{nullptr};       // 日志文件流
#endif
    std::mutex configMtx;               // 配置变量互斥锁
    std::mutex logQueueMtx;             // 日志队列互斥锁
    std::queue<LogMessage> logQueue;    // 日志队列
    std::thread logThread;              // 日志线程成员变量
    std::thread timerThread;            // 定时器线程成员变量
    std::condition_variable cv;         // 条件变量(日志队列)用于线程同步
    std::map<std::string, std::string> log_map;     // 配置检查信息
};

inline static const char* my_basename(const char* path) {
#if defined(_WIN32) || defined(_WIN64)
    const char* base = strrchr(path, '\\');
    return base? base+1 : path;
#else
    const char* base = strrchr(path, '/');
    return base? base+1 : path;
#endif
}
#define __FILENAME__ my_basename(__FILE__)
// 定义一个通用的日志宏
#define LOG(level, fmt, ...) \
    do { \
        Logger* logger = Logger::getInstance(); \
        if (logger) { \
            logger->log(level, "[%s:%d] " fmt, __FILENAME__, __LINE__, ##__VA_ARGS__); \
        } \
    } while (0)
// 使用通用日志宏定义具体的日志级别宏
#define LOG_TRACE(fmt, ...) LOG(LV_TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(LV_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LV_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LV_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LV_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG(LV_FATAL, fmt, ##__VA_ARGS__)

#endif // LOGGER_H