#include "UDPServer.h"
#include "ThreadPool.hpp"
#include "TaskQueue.hpp"
#include "ConnectionPool.h"

struct Arg {
    int fd = -1;
    socklen_t clientAddressLength = 0;
    sockaddr_in clientAddress{};
    std::mutex mapMutex;
    char buf[100] = { 0 };
};

struct MySQLArg {
    int fd = -1;
    socklen_t clientAddressLength = 0;
    sockaddr_in clientAddress{};
    string sn = "";
    string str3 = "";
    string A = "";
    string B = "";
    string C = "";
};

std::mutex UDPServer::mapMutex;
std::unordered_map<std::string, std::pair<string, int>> UDPServer::IP_Port_Map;
std::map<std::string, std::chrono::system_clock::time_point> UDPServer::deviceTimestamps;
ThreadPool<MySQLArg> MySQL_pool(20, 800, "mysql_pool");
ConnectionPool* MYSQLpool = ConnectionPool::getConnectionPool();
int UDPServer::fd = -1;  // 根据实际情况选择适当的初始值

UDPServer::UDPServer(int port)
{
    this->port = port;
    running = false;
    serverSocket = -1;
}

bool UDPServer::Start() {
    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 绑定地址和端口
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;  // 监听任意地址
    serverAddress.sin_port = htons(port);

    if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::cerr << "Failed to bind address" << std::endl;
        perror("bind");
        return false;
    }

    running = true;
    fd = serverSocket;

    // 启动处理连接的线程
    std::thread handleConnectionsThread(&UDPServer::HandleConnections, this);
    std::thread checkDeviceTimeoutsThread(&UDPServer::checkDeviceTimeouts);
    handleConnectionsThread.detach();
    checkDeviceTimeoutsThread.detach();

    std::cout << "Server started on port " << port << std::endl;

    return true;
}

void UDPServer::Stop() {
    std::cout << "服务器停止运行！" << std::endl;
    running = false;
    close(serverSocket);
}

void UDPServer::HandleConnections() {
    int a = 0;
    ThreadPool<Arg> Parse_strings_pool(20, 100, "Parse_strings_pool");
    sockaddr_in clientAddress{};
    socklen_t clientAddressLength = sizeof(clientAddress);
    while (running) {
        a++;
        char buffer[100] = { 0 };

        ssize_t receivedBytes = recvfrom(serverSocket, buffer, sizeof(buffer), 0,reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddressLength);
        if (receivedBytes == -1) {
            std::cerr << "Failed to receive data" << std::endl;
            continue;
        }
        if (receivedBytes >= 26) {
            Arg* threadarg = new Arg;
            threadarg->fd = serverSocket;
            threadarg->clientAddress = clientAddress;
            threadarg->clientAddressLength = clientAddressLength;
            strcpy(threadarg->buf,buffer);
            Parse_strings_pool.addTask(Task<Arg>(Parse_strings, threadarg));
        }
    }
}

// char buf[50] = 2#RZB02BDH2112030002HRB#30;

void UDPServer::Parse_strings(void* arg)
{
    Arg* threadarg = (Arg*)(arg);
    std::cout << "buf = " << threadarg->buf << endl;

    if ((threadarg->buf[0] == '2' && std::count(threadarg->buf, threadarg->buf + strlen(threadarg->buf), '#') == 2) || (threadarg->buf[0] == '4' && std::count(threadarg->buf, threadarg->buf + strlen(threadarg->buf), '#') == 4) ||
        (threadarg->buf[0] == '5' && std::count(threadarg->buf, threadarg->buf + strlen(threadarg->buf), '#') == 5)) {

        char ipAddress[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(threadarg->clientAddress.sin_addr), ipAddress, INET_ADDRSTRLEN);
        uint16_t port = ntohs(threadarg->clientAddress.sin_port);
        /*std::cout << "IP Address: " << ipAddress << std::endl;
        std::cout << "Port: " << port << std::endl;*/

        std::stringstream ss(threadarg->buf);
        std::string token = "";
        std::string str1 = "", str2 = "", str3 = "", str4 = "", str5 = "", str6 = "";
        char delimiter = '#';
        if (threadarg->buf[0] == '2') {
            if (std::getline(ss, token, delimiter)) {
                str1 = token;
                if (std::getline(ss, token, delimiter)) {
                    str2 = token;
                    if (std::getline(ss, token, delimiter)) {
                        str3 = token;
                    }
                }
            }
            std::string lowerBound = "01";
            std::string upperBound = "29";
            if (str3.length() == 2 && (str3 == "30" || str3 == "32" || (str3 >= lowerBound && str3 <= upperBound))) {
                MySQLArg* mysqlarg = new MySQLArg;
                mysqlarg->sn = str2;
                mysqlarg->str3 = str3;
                mysqlarg->fd = threadarg->fd;
                mysqlarg->clientAddress = threadarg->clientAddress;
                mysqlarg->clientAddressLength = threadarg->clientAddressLength;
                //cout << "进入mysql_operate" << endl;
                MySQL_pool.addTask(Task<MySQLArg>(mysql_operate, mysqlarg));
            }
            else if (str3 == "31") {
                std::time_t now = std::time(nullptr);
                std::tm* timeinfo = std::localtime(&now);

                char buffer[50] = {0};
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d#%H:%M:%S", timeinfo);
                string str7 = string(buffer);
                string strback = str2 + "#" + str3 + "#" + str7;
                ssize_t sentBytes = sendto(threadarg->fd, strback.c_str(), strlen(strback.c_str()), 0, reinterpret_cast<struct sockaddr*>(&threadarg->clientAddress), threadarg->clientAddressLength);
                if (sentBytes == -1) {
                    std::cerr << "Failed to send response to client" << std::endl;
                }
                else {
                    // 添加键值对
                    UDPServer::mapMutex.lock();
                    IP_Port_Map[str2] = std::make_pair(ipAddress, port);
                    UDPServer::mapMutex.unlock();
                }
            }
            else if (str3 == "33") {
                // 判断键是否存在
                UDPServer::mapMutex.lock();
                if (IP_Port_Map.count(str2) > 0) {   //返回值大于0说明设备存在
                    //cout << "存在设备: " << str2 << " IP Adderss = " << IP_Port_Map[str2].first << " Port = " << IP_Port_Map[str2].second << endl;
                    std::chrono::system_clock::time_point currentTime = std::chrono::system_clock::now();
                    deviceTimestamps[str2] = currentTime;
                    if (ipAddress != IP_Port_Map[str2].first || port != IP_Port_Map[str2].second) {
                        IP_Port_Map[str2].first = ipAddress;
                        IP_Port_Map[str2].second = port;
                    }
                }
                else {                   
                    IP_Port_Map[str2] = std::make_pair(ipAddress, port);
                    std::cout << "不存在设备: " << str2 << endl;
                }
                UDPServer::mapMutex.unlock();
            }

        }
        else if (threadarg->buf[0] == '4') {
            if (std::getline(ss, token, delimiter)) {
                str1 = token;
                if (std::getline(ss, token, delimiter)) {
                    str2 = token;
                    if (std::getline(ss, token, delimiter)) {
                        str3 = token;
                        if (std::getline(ss, token, delimiter)) {
                            str4 = token;
                            if (std::getline(ss, token, delimiter)) {
                                str5 = token;
                            }
                        }
                    }
                }
                if (str3 == "51") {
                    MySQLArg* mysqlarg = new MySQLArg;
                    mysqlarg->sn = str2;
                    mysqlarg->str3 = str3;
                    mysqlarg->A = str4;
                    mysqlarg->B = str5;
                    mysqlarg->fd = threadarg->fd;
                    mysqlarg->clientAddress = threadarg->clientAddress;
                    mysqlarg->clientAddressLength = threadarg->clientAddressLength;
                    //cout << "进入mysql_operate" << endl;
                    MySQL_pool.addTask(Task<MySQLArg>(mysql_operate, mysqlarg));
                }
            }
        }
        else if (threadarg->buf[0] == '5') {
            if (std::getline(ss, token, delimiter)) {
                str1 = token;
                if (std::getline(ss, token, delimiter)) {
                    str2 = token;
                    if (std::getline(ss, token, delimiter)) {
                        str3 = token;
                        if (std::getline(ss, token, delimiter)) {
                            str4 = token;
                            if (std::getline(ss, token, delimiter)) {
                                str5 = token;
                                if (std::getline(ss, token, delimiter)) {
                                    str6 = token;
                                }
                            }
                        }
                    }
                }
                if (str3 == "55" || str3 == "80") {
                    MySQLArg* mysqlarg = new MySQLArg;
                    mysqlarg->sn = str2;
                    mysqlarg->str3 = str3;
                    mysqlarg->A = str4;
                    mysqlarg->B = str5;
                    mysqlarg->C = str6;
                    mysqlarg->fd = threadarg->fd;
                    mysqlarg->clientAddress = threadarg->clientAddress;
                    mysqlarg->clientAddressLength = threadarg->clientAddressLength;
                    //cout << "进入mysql_operate" << endl;
                    MySQL_pool.addTask(Task<MySQLArg>(mysql_operate, mysqlarg));
                }
            }

        }
    }
    else {
        std::cout << "Invalid string format." << std::endl;
    }
}

void UDPServer::mysql_operate(void* arg)
{
    //cout << 123 << endl;
    MySQLArg* mysqlarg = (MySQLArg*)arg;
    if (mysqlarg->str3 == "30") {
        char sql[150];
        shared_ptr<MysqlConn> conn = MYSQLpool->getConnection();
        sprintf(sql, "select deadline,zero_operation_completed from new_rzb_down where serial_number = '%s'", mysqlarg->sn.c_str());
        conn->query(sql);
        while (conn->next()) {
            string deadline = conn->value(0);
            string zero_operation_completed = conn->value(1);
            std::cout << "deadline = " << deadline << "zero_operation_completed = " << zero_operation_completed << endl;
            if (conn->value(1) == "0") {
                //把这个zero_operation_completed修改为1
                memset(sql, 0, sizeof(sql));
                sprintf(sql, "update new_rzb_down set zero_operation_completed = 1 where serial_number = '%s'", mysqlarg->sn.c_str());
                conn->update(sql);
            }
            string strback = mysqlarg->sn + "#" + mysqlarg->str3 + "#" + conn->value(0);
            std::cout << "strback = " << strback << endl;
            ssize_t sentBytes = sendto(mysqlarg->fd, strback.c_str(), strlen(strback.c_str()), 0, reinterpret_cast<struct sockaddr*>(&mysqlarg->clientAddress), mysqlarg->clientAddressLength);
            if (sentBytes == -1) {
                std::cerr << "Failed to send response to client" << std::endl;
            }
        }
        return;

    }
    else if (mysqlarg->str3 == "32") {
        //cout << "静茹32" << endl;
        char sql[150];
        shared_ptr<MysqlConn> conn = MYSQLpool->getConnection();
        sprintf(sql, "select deadline,zero_operation_completed from new_rzb_down where serial_number = '%s'", mysqlarg->sn.c_str());
        conn->query(sql);
        while (conn->next()) {
            string deadline = conn->value(0);
            string zero_operation_completed = conn->value(1);
            std::cout << "deadline = " << deadline << "zero_operation_completed = " << zero_operation_completed << endl;
            if (conn->value(1) == "1") {
                //把这个zero_operation_completed修改为0
                memset(sql, 0, sizeof(sql));
                sprintf(sql, "update new_rzb_down set zero_operation_completed = 0 where serial_number = '%s'", mysqlarg->sn.c_str());
                conn->update(sql);
            }
            string strback = mysqlarg->sn + "#" + mysqlarg->str3 + "#" + conn->value(0);
            std::cout << "strback = " << strback << endl;
            ssize_t sentBytes = sendto(mysqlarg->fd, strback.c_str(), strlen(strback.c_str()), 0, reinterpret_cast<struct sockaddr*>(&mysqlarg->clientAddress), mysqlarg->clientAddressLength);
            if (sentBytes == -1) {
                std::cerr << "Failed to send response to client" << std::endl;
            }
        }
        return;
    }

    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    char buffer[50];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    if (mysqlarg->str3 == "51") {
        char sql[500];
        shared_ptr<MysqlConn> conn = MYSQLpool->getConnection();
        sprintf(sql, "update new_rzb_up set function_code = '%s',value1 = '%s',value2 = '%s',up_datetime = '%s' where serial_number = '%s';", mysqlarg->str3.c_str(),mysqlarg->A.c_str(),mysqlarg->B.c_str(),buffer,mysqlarg->sn.c_str());
        conn->update(sql);
    }
    else if (mysqlarg->str3 == "55" || mysqlarg->str3 == "80") {
        char sql[500];
        shared_ptr<MysqlConn> conn = MYSQLpool->getConnection();
        sprintf(sql, "update new_rzb_up set function_code = '%s',value1 = '%s',value2 = '%s',value3 = '%s',up_datetime = '%s' where serial_number = '%s';", mysqlarg->str3.c_str(), mysqlarg->A.c_str(), mysqlarg->B.c_str(),mysqlarg->C.c_str(),buffer,mysqlarg->sn.c_str());
        conn->update(sql);
    }
    else {
        //std::cout << "接收到01-29号命令 :" <<mysqlarg->str3<< endl;
        char sql[300];
        shared_ptr<MysqlConn> conn = MYSQLpool->getConnection();
        sprintf(sql, "update new_rzb_up set function_code = '%s',up_datetime = '%s' where serial_number = '%s';", mysqlarg->str3.c_str(),buffer,mysqlarg->sn.c_str());
        conn->update(sql);
    }
    //cout << 456 << endl;
}

void UDPServer::mysql_down_command()
{
    MysqlConn* conn = new MysqlConn;
    bool a = conn->connect("root", "Hckj201509", "share1", "127.0.0.1", 3306);
    //shared_ptr<MysqlConn> conn = MYSQLpool->getConnection();
    char sql[150] = "select serial_number,down_command from new_rzb_down where down_command != '';";
    string ip;
    int port = 0;
    auto deviceTime = std::chrono::system_clock::now();
    while (true) {
        auto currentTime = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - deviceTime).count() > 10800) {
            delete conn;
            conn = nullptr;
            conn = new MysqlConn;
            conn->connect("root", "Hckj201509", "share1", "127.0.0.1", 3306);
            deviceTime = std::chrono::system_clock::now();
            //std::cout << "更新一次数据库连接" << std::endl;
            info("正常更新数据库连接");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        conn->query(sql);
        while (conn->next()) {
            //std::cout << "sn = " << conn->value(0) << "com = " << conn->value(1) << endl;
            if (conn->value(1) != "") {
                mapMutex.lock();
                // 判断key是否存在
                if (IP_Port_Map.count(conn->value(0)) > 0) {
                    // key存在
                    ip = IP_Port_Map[conn->value(0)].first;
                    port = IP_Port_Map[conn->value(0)].second;
                    std::cout << "设备存在: ip = " << ip << "portt = " << port << endl;
                    char sqlempty[500];
                    sprintf(sqlempty, "update new_rzb_down set down_command = '' where serial_number = '%s';", conn->value(0).c_str());
                    std::cout << sqlempty << endl;
                    conn->update(sqlempty);
                }
                else {
                    //std::cout << "设备不存在" << endl;
                    mapMutex.unlock();
                    continue;
                    // key不存在
                }
                mapMutex.unlock();
                sockaddr_in clientAddress{};
                clientAddress.sin_family = AF_INET;
                clientAddress.sin_addr.s_addr = inet_addr(ip.c_str());
                clientAddress.sin_port = htons(port);
                string backstr = string(conn->value(0)) + "#" + string(conn->value(1));
                ssize_t sentBytes = sendto(fd, backstr.c_str(), strlen(backstr.c_str()), 0, reinterpret_cast<struct sockaddr*>(&clientAddress), sizeof(clientAddress));
                std::cout <<sentBytes<< endl;
                if (sentBytes == -1) {
                    std::cerr << "Failed to send response to client" << std::endl;
                }
            }
        }
    }
}

void UDPServer::checkDeviceTimeouts() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        mapMutex.lock();
        auto currentTime = std::chrono::system_clock::now();
        for (auto it = deviceTimestamps.begin(); it != deviceTimestamps.end(); ) {
            auto deviceTime = it->second;
            auto SN = it->first;
            if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - deviceTime).count() > 5) {
                std::cout << "Device " << it->first << " has timed out and will be removed." << std::endl;
                it = deviceTimestamps.erase(it);
                IP_Port_Map.erase(SN);
            }
            else {
                ++it;
            }
        }
        mapMutex.unlock();
    }
}
