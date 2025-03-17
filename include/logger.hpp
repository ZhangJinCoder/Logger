#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <cstdarg>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>

namespace logger {

// 日志级别枚举
enum LogLevel {
    TRACE,  // 跟踪信息
    DEBUG,  // 调试信息
    INFO,   // 普通信息
    WARN,   // 告警信息
    ERROR,  // 错误信息
    FATAL,  // 严重错误
    CLOSE,  // 关闭日志
};

// 日志级别名称
const std::string LogLevelNames[] = {
    "TRACE",    // 跟踪信息
    "DEBUG",    // 调试信息
    "INFO*",    // 普通信息
    "WARN*",    // 告警信息
    "ERROR",    // 错误信息
    "FATAL",    // 严重错误
    "CLOSE",    // 关闭日志
};

// 日志内容结构体
struct LogMessage {
    LogLevel level;     // 日志级别
    std::string time;   // 日志时间
    std::string message;    // 日志内容
};

// 日志管理器
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
        // logConfigFile = logDir + "/logger.conf";   // 日志配置文件路径+名称
        logConfigFile = "./logger.conf";   // 日志配置文件名称
    }

    ~Logger() {
        running = false;        // 停止日志处理线程
        if (logThread.joinable()) {
            cv.notify_one();    // 唤醒日志处理线程
            logThread.join();   // 等待线程结束
        }
        if(timerThread.joinable()) {
            timerThread.join(); // 等待线程结束
        }
        closeLogFile();         // 关闭日志文件
    }
    static Logger* getInstance(std::string logDir = "./log", std::string logModuleName = "default", LogLevel logLevel = LogLevel::INFO, bool outputToTerminal = true) // 获取单例对象
    {
        if (instance == nullptr) {
            instance = new Logger(logDir, logModuleName, logLevel, outputToTerminal);
        }
        return instance;
    }

    void start() 
    {
        running = true;
        logCreateDate = getDate();  // 获取当前日期
        createLogDir();     // 创建日志目录
        createLogFile();    // 创建日志文件
        // 启动日志处理线程
        logThread = std::thread([this]() {
            // std::cout << "日志处理线程启动" << std::endl;
            while(running) {
                if(!logQueue.empty())  {
                    std::unique_lock<std::mutex> lock(logQueueMtx);  // 加锁
                    cv.wait(lock, [this]() { return !logQueue.empty() || !running; });
                    if (!running) break;
                    auto& msg = logQueue.front();
                    if(msg.level >= logLevel) writeLog(msg.level, msg.time, msg.message); // 写入日志
                    logQueue.pop();
                } 
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
    }

    void log(LogLevel level, const char* fmt, ...) 
    {
        if(!running) return;
        va_list args;
        va_start(args, fmt);
        std::string message = format(fmt, args);
        va_end(args);
        addLogQueue(level, message);
    }

protected:
    void createLogDir()  // 创建日志目录 
    {
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
            if (access(currentPath.c_str(), F_OK) != 0) {
                if (mkdir(currentPath.c_str(), 0755) == -1) {
                    std::cerr << "Failed to create directory: " << currentPath << std::endl;
                    return;
                }
            }
        }
    }
    void createLogFile()  // 创建日志文件
    {
        logFileName = getCurrentLogFileName();
        // std::cout << "createLogFile: " << logFileName << std::endl;
        // 创建并打开日志文件
        logfp = std::ofstream(logFileName, std::ios::app);
        if (logfp.is_open()) {
            // logfp.close();
        } else {
            std::cerr << "Failed to create log file: " << logFileName << std::endl;
        }
    }
    void closeLogFile()  // 关闭日志文件
    {
        if (logfp.is_open()) {
            logfp.flush();
            logfp.close();
            std::cout << "closeLogFile: " << logFileName << std::endl;
        }
    }
    void needCreateNewLogFile()  // 判断是否需要创建新的日志文件
    {
        if(getDate().compare(logCreateDate) != 0) {
            // 创建新的日志文件
            createLogFile();
            logCreateDate = getDate();
        }
    }
    void writeLog(LogLevel level, const std::string date, const std::string& message)  // 写入日志
    {
        needCreateNewLogFile();
        if (logfp.is_open()) {      // 输出到文件
            if(level < CLOSE) logfp << "[" << date << "] [" << LogLevelNames[level] << "] " << message << std::endl;
            else logfp << "[" << date << "] " << message << std::endl;
            logfp.flush();
        }
        if (outputToTerminal) {     // 输出到终端
            if(level < CLOSE) std::cout << "[" << date << "] [" << LogLevelNames[level] << "] " << message << std::endl;
            else std::cout << "[" << date << "] " << message << std::endl;
        }
    }
    void addLogQueue(LogLevel level, const std::string& message) // 添加到日志队列中
    {
        std::string datetime = getDateTime();
        std::unique_lock<std::mutex> lock(logQueueMtx);
        logQueue.push({level, datetime, message});
        cv.notify_one(); // 唤醒等待的线程
    }
    std::string format(const char *fmt, va_list args) // 格式化日志消息
    {
        va_list args_copy;
        va_copy(args_copy, args);
        int size = std::vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        
        std::string result(size+1, '\0');
        std::vsnprintf(&result.front(), size+1, fmt, args);
    
        return result;
    }

    bool checkConfigFileChange()  // 检查日志配置文件是否有变化
    {
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
        return false;
    }

    void loadConfig()  // 加载日志配置文件
    {
        log_map.clear();
        std::ifstream configFile(logConfigFile);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                // 去除空格
                line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
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
                if(_logLevel >= TRACE && _logLevel <= FATAL) {
                    this->logLevel = static_cast<LogLevel>(_logLevel);
                    log(CLOSE, "日志输出级别变更为: %s", LogLevelNames[_logLevel].c_str()); // 记录日志级别变更日志
                }
            }
            catch(const std::exception& e) {
                std::cerr << e.what() << '\n';
            }            
        }
    }

protected:
    std::string getDate()   // 获取当前日期
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
        return ss.str();
    }
    std::string getDateTime() // 获取当前日期和时间
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    std::string getCurrentLogFileName()  // 获取当前日志文件名
    {
        return logDir + "/" + logModuleName + "." + getDate() + ".log";
    }

    std::pair<std::string, std::string> splitByEqual(const std::string& str) {
        size_t pos = str.find('=');
        if (pos == std::string::npos) {
            // 没有找到等号，返回空字符串作为key和value
            return std::make_pair(std::string(), std::string());
        }
        return std::make_pair(str.substr(0, pos), str.substr(pos + 1));
    }

private:
    std::ofstream logfp;        // 日志文件流
    static inline Logger* instance = nullptr;    // 单例实例指针

    std::mutex logQueueMtx;     // 日志队列互斥锁
    std::mutex configMtx;       // 配置变量互斥锁
    std::condition_variable cv; // 条件变量(日志队列)用于线程同步
    std::queue<LogMessage> logQueue; // 日志队列
    std::thread logThread;      // 日志线程成员变量
    std::thread timerThread;    // 定时器线程成员变量
    std::map<std::string, std::string> log_map;     // 配置检查信息
};

// Logger* Logger::instance = nullptr;

static const char* my_basename(const char* path) {
    const char* base = strrchr(path, '/');
    return base ? base+1 : path;
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
#define LOG_TRACE(fmt, ...) LOG(TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG(FATAL, fmt, ##__VA_ARGS__)

}  // namespace Logger

#endif  // LOGGER_HPP