# Dillon Coffman, Section 1 - Operating Systems Project 3
# CompLab Tutor: Chenglong Fu
myshell:
	gcc -o server server.c open_listenfd.c -pthread

test:
	gcc -o testing server.c open_listenfd.c -pthread -D TESTING
	./testing

clean: 
	rm -rf server
	rm -rf testing