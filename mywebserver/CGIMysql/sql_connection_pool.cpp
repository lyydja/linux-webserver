#include "sql_conntion_pool.h"

ConnectionPool::ConnectionPool() {
    m_freeConn = 0;
    m_currConn  = 0;
}

//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL* ConnectionPool::GetConnection() {
    MYSQL* conn = NULL;
    if(0 == connList.size()) {
        return NULL;
    }

    sem.Wait();//减少信号量，即可以连接的数量减1
    lock.Lock();

    conn = connList.front();
    connList.pop_front();

    m_freeConn--;
    m_currConn++;

    lock.Unlock();
    return conn;
}

//释放当前的连接
bool ConnectionPool::ReleaseConnection(MYSQL* conn) {
    if(NULL == conn) {
        return false;
    }

    lock.Lock();
    
    connList.push_back(conn);
    m_freeConn++;
    m_currConn--;

    lock.Unlock();
    sem.Post(); //增加信号量，即可以连接的数量加1

    return true;
}

int ConnectionPool::GetFreeConn() {
    return this->m_freeConn;
}

void ConnectionPool::DestroyPool() {
    lock.Lock();

    //如果有正在连接的，先关闭它们
    if(connList.size() > 0) {
        list<MYSQL*>::iterator it;
        for(it = connList.begin(); it != connList.end(); it++) {
            MYSQL* conn = *it;
            mysql_close(conn);
        }
        m_currConn = 0;
        m_freeConn = 0;
        connList.clear();
    }

    lock.Unlock();
}

//单例模式
ConnectionPool* ConnectionPool::GetInstance() {
    static ConnectionPool connPool;
    return &connPool;
}

//构造初始化
void ConnectionPool::Init(string url, string user, string passwd, string databaseName, int port, int maxConn, int closeLog) {
    m_url = url;
    m_user = user;
    m_passwd = passwd;
    m_databaseName = databaseName;
    m_port = port;
    m_closeLog = closeLog;

    for(int i = 0; i < maxConn; i++) {
        MYSQL* conn = NULL;
        conn = mysql_init(conn);

        if(NULL == conn) {
            LOG_ERROR("%s", "MYSQL Error");
            exit(1);
        }

        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), passwd.c_str(), databaseName.c_str(), port, NULL, 0);

        if(NULL == conn) {
            LOG_ERROR("%s", "MYSQL Error");
            exit(1);
        }
        connList.push_back(conn);//添加到连接池中
        m_freeConn++;
    }

    sem = Sem(m_freeConn);
    m_maxConn = m_freeConn;
}

ConnectionPool::~ConnectionPool() {
    DestroyPool();
}

ConnectionRAII::ConnectionRAII(MYSQL** SQL, ConnectionPool* connPool) {
    *SQL = connPool->GetConnection();
    
    connRAII = *SQL;
    poolRAII = connPool;

}

ConnectionRAII::~ConnectionRAII() {
    poolRAII->ReleaseConnection(connRAII);
}