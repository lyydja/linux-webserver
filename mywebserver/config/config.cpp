#include "config.h"



Config::Config() {
    PORT = 9006;
    LOGWRITE = 0;
    TRIGMODE = 0;
    LISTENTRIGMODE = 0;
    CONNTRIGMODE = 0;
    OPTLINGER = 0;
    SQLNUM = 8;
    THREADNUM = 8;
    CLOSELOG = 0;
    ACTORMODEL = 0;
}

/* 接收main 函数的输入，将成员变量设置成对应的值,
如果用户调用 main 函数时有输入指定参数，使用 getopt 获取并修改对应值 */
void Config::ParseArg(int argc, char* argv[]) {
    int opt;
    const char* str = "p:l:m:o:s:t:c:a:";
    while((opt = getopt(argc, argv, str)) != -1) {
        switch (opt)
        {
        case 'p':
            PORT = atoi(optarg);
            break;
        case 'l':
            LOGWRITE = atoi(optarg);
            break;
        case 'm':
            TRIGMODE = atoi(optarg);
            break;
        case 'o':
            OPTLINGER = atoi(optarg);
            break;
        case 's':
            SQLNUM = atoi(optarg);
            break;
        case 't':
            THREADNUM = atoi(optarg);
            break;
        case 'c':
            CLOSELOG = atoi(optarg);
            break;
        case 'a':
            ACTORMODEL = atoi(optarg);
            break; 
        default:
            break;
        }
    }
   
}