#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include "util.h"

#define MAX_INPUT_FILES 10
#define MAX_CONVERT_THREADS 10
#define MAX_PARSING_THREADS 20
#define MAX_NAME_LENGTH 1025
#define INDEX_MAX 30
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

pthread_t p_tid[MAX_PARSING_THREADS];
pthread_t c_tid[MAX_CONVERT_THREADS];
static char *INTER_BUFF;
int BUFFER_INDEX = 0;
int PARSE_DONE = 0; // 0 means parsing thread still exists
int BUFFER_EMPTY = 1; // 1 means buffer is currently empty, 0 means there is at least 1 entry

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void *parsing(void *arg){
	
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	int files_served = 0;

	int thread_no = 0;
	pthread_t id = pthread_self();
	
	for (thread_no = 0; thread_no < MAX_PARSING_THREADS; thread_no++ ){
		if ( pthread_equal(id , p_tid[thread_no]) ){
			break;
		}
	}
	

	fp = fopen(arg, "r");
	
	if (fp != NULL){
		while ((read = getline(&line, &len, fp)) != -1) {

			pthread_mutex_lock( &buffer_mutex );
			if ( BUFFER_INDEX < INDEX_MAX ){
				memcpy(INTER_BUFF + BUFFER_INDEX * MAX_NAME_LENGTH, line, MAX_NAME_LENGTH );			
				BUFFER_INDEX++;
				BUFFER_EMPTY = 0;
			}
			pthread_mutex_unlock( &buffer_mutex );
		}
		fclose(fp);
		files_served++;
	} else {
		printf("'%s' is invalid input file.\n", (char*)arg);
	}
	
	printf("Parsing thread %d serviced %d files. Thread exiting.\n", thread_no, files_served);
	
	pthread_exit(0);
	return NULL;
}





void *convert(void *arg){
	
	FILE * fp;
	char tempstr[MAX_NAME_LENGTH];
	char tempIP[MAX_IP_LENGTH];
	int err;

	fp = fopen(arg, "a");

	if (fp == NULL){
		printf("Invalid output file.\n");
		return NULL;
	}

	while ( !PARSE_DONE || !BUFFER_EMPTY ){
		pthread_mutex_lock( &buffer_mutex );
		if ( !BUFFER_EMPTY ){
			BUFFER_INDEX--;
			memcpy( tempstr, INTER_BUFF + BUFFER_INDEX * MAX_NAME_LENGTH, MAX_NAME_LENGTH );
			printf("Searching IP for %s", tempstr);

			err = dnslookup( strtok(tempstr, "\n") , tempIP , MAX_IP_LENGTH );

			fputs( tempstr, fp);
			fputs( ", ", fp);
			if (err == 0 ){
				fputs( tempIP, fp);
			}
			fputs("\n", fp);
			

			if ( BUFFER_INDEX == 0 ){
				BUFFER_EMPTY = 1;
			}
		}
		pthread_mutex_unlock( &buffer_mutex );
	}
	
	fclose(fp);
	printf("Converting thread 1 exiting.\n");

	pthread_exit(0);
	return NULL;
}





int main(int argc, char* argv[]){
	
	int i = 0;
	int j = 0;
	int par_threads;
	int con_threads;
	int arg;
	int err;
	void *status;

	if (argc < 6){
		printf ("Missing parameters\n");
		return 0;
	}

	INTER_BUFF = malloc(INDEX_MAX * MAX_NAME_LENGTH);
	
	printf ( "The parameters are: \n");
	for (arg = 0; arg < argc ; arg++){
		printf ("%3d: [%s]\n", arg, argv[arg]);
	}


	par_threads = atoi(argv[1]);
	//
	//
	// REMOVE THIS LINE
	par_threads = 1; // for step 3
	//
	//
	printf("Number of parsing threads: %3d\n", par_threads);

	if (par_threads == 0){
		printf("Invalid number of parsing threads.\n");
		return 0;
	}
	// match number of parsing threads to number of files to read.
	if (par_threads > argc-5){
		par_threads = argc-5;
		printf("Actual par_threads: %3d\n", par_threads);
	}
	
	con_threads = atoi(argv[2]);
	//
	//
	// REMOVE THIS LINE
	con_threads = 1; // for step 3
	//
	//
	printf("Number of converting threads: %3d\n", con_threads);

	if (con_threads == 0){
		printf("Invalid number of parsing threads.\n");
		return 0;
	}

	//
	// to be reworked
	//
	while ( i < par_threads ){

		err = pthread_create(&(p_tid[i]), NULL, &parsing, (void *)argv[5]);
		if (err != 0){
			printf("Failed to create parsing thread. Exiting...\n");
			return -1;
		}
		i++;

	}
	
	while ( j < con_threads ){

		err = pthread_create(&(c_tid[j]), NULL, &convert, (void *)argv[4]);
		if (err != 0){
			printf("Failed to create converting thread. Exiting...\n");
			return -1;
		}
		j++;

	}

	pthread_join(p_tid[0], &status);
	PARSE_DONE = 1;
	pthread_join(c_tid[0], &status); // same status...	
	//
	// to be reworked
	//
	
	free(INTER_BUFF);
	printf("\nFinished writing to %s.\n\n", argv[4]);
	return 0;
}
