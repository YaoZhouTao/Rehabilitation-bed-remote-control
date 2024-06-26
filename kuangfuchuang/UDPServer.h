#pragma once
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <thread>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include<pthread.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <utility>               // For std::pair
#include <mutex>                // 包含 <mutex> 头文件
#include<map>
#include"Logger.h"
using namespace std;             // 命名空间声明

class UDPServer {
public:
    UDPServer(int port);
    //启动监听线程
    bool Start();
    //结束监听线程
    void Stop();
    //监听下发指令数据库
    static void mysql_down_command();

private:
    //循环接收连接和数据
    void HandleConnections();
    //解析字符串
    static void Parse_strings(void* arg); 
    //mysql数据库操作
    static void mysql_operate(void* arg);
    //检查客户端连接是否超时
    static void checkDeviceTimeouts();
    

    int serverSocket;
    static int fd;
    int port;
    bool running;
    static std::mutex mapMutex;

    //存放客户端ip和端口
    static std::unordered_map<std::string, std::pair<string, int>> IP_Port_Map;

    //记录超时客户端
    static std::map<std::string, std::chrono::system_clock::time_point> deviceTimestamps;
};
