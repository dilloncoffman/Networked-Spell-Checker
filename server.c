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
pthread_mutex_t job_mutex, log_mutex;
pthread_cond_t job_cv_cs, job_cv_pd;
pthread_cond_t log_cv_cs, log_cv_pd;
int jobBuff[JOB_BUF_LEN]; // array of ints that represent the socket for a client trying to connect
char logBuff[LOG_BUF_LEN][PHRASE_SIZE]; // array of phrases to be written to log file
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
char* msgRequest = "Send me another word to spell check! Or, enter the escape key and hit enter to quit this connection..\n";
char* msgError = "I didn't get your message. ):\n";
char* msgClose = "Goodbye!\n";

FILE* logFile_ptr;




int main(int argc, char** argv) {
	// Initialize mutexes
	// if(pthread_mutex_init(&job_mutex, NULL) != 0) {
	// 	printf("Error initializing job mutex!\n");
	// 	return -1;
	// }
	// if(pthread_mutex_init(&log_mutex, NULL) != 0) {
	// 	printf("Error initializing log mutex!\n");
	// 	return -1;
	// }
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

	// use open_listenfd (from Ch. 11 O'Halloran) to get socket descriptor to listen for incoming connections
	connectionSocket = open_listenfd(connectionPort);
	if (connectionSocket == -1){ // if connectionSocket is -1
		printf("Could not connect to %s, maybe try another port number?\n", argv[1]); // print an error
		return -1; // return -1 to end program
	}

	while(1) { // continously accepts connections to process once currently connected client quits so long as a worker thread is available for servicing
		//accept() waits until a user connects to the server, writing information about that server into the sockaddr_in client.
		//If the connection is successful, we obtain A SECOND socket descriptor. 
		//There are two socket descriptors being used now:
		//One by the server to listen for incoming connections.
		//The second that was just created that will be used to communicate with 
		//the connected user.
		if ((clientSocket = accept(connectionSocket, (struct sockaddr*)&client, &clientLen)) == -1){
			printf("Error connecting to client.\n");
			return -1;
		}
	//	printf("Client socket: %d\n", clientSocket); // FOR TESTING

		// Add clientSocket to job Buffer
		pthread_mutex_lock(&job_mutex); // lock mutex for job buffer
		while(jobCount == JOB_BUF_LEN) {// while job buffer is full
			pthread_cond_wait(&job_cv_pd, &job_mutex); // wait for job buffer to not be full
		}
		insertConnection(clientSocket); // insert socketConnection into jobBuffer
		pthread_mutex_unlock(&job_mutex); // unlock mutex
		pthread_cond_signal(&job_cv_cs); // signal client buffer NOT EMPTY
		printf("Connection success!\n");
		send(clientSocket, clientMessage, strlen(clientMessage), 0);
	}
	return 0;
}

void* workerThreadFunc(void* arg) {
	while (1) {
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
		#ifdef TESTING
		printf("job count of job buffer AFTER removing: %d\n", jobCount); // FOR TESTING
		printf("Inside CS for workerThreadFunc!\n"); // FOR TESTING
		#endif
		pthread_mutex_unlock(&job_mutex); // unlock job_mutex
		pthread_cond_signal(&job_cv_pd); // pthread_cond_signal client buffer not full because we just took something out


		/**** RECEIVE A WORD TO USE FOR SPELL-CHECKING ****/
		char* word = calloc(MAX_WORD_SIZE, 1); // allocate memory for word you're going to receive from client with calloc(max size you'll accept, 1);
		while(recv(socketDesc, word, MAX_WORD_SIZE, 0)) { //- takes four args, can then assume word from client has been received
			
			if (strlen(word) <= 1) { // if nothing was entered, continue
				continue;
			}

			printf("\nRECEIVED WORD FROM USER! word received: %s\n", word); // FOR TESTING

			// EXIT CLIENT IF ESCAPE ENTERED
			if (word[0] == 27) { // if escape entered, exit this thread
				printf("EEscape was entered! Exiting a client..\n");  // print message
				write(socketDesc, msgClose, strlen(msgClose)); // write closing message to client
				close(socketDesc); // close client socket
				break;
			}
			
			//strcat(word, '\0');
			// Check to see if the word you allocated memory for with calloc is in dictionary, add the CORRECTNESS to end of word with realloc
			// Binary Search a word //
			if (searchForWordInDict(dictionary, word)) { // if word was found in dictionary
				strtok(word, "\n"); // take newline out of word for readability
				word = realloc(word, sizeof(char*)*PHRASE_SIZE); // realloc space for word to fit in 'correctness' concatenated on end, realloc takes care of freeing old memory for word
				strcat(word, " OK\n"); // concatenate OK onto end of the word
				#ifdef TESTING
				printf("word WAS found!!!\n"); // FOR TESTING
				#endif
			} else { // word was not found :(
				strtok(word, "\n"); // take newline out of word for readability
				word = realloc(word, sizeof(char*)*PHRASE_SIZE); // realloc space for word to fit in 'correctness' concatenated on end, realloc takes care of freeing old memory for word
				strcat(word, " WRONG\n"); // concatenate WRONG onto end of the word
				#ifdef TESTING
				printf("word was NOT found!\n"); // FOR TESTING
				#endif
			}

			write(socketDesc, word, strlen(word)); // write a message with write() or send() to client with word plus correctness
			write(socketDesc, msgRequest, strlen(msgRequest)); // wr
		
			// Write phrase to log buffer using mutual exclusion for log buffer
			pthread_mutex_lock(&log_mutex); // lock mutex for log buffer
			while(logCount == LOG_BUF_LEN) {// while log buffer is full
				pthread_cond_wait(&log_cv_pd, &log_mutex); // wait for log buffer to not be full
			}
			//char* phrase = insertPhrase(word);
			insertPhrase(word); // insert phrase into log buffer
			pthread_mutex_unlock(&log_mutex); // unlock mutex
			pthread_cond_signal(&log_cv_cs); // signal log buffer NOT EMPTY
			//free(phrase);
			free(word); // free old word used
			word = calloc(MAX_WORD_SIZE, 1); // calloc memory for new word to use
		}
		// print message to server that connection has been closed
		close(socketDesc); // close socketDesc
	}
}

void* logThreadFunc(void* arg) {
	while(1) {
		// take phrase out of log buffer with mutual exclusion
		pthread_mutex_lock(&log_mutex); // lock log mutex
		while(logCount == 0) { // while log buffer is empty
			pthread_cond_wait(&log_cv_cs, &log_mutex); // wait to consumer from log buffer
		}
		char* phraseToAppend = removePhraseFromLogBuff();
		printf("Phrase taken out of log buffer to append to log file: %s\n", phraseToAppend); // FOR TESTING
		logFile_ptr = fopen("log.txt", "a"); // open the log file for appending
		if (logFile_ptr == NULL) { // if the log file was not opened successfully
			printf("Error opening log file for appending!\n"); // print error message
			exit(1);
		}
		fputs(phraseToAppend, logFile_ptr); // append phrase taken out of log buffer to log file
		printf("Successfully appended to log file!\n"); // FOR TESTING
		fclose(logFile_ptr); // close file when done appending
		pthread_mutex_unlock(&log_mutex); // unlock log mutex
		pthread_cond_signal(&log_cv_pd); // signal log buffer not full since we took 
	}
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
		printf("socket descriptor inserted into job buffer: %d\n", jobBuff[jobRear]); // FOR TESTING
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

void insertPhrase(char* phrase) {
	// phrase = realloc(phrase, sizeof(char*)*PHRASE_SIZE); // realloc space for word shouldn't go here, it should go up before you concatenate 'correctness'
	if (logCount == LOG_BUF_LEN) { // if the log buffer is full
		printf("Log buffer is FULL!\n"); // print that it's full
	} else {
		if (logCount == 0) {// if the log buffer is empty, make the item the first in the queue, update front and rear
			logFront = 0;
			logRear = 0;
		}
		strcpy(logBuff[logRear], phrase);// add the phrase to the log buffer in the correct 
		logCount++; // increment count of phrases in the log buffer
		printf("phrase inserted into log buffer: %s\n", logBuff[logRear]); // FOR TESTING
	//	printf("log count after previous insertion: %d\n", logCount); // FOR TESTING WHEN LOG THREAD IS COMMENTED OUT
		logRear = (logRear + 1) % LOG_BUF_LEN; // reset logRear when it reaches the 100th index (this makes it circular)
	}

	// return phrase
}

char* removePhraseFromLogBuff() {
	if (logCount == 0) { // if the log buffer is empty
		printf("log buffer is empty! Can't remove anything!\n"); // print a message
	} else { // otherwise remove a phrase from the log buffer
		char* phrase = logBuff[logFront]; // store phrase to remove from log buffer in phrase
		logFront = (logFront + 1) % LOG_BUF_LEN; // reset logFront when it reaches 100th index (this makes it circular)
		logCount--; // decrement count of phrases in log buffer
		return phrase; // return the phrase to append to log file
	}
}

int searchForWordInDict(char list_of_words[][MAX_WORD_SIZE], char* wordToFind) {
	int i = 0;
	while(i < wordsInDictionary - 1) {
		if (strcmp(list_of_words[i], wordToFind) == 0) {
			printf("WORD FOUND!\n");
			return 1;
		}
		i++;
	}
	printf("Word NOT found!\n");
	return 0;
}