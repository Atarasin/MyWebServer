TARGET = server
CFLAGS = -g -o
CINCLUDE = -I ./include/
OBJCPPS = sql_connection_pool.cpp http_connection.cpp log.cpp timer_minheap.cpp webserver.cpp \
		  epoller.cpp buffer.cpp

$(TARGET): main.cpp $(OBJCPPS)
	g++ $(CFLAGS) $(TARGET) $(CINCLUDE) $^ -lpthread -lmysqlclient

clean:
	rm  -r server
