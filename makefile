# Dillon Coffman, Section 1 - Operating Systems Project 3
# CompLab Tutor: Chenglong Fu
myshell:
	gcc -o server server.c open_listenfd.c -pthread

test:
	gcc -o testing server.c open_listenfd.c -pthread -D TESTING
	./testing

testdictionary:
	gcc -o testingdictionary server.c open_listenfd.c -pthread -D TESTINGDICTIONARY
	./testingdictionary

clean: 
	rm -rf server
	rm -rf testing
	rm -rf testingdictionary