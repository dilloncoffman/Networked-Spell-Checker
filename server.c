#include "server.h"

//An extremely simple server that connects to a given port.
//Once the server is connected to the port, it will listen on that port
//for a user connection.
//A user will connect through telnet, the user also needs to know the port number.
//If the user connects successfully, a socket descriptor will be created.
//The socket descriptor works exactly like a file descriptor, allowing us to read/write on it.
//The server will then use the socket descriptor to communicate with the user, sending and 
//receiving messages.

pthread_t threadPool[NUM_WORKER_THREADS]; // declare global thread pool to use
int connectionPort = 0; // declare global connectionPort to be used
char* dictionaryName = ""; // declare global dictionaryName to be used
char dictionary[DICTIONARY_SIZE][MAX_WORD_SIZE]; // dictionary to store words from dictionaryName file in
int wordsInDictionary = 0; // global var to keep track of word count of dictionary - to be used when searching

int main(int argc, char** argv)
{
	// Create threads

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
		while((fgets(dictionary[i], sizeof(dictionary[i]), dictionaryName_ptr) != NULL) && (i < DICTIONARY_SIZE - 1)) {  // store the word from the dictionary into dictionary data structure (two-dimensional char array in our case) fgets() also takes care of appending '\0' to the end of the word :) - using a while with fgets() like this ensures we don't read past the end of the statically allocated dictionary[][]
			strtok(dictionary[i], "\n"); // take out newline for each word in dictionary file - MIGHT NEED \n IN SEARCHING PART TO SEE WHERE WORD ENDS
			wordsInDictionary++;
			#ifdef TESTING
			printf("WORD: %s\n", dictionary[i]); // print each word FOR TESTING
			#endif
			i++;
		}
		#ifdef TESTING
		printf("Word count of words in the dictionary: %d\n", wordsInDictionary); // FOR TESTING
		#endif
	}

	//sockaddr_in holds information about the user connection. 
	//We don't need it, but it needs to be passed into accept().
	struct sockaddr_in client;
	int clientLen = sizeof(client);
	int connectionSocket, clientSocket, bytesReturned;
	char recvBuffer[BUF_LEN];
	recvBuffer[0] = '\0';

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

	while(1) { // continously accepts connections to process once currently connected client quits
	
		//accept() waits until a user connects to the server, writing information about that server
		//into the sockaddr_in client.
		//If the connection is successful, we obtain A SECOND socket descriptor. 
		//There are two socket descriptors being used now:
		//One by the server to listen for incoming connections.
		//The second that was just created that will be used to communicate with 
		//the connected user.
		if ((clientSocket = accept(connectionSocket, (struct sockaddr*)&client, &clientLen)) == -1){
			printf("Error connecting to client.\n");
			return -1;
		}

		// Add clientSocket to Job Queue THIS SHOULD BE IN SIDE MUTEX LOCK WITH CONDITION VARIABLES


		printf("Connection success!\n");
		char* clientMessage = "Hello! I hope you can see this.\n";
		char* msgRequest = "Send me some text and I'll respond with something interesting!\nSend the escape key to close the connection.\n";
		char* msgResponse = "I actually don't have anything interesting to say...but I know you sent ";
		char* msgPrompt = ">>>";
		char* msgError = "I didn't get your message. ):\n";
		char* msgClose = "Goodbye!\n";

		//send()...sends a message.
		//We specify the socket we want to send, the message and it's length, the 
		//last parameter are flags.
		send(clientSocket, clientMessage, strlen(clientMessage), 0);
		send(clientSocket, msgRequest, strlen(msgRequest), 0);


		//Begin sending and receiving messages.
		while(1){
			send(clientSocket, msgPrompt, strlen(msgPrompt), 0);
			//recv() will store the message from the user in the buffer, returning
			//how many bytes we received.
			bytesReturned = recv(clientSocket, recvBuffer, BUF_LEN, 0);

			//Check if we got a message, send a message back or quit if the
			//user specified it.
			if(bytesReturned == -1){
				send(clientSocket, msgError, strlen(msgError), 0);
			}
			//'27' is the escape key.
			else if(recvBuffer[0] == 27){
				send(clientSocket, msgClose, strlen(msgClose), 0);
				close(clientSocket);
				break;
			}
			else{
				send(clientSocket, msgResponse, strlen(msgResponse), 0); // sends default message response to be followed by what the user entered 
				send(clientSocket, recvBuffer, bytesReturned, 0); // sends what was received from user back to client
			}
		}
	}
	
	return 0;
}