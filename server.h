#ifndef _SIMPLE_SERVER_H
#define _SIMPLE_SERVER_H
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> // used for comparing args at start ie. checking for port number vs. dictionary file
#include <netinet/in.h> // need for sockaddr_in struct to use with accept()
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> // for working with threads, mutexes, and condition variables

// Defined constants
#define DEFAULT_PORT 7000 // to be used if no port specified by  user
#define DEFAULT_DICTIONARY "words.txt" // to be used if user does not specify a dictionary to use
#define NUM_WORKER_THREADS 3 // number of worker threads to be created at start
#define DICTIONARY_SIZE 200425 // size of dictionary file to be read in
#define JOB_BUF_LEN 100 // size of job buffer which holds socket descriptors
#define LOG_BUF_LEN 100 // size of log buffer
#define MAX_WORD_SIZE 15 // max word size possible in dictionary
#define PHRASE_SIZE 25 // max phrase size - to be able to concatenate and include 'OK' or 'WRONG' on end of word

// Function Prototypes
int open_listenfd(int);
void* workerThreadFunc(void*);
void* logThreadFunc(void*);
int searchForWordInDict(char list_of_words[][MAX_WORD_SIZE], char* wordToFind);

#endif
