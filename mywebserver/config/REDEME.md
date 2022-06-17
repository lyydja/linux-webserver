### Config类
#### 1.成员函数及成员变量
```c++
//该类用来解析main函数的输入，配置相关参数
Config();
~Config() {}
void ParseArg(int argc, char* argv[]); //接收main 函数的输入，将成员变量设置成对应的值
int PORT;           //服务器的端口号
int LOGWRITE;       //日志写入方式
int TRIGMODE;       //触发组合模式
int LISTENTRIGMODE; //listenfd触发模式
int CONNTRIGMODE;   //conn触发模式
int OPTLINGER;      //优雅的关闭连接
int SQLNUM;         //数据库数量
int THREADNUM;      //线程池内的线程数量
int CLOSELOG;       //是否关闭日志
int ACTORMODEL;     //并发模型选择
```

```c++
#### 2.一些参数说明：
/*
	参数：
	“p:l:m:o:s:t:c:a”
        -p：自定义服务器端口号，默认9006。
        -l：选择日志写入方式，默认同步日志。 
            0：同步写入
            1：异步写入
        -m：listenfd和connfd的模式组合，默认使用LT+LT。
            0：表示LT+LT
            1：表示LT+ET
            2：表示ET+LT
            3：表示ET+ET
        -o：优雅关闭连接，默认不使用
            0：不使用
            1：使用
        -s：数据库连接数量，默认为8
        -t：线程数量，默认为8
        -c：关闭日志，默认打开
            0：打开日志
            1：关闭日志
        -a：选择反应堆模型，默认Proactor
            0：Proactor模型
            1：Reactor模型
*/
```


​            



