#include<iostream>
#include"UDPServer.h"
#include"Logger.h"
using namespace std;

int main() {
    //初始化日志库
    string filename = "";
    time_t NOW = time(NULL);
    struct tm* t = std::localtime(&NOW);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S.txt", t);
    filename = buffer;
     // 初始化日志对象
    Logger::instance()->open(filename);
    debug("程序开始执行，日志正常工作");
    UDPServer server(6666);
    server.Start();
    UDPServer::mysql_down_command();
    //std::this_thread::sleep_for(std::chrono::seconds(6000));
    server.Stop();

	return 0;
}