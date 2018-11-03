#include "server.h"

//An extremely simple server that connects to a given port.
//Once the server is connected to the port, it will listen on that port
//for a user connection.
//A user will connect through telnet, the user also needs to know the port number.
//If the user connects successfully, a socket descriptor will be created.
//The socket descriptor works exactly like a file descriptor, allowing us to read/write on it.
//The server will then use the socket descriptor to communicate with the user, sending and 
//receiving messages.

pthread_t threadPool[NUM_WORKER_THREADS], logThread; // declare global thread pool to use as worker threads
//pthread_t logThread; // declare global log thread to be used
pthread_mutex_t job_mutex, log_mutex;
pthread_cond_t job_cv_cs, job_cv_pd;
pthread_cond_t log_cv_cs, log_cv_pd;
int jobBuff[JOB_BUF_LEN]; // array of ints that represent the socket for a client trying to connect
char logBuff[LOG_BUF_LEN][MAX_WORD_SIZE]; // array of phrases to be written to log file
int jobLen = JOB_BUF_LEN; // capacity of job buffer
int logLen = LOG_BUF_LEN; // capacity of log buffer
int jobCount, logCount = 0; // number of items in each buffer
int jobFront = -1;
int jobRear = 0;
int logFront = -1;
int logRear = 0;

int connectionPort = 0; // declare global connectionPort to be used
char* dictionaryName = ""; // declare global dictionaryName to be used
char dictionary[DICTIONARY_SIZE][MAX_WORD_SIZE]; // dictionary to store words from dictionaryName file in
int wordsInDictionary = 0; // global var to keep track of word count of dictionary - to be used when searching
struct sockaddr_in client;
int clientLen = sizeof(client);
int connectionSocket, clientSocket, bytesReturned, socketDesc;
//char recvBuffer[MAX_WORD_SIZE];
char* clientMessage = "Hello! You're connected to the server. Send the server a word to spell check!\n";
char* msgRequest = "Send me another word to spell check!\n";
char* msgResponse = "I actually don't have anything interesting to say...but I know you sent ";
char* msgPrompt = ">>>";
char* msgError = "I didn't get your message. ):\n";
char* msgClose = "Goodbye!\n";




int main(int argc, char** argv) {
	//recvBuffer[0] = '\0';

	if (pthread_cond_init(&job_cv_cs, NULL) != 0) { // check that job buffer consume condition variable initialized
		printf("Error initializing job buffer consume condition variable!\n");
		return -1;
	}
	if (pthread_cond_init(&job_cv_pd, NULL) != 0) { // check that job buffer produce condition variable initialized
		printf("Error initializing job buffer produce condition variable!\n");
		return -1;
	}
	if (pthread_cond_init(&log_cv_cs, NULL) != 0) { // check that log buffer consume condition variable initialized
		printf("Error initializing log buffer consume condition variable!\n");
		return -1;
	}
	if (pthread_cond_init(&log_cv_pd, NULL) != 0) { // check that log buffer produce condition variable initialized
		printf("Error initializing log buffer produce condition variable!\n");
		return -1;
	}
	

	// Create threads - worker threads and log thread
	for (int i = 0; i < NUM_WORKER_THREADS; i++) { // create worker threads
		pthread_create(&threadPool[i], NULL, workerThreadFunc, NULL);
						//pointer to pthread_t struct, attributes set to NULL, function thread will begin executing, arguments you want to pass into the function
	}
	pthread_create(&logThread, NULL, logThreadFunc, NULL); // create log thread to write phrases from log buffer to log file

	// If no port or dictionaryName is specified by user, use the default port and default dictionaryName
	if (argc == 1){
		printf("No port number entered..using default port 7000!\n");
		// use DEFAULT_PORT and DEFAULT_DICTIONARY
		connectionPort = DEFAULT_PORT;
		dictionaryName = DEFAULT_DICTIONARY;
		#ifdef TESTING
		printf("THE DICTIONARY FILE IS: %s\n", dictionaryName); // FOR TESTING
		#endif
	} else if (argc == 2) { // check whether second argument is a port number or a dictionaryName file
		#ifdef TESTING
		printf("You entered 2 arguments!\n"); // FOR TESTING
		#endif
		// If it is just a port number, be sure to make dictionaryName the DEFAULT_DICTIONARY
		if (strstr(argv[1], ".txt") == NULL){ // if the second arg does not contain .txt, it's safe to assume it's a port number
			connectionPort = atoi(argv[1]); // set connectionPort to second arg
			dictionaryName = DEFAULT_DICTIONARY; // set dictionaryName to DEFAULT_DICTIONARY since no dictionaryName was specified
			#ifdef TESTING
			printf("Assigned dictionaryName to DEFAULT_DICTIONARY!\n"); // FOR TESTING
			#endif
		} 
		// If it is just a dictionaryName, be sure to make connectionPort the DEFAULT_PORT
		if (strstr(argv[1], ".txt") != NULL) { // if the second arg contains .txt, it's safe to assume it's a dictionaryName file
			connectionPort = DEFAULT_PORT; // set connectionPort to DEFAULT_PORT since no port was specified
			dictionaryName = argv[1]; // set dictionaryName to user-specified dictionaryName file name
			#ifdef TESTING
			printf("Assigned connectionPort to DEFAULT_PORT!\n"); // FOR TESTING
			#endif
		}
	} else if (argc == 3) { // check whether second arg is a port number and third arg is a dictionaryName OR second arg is a dictionaryName and third arg is a port number
		#ifdef TESTING
		printf("You entered 3 arguments!\n"); // FOR TESTING
		#endif
		// If the second arg is a port number AND the third arg is a dictionaryName file, assign those args to the connectionPort and dictionaryName global variables respectively
		if ((strstr(argv[1], ".txt") == NULL) && (strstr(argv[2], ".txt") != NULL)) {
			#ifdef TESTING
			printf("Second arg was: %s\n", argv[1]); // FOR TESTING
			printf("Third arg was: %s\n", argv[2]); // FOR TESTING
			#endif
			connectionPort = atoi(argv[1]); // set connectionPort to user-specified port
			dictionaryName = argv[2]; // set dictionaryName to user-specified dictionaryName
		// Else-If the second arg is a dictionaryName AND the third arg is a port number, assign those args to the dictionaryName and connectionPort global variables respectively
		} else if ((strstr(argv[1], ".txt") != NULL) && (strstr(argv[2], ".txt") == NULL)) {
			#ifdef TESTING
			printf("Second arg was: %s\n", argv[1]); // FOR TESTING
			printf("Third arg was: %s\n", argv[2]); // FOR TESTING
			#endif
			dictionaryName = argv[1]; // set dictionaryName to user-specified dictionaryName
			connectionPort = atoi(argv[2]); // set connectionPort to user-specified port
		} else {
			printf("Please enter an appropriate command.\nFor example: './server', './server PORTNUMBER', './server DICTIONARYFILE', './server PORTNUMBER DICTIONARYFILE', './server DICTIONARYFILE PORTNUMBER'\n");
		return -1;
		}
	} else { // otherwise too many arguments were entered, print error message asking user to enter proper number of args and return -1
		printf("Please enter an appropriate command.\nFor example: './server', './server PORTNUMBER', './server DICTIONARYFILE', './server PORTNUMBER DICTIONARYFILE', './server DICTIONARYFILE PORTNUMBER'\n");
		return -1;
	}

	// Attempt to open dictionaryName file to load words into dictionary
	FILE* dictionaryName_ptr = fopen(dictionaryName, "r"); // open dictionary file for reading
	if (dictionaryName_ptr == NULL) { // if dictionaryName_ptr is NULL, there was an issue opening the file
		printf("Error opening dictionary file!\n"); // print an error
		return -1; // return -1 to exit program
	} else { // otherwise dictionary file was opened for reading successfully
		// Store words from dictionary file into dictionary[][]
		int i = 0;
		while((fgets(dictionary[i], sizeof(dictionary[i]), dictionaryName_ptr) != NULL) && (i < (DICTIONARY_SIZE - 1))) {  // store the word from the dictionary into dictionary data structure (two-dimensional char array in our case) fgets() also takes care of appending '\0' to the end of the word :) - using a while with fgets() like this ensures we don't read past the end of the statically allocated dictionary[][]
			//strtok(dictionary[i], "\n"); // take out newline for each word in dictionary file - MIGHT NEED \n IN SEARCHING PART TO SEE WHERE WORD ENDS
			wordsInDictionary++;
			#ifdef TESTING
			printf("WORD: %s", dictionary[i]); // print each word FOR TESTING
			#endif
			i++;
		}
		#ifdef TESTING
		printf("Word count of words in the dictionary: %d\n", wordsInDictionary); // FOR TESTING
		#endif
		fclose(dictionaryName_ptr); // close dictionary
	}

	//We can't use ports below 1024 and ports above 65535 don't exist.
	if (connectionPort < 1024 || connectionPort > 65535){
		printf("Port number is either too low(below 1024), or too high(above 65535).\n");
		return -1;
	}
	printf("Waiting to make a connection.. :)\n");

	//Does all the hard work for us.
	connectionSocket = open_listenfd(connectionPort);
	if (connectionSocket == -1){
		printf("Could not connect to %s, maybe try another port number?\n", argv[1]);
		return -1;
	}

	
	
		//accept() waits until a user connects to the server, writing information about that server
		//into the sockaddr_in client.
		//If the connection is successful, we obtain A SECOND socket descriptor. 
		//There are two socket descriptors being used now:
		//One by the server to listen for incoming connections.
		//The second that was just created that will be used to communicate with 
		//the connected user.
	while(1) { // continously accepts connections to process once currently connected client quits so long as a worker thread is available for servicing
		if ((clientSocket = accept(connectionSocket, (struct sockaddr*)&client, &clientLen)) == -1){
			printf("Error connecting to client.\n");
			return -1;
		}
	//	printf("Client socket: %d\n", clientSocket); // FOR TESTING

		// Add clientSocket to Job Buffer THIS SHOULD BE IN SIDE MUTEX LOCK WITH CONDITION VARIABLES
		pthread_mutex_lock(&job_mutex); // lock mutex for job buffer
		while(jobCount == JOB_BUF_LEN) {// while queue is full
			pthread_cond_wait(&job_cv_pd, &job_mutex); // wait for job buffer to not be full
		}
		insertConnection(clientSocket); // insert socketConnection into jobBuffer
		pthread_mutex_unlock(&job_mutex); // unlock mutex
		pthread_cond_signal(&job_cv_cs); // signal client buffer NOT EMPTY
		printf("Connection success!\n");
		send(socketDesc, clientMessage, strlen(clientMessage), 0);



		/**** Nothing below this ****/


		
		// char* clientMessage = "Hello! I hope you can see this.\n";
		// char* msgRequest = "Send me a word to spell check!\n";
		// char* msgResponse = "I actually don't have anything interesting to say...but I know you sent ";
		// char* msgPrompt = ">>>";
		// char* msgError = "I didn't get your message. ):\n";
		// char* msgClose = "Goodbye!\n";

		//send()...sends a message.
		//We specify the socket we want to send, the message and it's length, the 
		//last parameter are flags.
		// send(clientSocket, clientMessage, strlen(clientMessage), 0);
		// send(clientSocket, msgRequest, strlen(msgRequest), 0);


		//Begin sending and receiving messages.
		// while(1){
		// 	send(clientSocket, msgPrompt, strlen(msgPrompt), 0);
		// 	//recv() will store the message from the user in the buffer, returning
		// 	//how many bytes we received.
		// 	bytesReturned = recv(clientSocket, recvBuffer, MAX_WORD_SIZE, 0);

		// 	//Check if we got a message, send a message back or quit if the
		// 	//user specified it.
		// 	if(bytesReturned == -1){
		// 		send(clientSocket, msgError, strlen(msgError), 0);
		// 	}
		// 	//'27' is the escape key.
		// 	else if(recvBuffer[0] == 27){
		// 		send(clientSocket, msgClose, strlen(msgClose), 0);
		// 		close(clientSocket);
		// 		break;
		// 	}
		// 	else{
		// 		send(clientSocket, msgResponse, strlen(msgResponse), 0); // sends default message response to be followed by what the user entered 
		// 		send(clientSocket, recvBuffer, bytesReturned, 0); // sends what was received from user back to client
		// 	}
		// }
	}
	return 0;
}

void* workerThreadFunc(void* arg) {
	/***** TAKE SOCKET OUT *****/
	pthread_mutex_lock(&job_mutex); // lock mutex for client buffer	
	while(jobCount == 0) {// while loop to check if size of client buffer is empty, if it is 0
		pthread_cond_wait(&job_cv_cs, &job_mutex); // have the consumer wait
		// ?
	}
	#ifdef TESTING
	printf("job count of job buffer BEFORE removing: %d\n", jobCount); // FOR TESTING
	#endif
	socketDesc = removeConnectionSocketDesc(); // then take socket descriptor out of client buffer - store it in a local int here called socketDescriptor
	// clientTail tracks index of next one to move - change clientTail so next time we go to take out, we know what index to take out of - don't just do tail++ cause we'll run out of space, use modulo (%) to not run out (clientTail + 1) % capacity of buffer JOB_BUF_LENGTH -> REMOVE FUNCTION TAKES CARE OF THIS
	#ifdef TESTING
	printf("job count of job buffer AFTER removing: %d\n", jobCount); // FOR TESTING
	printf("Inside CS for workerThreadFunc!\n"); // FOR TESTING
	#endif
	pthread_mutex_unlock(&job_mutex); // unlock job_mutex
	pthread_cond_signal(&job_cv_pd); // pthread_cond_signal client buffer not full because we just took something out

	/**** RECEIVE A WORD TO USE FOR SPELL-CHECKING ****/
	while (1) { // while(1) { // TEST putting mutual exclusion above inside HERE
		char* word = calloc(MAX_WORD_SIZE, 1); // allocate memory for word you're going to receive from client with calloc(max size you'll accept, 1);
		while(recv(socketDesc, word, MAX_WORD_SIZE, 0)) { //- takes four args, can then assume word from client has been received
			
			if (strlen(word) <= 1) { // if nothing was entered, continue
				continue;
			}

			// printf("\nRECEIVED WORD FROM USER! word received: %s\n", word); // FOR TESTING
			// if (word == 27) { // if escape entered, exit this thread
			// 	printf("Escape was entered! Exiting client..\n");
			// 	close(socketDesc);
			// 	pthread_exit(NULL);
			// }
			
			//strcat(word, '\0');
			// Check to see if the word you allocated memory for with calloc is in dictionary, add the CORRECTNESS to end of word with realloc
			// Binary Search a word //
			if (searchForWordInDict(dictionary, word)) { // if word was found in dictionary
				strtok(word, "\n"); // take newline out of word for readability
				strcat(word, " OK\n"); // concatenate OK onto end of the word
				#ifdef TESTING
				printf("word WAS found!!!\n"); // FOR TESTING
				#endif
			} else { // word was not found :(
				strtok(word, "\n"); // take newline out of word for readability
				strcat(word, " WRONG\n"); // concatenate WRONG onto end of the word
				#ifdef TESTING
				printf("word was NOT found!\n"); // FOR TESTING
				#endif
			}
			//send(socketDesc, "You entered: ", strlen("You entered: "), 0);
			write(socketDesc, word, strlen(word)); // write a message with write() or send() to client with word plus correctness
			write(socketDesc, msgRequest, strlen(msgRequest));
			// Write phrase to log buffer using mutual exclusion for log buffer
			
			free(word); // free old word used
			word = calloc(MAX_WORD_SIZE, 1); // calloc memory for new word to use
		}
		// print message to server that connection has been closed
		close(socketDesc); // close socketDesc
	}
}

void* logThreadFunc(void* arg) {
	// take phrase out of log buffer with mutual exclusion
	pthread_mutex_lock(&log_mutex);
	while(logCount == 0) {
		pthread_cond_wait(&log_cv_cs, &log_mutex);
	}
	while(logCount != 0) { // write phrases to log file until log buffer is empty - put this inside the mutual exclusion
		// make sure function that removes from buffer updates front, rear, and the logCount for items
		// write phrases to log file
	}
	pthread_mutex_unlock(&log_mutex);
	pthread_cond_signal(&log_cv_pd); // signal log buffer not full since we took 
}

void insertConnection(int connectionSocketDescriptor) { // FIX THIS
	if (jobCount == JOB_BUF_LEN) { // if the job buffer is full
		printf("Job buffer is FULL!\n"); // print that it's full
	} else {
		if (jobCount == 0) {// if the job buffer is empty, make the item the first in the queue, update front and rear
			jobFront = 0;
			jobRear = 0;
		} 
		jobBuff[jobRear] = connectionSocketDescriptor; // add the connectionSocketDescriptor to the job buffer
		jobCount++; // increment count of socket descriptors in the job buffer
		printf("socket descriptor inserted into circular array: %d\n", jobBuff[jobRear]); // FOR TESTING
		jobRear = (jobRear + 1) % JOB_BUF_LEN; // reset jobRear when it reaches the 100th index (this makes it circular)
	}
}

int removeConnectionSocketDesc() {
	if (jobCount == 0) { // if the job buffer is empty
		printf("Job buffer is empty! Can't remove anything!\n"); // print a message
	} else { // otherwise remove a connectionSocketDescriptor from the job buffer
		int socketDesc = jobBuff[jobFront]; // store socket descriptor to remove in socketDesc
		jobFront = (jobFront + 1) % JOB_BUF_LEN; // reset jobFront when it reaches 100th index (this makes it circular)
		jobCount--;
		return socketDesc;
	}
}

int searchForWordInDict(char list_of_words[][MAX_WORD_SIZE], char* wordToFind){
	int i = 0;
	while(i < wordsInDictionary-1) {
		if (strcmp(list_of_words[i], wordToFind) == 0) {
			printf("WORD FOUND!\n");
			return 1;
		}
		i++;
	}
	printf("Word NOT found!\n");
	return 0;
}