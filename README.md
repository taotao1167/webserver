# webserver
a tiny web server based on libevent

# Compile
```bash
# 带ssl的静态库
cmake .. -DEVENT__DISABLE_OPENSSL=OFF -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_SAMPLES=ON -DEVENT__LIBRARY_TYPE=STATIC -DCMAKE_INSTALL_PREFIX=/home/www/libevent_ins
# 使用
cmake -DLibevent_DIR=/home/www/libevent_ins/lib/cmake/libevent ..
```
