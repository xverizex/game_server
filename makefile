all:
	gcc   ./main.c ./config.c ./db.c ./server.c ./ssl.c -I/usr/include/mysql -lmysqlclient -lssl -lcrypto -pthread -o broken_heads
install:
	install broken_heads /usr/local/bin
	install broken_heads.conf /usr/local/etc
clean:
	rm broken_heads
uninstall:
	rm /usr/local/bin/broken_heads
