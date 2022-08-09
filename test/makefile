TARGET = test
CFLAGS = -g -o
CINCLUDE = -I ./include/
OBJCPPS = ../src/buffer.cpp \
		  ../src/http_connection.cpp 

$(TARGET): unitTest.cpp $(OBJCPPS)
	g++ $(CFLAGS) $(TARGET) $(CINCLUDE) $^ 

clean:
	rm  -r test