# linux-webserver（仅用于学习）
* 使用 线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现) 的并发模型
* 使用状态机解析HTTP请求报文，支持解析GET和POST请求
* 访问服务器数据库实现web端用户注册、登录功能，可以请求服务器图片和视频文件
* 利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销
* 实现同步/异步日志系统，记录服务器运行状态

### 使用环境

	1. ubuntu 18.04.6
	2. MYSQL 8.0.29

### 运行前需要做的事情：

```mysql
#1，测试前请确认已经安装MYSQL数据库
#2.创建一个database
	CREATE DATABASE Webserver;
#3.进入Webserver库
	USE Webserver;
#4.创建user表
	CREATE TABLE user(
    	username char(50) NOT NULL,
        passwd char(50) NOt NULL,
        primary key(username)
    )ENGINE=InnoDB;

#添加数据的格式
INSERT INTO user(username, passwd) VALUE('name', 'passwd');

#5.修改main.cpp中的数据库初始信息，根据自己设定的修改
string user = "root";
string passwd = "123456";
string databasename = "Webserver";
```

### 目录树

```bash
├── CGIMysql 					数据库程序	
│   ├── REDEME.md
│   ├── sql_connection_pool.cpp
│   └── sql_conntion_pool.h
├── config						输入解析程序
│   ├── config.cpp
│   ├── config.h
│   └── REDEME.md
├── http						http处理程序
│   ├── http_conn.cpp
│   └── http_conn.h
├── lock						线程锁
│   └── locker.h
├── log							日志程序
│   ├── block_queue.h
│   ├── log.cpp
│   ├── log.h
│   └── README.md
├── main.cpp					主程序
├── makefile
├── README1.md
├── README2.md
├── README3.md
├── root						静态资源
│   ├── fans.html
│   ├── images
│   │   ├── image1.jpg
│   │   ├── test1.jpg
│   │   └── xxx.jpg
│   ├── index.html
│   ├── judge.html
│   ├── logError.html
│   ├── log.html
│   ├── picture.html
│   ├── README.md
│   ├── registerError.html
│   ├── register.html
│   ├── video
│   │   └── xxx.mp4
│   ├── video.html
│   └── welcome.html
├── server						可执行文件
├── test_presure				测试软件
│   └── webbench-1.5
├── threadpool					线程池
│   └── threadpool.h
├── timer						定时器
│   ├── timer.cpp
│   └── timer.h
├── var							日志
└── webserver
    ├── webserver.cpp
    └── webserver.h

```

#### **已经添加的功能：**

#### 1.服务器与客户端之间的正常通信
	1.服务器与客户端之间的连接
	2.服务器可以正确读取客服端发来的请求信息，请求类型为：GET和POST
	3.服务器可以正确的解析客户端发来的请求
	4.服务器可以正确的发送响应信息

#### 2.日志系统
	1.日志文件放在/var目录下
	2.每个日志文件可以通过初始化来确定是同步日志还是异步日志，以及各种属性
	3.根据日志的输出信息的不同，分别放入不同的文件。例如：可以将error和warning信息放入到error文件中，其余的放在另外一个文件中，这样可以很方便的找到错误信息。

#### 3.定时器功能
	1.如果连接一段时间没有请求或响应（非活跃连接），则会关闭它，避免占用线程资源

#### 4.数据库
	1.可以注册账号密码，来登陆
	2.登陆之后进入可以看图片，视频（仅仅是测试）

#### 还需要学习的内容

	1. 定时器时间轮和时间堆的原理
	2. 单例模式和多例模式
	3. webbench的原理

### 致谢
  * Linux高性能服务器编程，游双著.
  * https://github.com/qinguoyi/TinyWebServer
