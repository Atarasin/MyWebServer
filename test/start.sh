cd /home/zh/MyWebServer/test

gcc client.c -o client -pthread

for ((i = 1; i <= 10; i++))
do
    ./client &
    sleep 2
done
