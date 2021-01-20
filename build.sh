gcc -DWITH_WEBSOCKET main.c -L lib -L/usr/local/lib -lhttpsvr -lpthread -levent -levent_openssl -lcrypt -lssl -o app
# cat package.bin >> app
