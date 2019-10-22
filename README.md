# Networked-Spell-Checker
Using the socket.h and pthread.h C libraries (among others), this project starts a server which uses multithreading and synchronization to connect multiple clients at once (up to 3) who can send words to the server to be spell-checked. The correctness of the word is then logged in the log file.

Program Overview:
	My Networked Spell Checker program starts a server which users (clients) can connect to (using clients like telnet or netcat) and send words to one at a time. The server will then search a dictionary for that word and respond to the client if that word is spelled correctly or not. As it responds to the client, it also writes the phrase (the word + the correctness of the word) to a log file for the users (clients) to view all the words they sent and whether or not they were correct. Essentially the program centers around key topics including sockets, multithreading, concurrency, and synchronization. To reiterate - the program (server) accepts words from clients, searches if the word they entered was correct, sends a message back to the client with the word and whether or not it was correct and logs that phrase to a log file. 

Program Design:
	When designing any program, it’s important to break it down into parts so that things remain easy to understand conceptually and thus easier to code. A slide from the project notes really sums up exactly what my program does:  

With this diagram, one can easily begin to divide up the networked spell checker program to code and test. The first portion is the socket programming in order to allow for multiple clients to connect to a server running on one port. To accomplish this, I used Chapter 11 in Bryant O’Halloran as well as Chenglong’s provided simple server implementation we were free to use. To dive into what exactly every part of the socket programming code does would take a whole chapter for me to explain so let’s split it into the basics. First, a socket descriptor should be created using the socket() function, then bind is called to associate a port number with that created socket descriptor. Next, the socket is prepared to allow accept() calls in main using the listen(socket) function to listen for connections on that socket. After all of that is done, in an infinite while-loop my program calls accept() on a connectionSocket which is the socket descriptor for our server, this function returns a socket descriptor for the clientSocket. So now we have two socket descriptors, one used to listen for incoming connections and one to communicate with the connected client.
	That takes us up to the ‘Job Queue” as listed in the diagram above. We see that multiple clients should be able to insert their socket descriptor into this job buffer at once. I made both my job buffer and my log buffer circular arrays to easily insert and remove from. Multiple clients imply we need multithreading because we should be able to service multiple clients at the same time on the server. Thus we see the need to create a thread pool of worker threads who will take care of SAFELY (using mutual exclusion and condition variables) removing a client socket descriptor from the job buffer, servicing that client (checking if the word is correct or not), and then adding the word and its correctness to the log buffer. The job buffer along with the log buffer are shared resources in our program. Because of this, we need to implement synchronization to ensure no data corruption takes place and that our program can run smoothly. Basically this becomes an instance of the Producer/Consumer problem where we need to use mutual exclusion and condition variables when inserting/removing from a buffer to ensure no data is lost or corrupted. So we need one mutex for each buffer, and two condition variables for each buffer. My mutexes for my program are thus initialized at the beginning of main thread, and the thread pool is created at the beginning of the main thread.
	After the word and its correctness are inserted into the log buffer, another thread has to step in to handle safely removing it from the log buffer and writing that word and its correctness (which I call a phrase), to the log file. To do this, I created a logThread at the beginning of the main thread just below where I created the worker threads. This log thread is responsible for safely removing the phrase from the log buffer using mutual exclusion and condition variables, and then appending that phrase to the log file.

To recap – my program is split into three main parts: the main (server) thread, the worker threads, and the log thread. The main thread is responsible for initializing a mutex and condition variables for each buffer, creating the thread pool of 3 worker threads and a log thread, processing the user’s arguments, loading the dictionary file into memory to use, getting a connection with a client and inserting that socket descriptor describing that connection into the job buffer using the job buffer mutex and the job buffer condition variables. The worker threads when created call their start routines which I made a thread function for called workerThreadFunc(). The worker thread function all in an infinite loop, safely removes the client socket descriptor to be serviced from the job buffer using mutual exclusion and condition variables to signal, services that client’s socket descriptor by receiving words from the user until the user enters escape or exits the connection with ctrl+c, (servicing entails receiving that word from the client, searching the previously loaded in dictionary for that word, and concatenating if that word is correct or not onto the end of it, writing a message back to the client if the word was correct or not, and safely inserting that phrase into the log buffer, then signaling to that the log buffer is no longer empty since we just inserted into it). The log thread when created calls its start routine which is a thread function I made, creatively called logThreadFunc(). This log thread function sits in an infinite loop that safely removes a phrase from the log buffer using mutual exclusion and condtion variables, and appends that phrase to the log file after opening it, closes the log file, releases the mutex, and signals that the log buffer is not empty since we would have just safely removed from it.



TESTING IS BELOW HERE


Testing: (‘make test’)
To test my program and in doing so, demonstrate it works correctly, I divided my testing up like so and provided screenshots of both execution and code for that specific part: 
•	Main Thread
o	Show ability to process various arguments (keep in mind my DEFAULT_DICTIONARY is “words.txt” and DEFAULT_PORT is 7000 and “dict.txt” is a fake dictionary I made up for testing)
	No arguments? Use default port (7000) and default dictionary “words.txt”  
	Only user port specified? Use default dictionary  
	Only user dictionary specified? Use default port 
	Dictionary and port user-specified? Use their specified port and dictionary, keep in mind the port number and dictionary can be switched 
 
o	







o	SEE PAGE BELOW







o	Show dictionary file loaded in (‘make testdictionary’) 
	Hard to show through screenshots but will show in demo, ‘make testdictionary’ prints every word read into the dictionary array and the word count of the dictionary used. I tried to include a video for this but it didn’t work, this is something best shown in the demo.





SEE PAGE BELOW for code on loading dictionary file in


 

o	Show threads created successfully with if-blocks (pthread_create() returns 0 on success)
	Simple print statement printed at the start – we know they’re created because this statement only prints if pthread_create() returns 0 which means success
o	 
 

o	Show connection success after inserting client socket descriptor into job buffer
	Print connection success every time a connection is made, note that more connections can be made than actually serviced since there are only 3 worker threads to service 3 clients. It’s not until one quits (assuming all three connections made) that a 4th connection could be serviced. Will show this in demo more effectively, harder to show through screenshots.
I tried to include a video for this but it didn’t work, this is something best shown in the demo.
 

•	Worker Threads
o	Show removing from job buffer is successful
	Print job buffer count after removing  
 
o	Show receiving word from a client is successful
	Print the word on server side
 

o	Show searching dictionary for word is successful for word spelled correctly and incorrectly
	Print ‘WORD FOUND’ or ‘WORD NOT FOUND’ (see above screenshot for previous bullet) I only included my function code here for finding the word, but see my code inside workThreadFunc for where I call it
 
o	Show inserting phrase (word + correctness) into log buffer is successful (see above screenshot for second previous bullet) 








•	Log Thread
o	Show taking phrase out of log buffer is successful
	Show that the phrase is the same as the one that was inserted 
	SEE RIGHT BELOW HERE FOR SCREENSHOT OF EXECUTION AND CODE 
 
o	Show appending phrase to log file is successful
	Show log file after entering words on client, this will be easier to show in demo than with screenshots (SEE ABOVE FOR CODE THOUGH)
(Note that some tests required the commenting out of some parts of code just to demonstrate they work and may not actually show up when doing ‘make test’ which runs the program with general testing statements, for example inserting into the buffers, it’s important to see the count actually increases, but when the program actually runs, the count fluctuates because items are inserted/removed concurrently with the threads. The log count almost always remains 0 because a phrase removed right away by the log thread and serviced as they are inserted into the log buffer to avoid problems with the buffer overflowing as stated in the project slides)
