# linux-webserver（仅用于学习）
* 使用 线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现) 的并发模型
* 使用状态机解析HTTP请求报文，支持解析GET和POST请求
* 访问服务器数据库实现web端用户注册、登录功能，可以请求服务器图片和视频文件
* 利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销
* 实现同步/异步日志系统，记录服务器运行状态

### 致谢
Linux高性能服务器编程，游双著.
https://github.com/qinguoyi/TinyWebServer
