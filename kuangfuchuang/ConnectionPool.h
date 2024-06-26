#ifndef ConnectionPool_H_
#define ConnectionPool_H_
#include<queue>
#include<string>
#include<mutex>
#include<condition_variable>
#include"MysqlConn.h"
#include<memory>
using namespace std;

class ConnectionPool
{
public:
	//ͨ����̬������ȡ������Ķ���
	static ConnectionPool* getConnectionPool();
	ConnectionPool(const ConnectionPool& obj) = delete;
	ConnectionPool& operator = (const ConnectionPool& obj) = delete;
	//��ȡ���ݿ�����
	shared_ptr<MysqlConn> getConnection();
	void close();
private:
	~ConnectionPool();
	ConnectionPool();
	//������ӵĺ���
	void addConnection();
	//�����������ݿ����ӵ�������
	void produceConnection();
	//�����������ݿ����ӵ�������
	void recycleConnection();


	string m_ip = "127.0.0.1";
	string m_user = "root";  
	string m_passwd = "Hckj201509";
	string m_dbName = "share1";
	/*string m_ip = "127.0.0.1";
	string m_user = "root";
	string m_passwd = "563128";
	string m_dbName = "share1";*/
	unsigned short m_port = 3306;
	int m_minSize = 100;							//�����������ж��ٸ�����
	int m_maxSize = 800;							//����������ж��ٸ�����
	int m_timeout = 1000;							//��������û������ʱ���������ó�ʱʱ��
	int m_maxIdleTime = 10800000;					//3��Сʱ
	//int m_maxIdleTime = 1000;	//����������ʱ�����ﵽ������ʱ���ر�����
	queue<MysqlConn*> m_connectionQ;		//�����Ч���ӵĶ���
	mutex m_mutexQ;
	condition_variable m_cond;
	bool ISClose = true;

	static int busynum;
};

#endif



