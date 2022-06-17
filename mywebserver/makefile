target=server
libs=main.cpp ./http/http_conn.cpp ./log/log.cpp ./webserver/webserver.cpp ./timer/timer.cpp ./CGIMysql/sql_connection_pool.cpp ./config/config.cpp


$(target):$(libs)
	g++ $(libs) -std=c++11 -I/usr/include/mysql -L/usr/lib/mysql -o $(target) -pthread -lmysqlclient 
clean:
	rm -r $(target)