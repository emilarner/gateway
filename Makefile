CC=g++
CPPFLAGS=-g

gateway-master: gateway-master.o gateway-relay.o ip-database.o main.o
	$(CC) $(CPPFLAGS) -o gateway-master gateway-master.o gateway-relay.o ip-database.o main.o

gateway-master.o: gateway-master.cpp gateway-master.hpp
	$(CC) $(CPPFLAGS) -c gateway-master.cpp

gateway-relay.o: gateway-relay.cpp gateway-relay.hpp
	$(CC) $(CPPFLAGS) -c gateway-relay.cpp

ip-database.o: ip-database.cpp ip-database.hpp
	$(CC) $(CPPFLAGS) -c ip-database.cpp

main.o: main.cpp
	$(CC) $(CPPFLAGS) -c main.cpp

install:
	sudo pip install -r requirements.txt
	cp gateway-master /usr/bin/
	cp launcher.py /usr/bin/gateway-launcher
	cp defaults/* /var/
	chmod 0755 /usr/bin/gateway-launcher
	chmod 0755 /usr/bin/gateway-master


clean:
	rm *.o
	rm gateway-master

