TARGET = server
CFLAGS = -g -o
CINCLUDE = -I ./include/
OBJCPPS = sql_connection_pool.cpp http_conn.cpp log.cpp timer_minheap.cpp

server: main.cpp $(OBJCPPS)
	g++ $(CFLAGS) $(TARGET) $(CINCLUDE) $^ -lpthread -lmysqlclient

clean:
	rm  -r server
