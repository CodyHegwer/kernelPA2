#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include "util.h"

#define MAX_INPUT_FILES 10
#define MAX_CONVERT_THREADS 10
#define MAX_PARSING_THREADS 10
#define MAX_NAME_LENGTH 1025
#define INDEX_MAX 30
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

pthread_t p_tid[MAX_PARSING_THREADS];
pthread_t c_tid[MAX_CONVERT_THREADS];
static char *INTER_BUFF;
static char *INPUT_BUFF; // Buffer to store names of input files.
int BUFFER_INDEX = 0;
int INPUT_INDEX = 0;
int PARSE_DONE = 0; // 0 means parsing thread still exists
int INTER_BUFFER_EMPTY = 1; // 1 means buffer is currently empty, 0 means there is at least 1 entry

pthread_mutex_t par_log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t output_file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t input_file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void *parsing(void *parlog_fp){
	
	char filename[MAX_NAME_LENGTH];
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	int files_served = 0;
	int written = 0; // boolean, 1 means successfully wrote to buffer, 0 means still trying to write to buffer.
	char tempstr[MAX_NAME_LENGTH];

	int thread_no = 0;
	pthread_t id = pthread_self();
	
	for (thread_no = 0; thread_no < MAX_PARSING_THREADS; thread_no++ ){
		if ( pthread_equal(id , p_tid[thread_no]) ){
			break;
		}
	}
	

	while ( INPUT_INDEX > -1 ){ // valid index
		
		pthread_mutex_lock( &input_file_mutex );
		if ( INPUT_INDEX <= -1 ){ // invalid index/double check
			break;
		} else {
			memcpy( filename, INPUT_BUFF + INPUT_INDEX * MAX_NAME_LENGTH, MAX_NAME_LENGTH );
			INPUT_INDEX--;
		}
		pthread_mutex_unlock( &input_file_mutex );


		fp = fopen(filename, "r");
	
		if (fp != NULL){
			while ((read = getline(&line, &len, fp)) != -1) {

				written = 0;
				while ( !written ){ 

					if ( BUFFER_INDEX < INDEX_MAX ){ // remove busy waiting
						pthread_mutex_lock( &buffer_mutex );
						if ( BUFFER_INDEX < INDEX_MAX ){ // double check for race condition
							memcpy(INTER_BUFF + BUFFER_INDEX * MAX_NAME_LENGTH, line, MAX_NAME_LENGTH );		
							BUFFER_INDEX++;
							INTER_BUFFER_EMPTY = 0;
							written = 1;
						}
						pthread_mutex_unlock( &buffer_mutex );
					}

				}
			}
			fclose(fp);
			files_served++;
		} else {
			printf("'%s' is invalid input file.\n", filename);
		}
	}
	
	sprintf(tempstr, "Thread %d serviced %d files.\n", thread_no, files_served);

	pthread_mutex_lock( &par_log_mutex );
	fputs( tempstr, parlog_fp );
	pthread_mutex_unlock( &par_log_mutex );

	printf("Parsing thread %d serviced %d files. Thread exiting.\n", thread_no, files_served);
	free(line);
	pthread_exit(0);
	return NULL;
}





void *convert(void *fp){
	
	char tempstr[MAX_NAME_LENGTH];
	char tempIP[MAX_IP_LENGTH];
	int err;
	int copied = 0; // boolean, 0 means did not copy, 1 means did copy something from buffer

	int thread_no = 0;
	pthread_t id = pthread_self();

	for (thread_no = 0; thread_no < MAX_CONVERT_THREADS; thread_no++ ){
		if ( pthread_equal( id , c_tid[thread_no]) ){
			break;
		}
	}

	while ( !PARSE_DONE || !INTER_BUFFER_EMPTY ){

		copied = 0; // reset copied for current loop iteration

		pthread_mutex_lock( &buffer_mutex );
		if ( !INTER_BUFFER_EMPTY ){ // double check for race condition
			BUFFER_INDEX--;
			memcpy( tempstr, INTER_BUFF + BUFFER_INDEX * MAX_NAME_LENGTH, MAX_NAME_LENGTH );
			copied = 1;		

			if ( BUFFER_INDEX == 0 ){
				INTER_BUFFER_EMPTY = 1;
			}
		}
		pthread_mutex_unlock( &buffer_mutex );

		if (copied) {

			pthread_mutex_lock( &output_file_mutex );
			printf("Searching IP for %s", tempstr);
			err = dnslookup( strtok( tempstr, "\n"), tempIP , MAX_IP_LENGTH );

			if (err != 0){
				printf("'%s' is an invalid domain name.\n", strtok(tempstr, "\n"));
			}
			
			fputs( tempstr, fp );
			fputs( ", ", fp );
			
			if (err == 0 ){
				fputs( tempIP, fp );
			}
			fputs("\n", fp);

			pthread_mutex_unlock( &output_file_mutex );
		}

	}
	
	printf("Converting thread %d exiting.\n", thread_no);

	pthread_exit(0);
	return NULL;
}





int main(int argc, char* argv[]){
	
	// counter variables
	int i = 0;
	int j = 0;
	int k = 5;

	int num_input_files = 0;
	int par_threads;
	int con_threads;
	int arg;
	int err;
	FILE * parlog_fp; // parsing log file pointer
	FILE * output_fp; // output file pointer
	void *status;

	if (argc < 6){
		printf ("Missing parameters\n");
		return -1;
	}

	INTER_BUFF = malloc(INDEX_MAX * MAX_NAME_LENGTH);
	
	printf ( "The parameters are: \n");
	for (arg = 0; arg < argc ; arg++){
		printf ("%3d: [%s]\n", arg, argv[arg]);
	}
	
	// if argc is > 15, there are more than 10 input files
	if (argc > MAX_INPUT_FILES + 5) {
		argc = MAX_INPUT_FILES + 5;
		printf ( "Too many input files, max files is %d.\n", MAX_INPUT_FILES);
	}

	// count number of input files, and create input name buffer. k starts at 5..
	while ( k < argc ) {
		num_input_files++;
		k++;
	}
	k = 0; // reset counter

	printf ( "There are %d input files to parse.\n", num_input_files );
	INPUT_INDEX = num_input_files - 1; // start from end of buffer.
	INPUT_BUFF = malloc( num_input_files * MAX_NAME_LENGTH ); // allocate space 
	while ( k <= INPUT_INDEX ){ // write input names into buffer
		printf ( "%s\n", argv[k+5] );
		memcpy( INPUT_BUFF + k * MAX_NAME_LENGTH, argv[k+5], MAX_NAME_LENGTH );
		k++;
	}

	par_threads = atoi(argv[1]);
	printf("Number of parsing threads: %3d\n", par_threads);

	if (par_threads <= 0){
		printf("Invalid number of parsing threads.\n");
		return -1;
	}
	// Require not to exceed maximum number of parsing threads.
	if (par_threads > MAX_PARSING_THREADS){
		par_threads = MAX_PARSING_THREADS;
		printf("Actual par_threads: %3d\n", par_threads);
	}
	
	con_threads = atoi(argv[2]);
	printf("Number of converting threads: %3d\n", con_threads);

	if (con_threads <= 0){
		printf("Invalid number of parsing threads.\n");
		return -1;
	}
	// Require not to exceed maximum number of converting threads.
	if (con_threads > MAX_CONVERT_THREADS){
		con_threads = MAX_CONVERT_THREADS;
		printf("Actual con_threads: %3d\n", con_threads);
	}

	// open output file to pass to converting threads.
	output_fp = fopen(argv[4], "a");
	if ( output_fp == NULL ){
		printf("\nInvalid output file.\n\n");
		return -1;
	}

	//open parsing log file to pass to parsing threads.
	parlog_fp = fopen(argv[3], "a");
	if ( parlog_fp == NULL ){
		printf("\nInvalid parse log file.\n\n");
		return -1;
	}

	// create parsing threads
	while ( i < par_threads ){

		err = pthread_create(&(p_tid[i]), NULL, &parsing, (void *)parlog_fp);
		if (err != 0){
			printf("Failed to create parsing thread. Exiting...\n");
			return -1;
		}
		i++;

	}
	i = 0; // reset counter
	
	// create converting threads
	while ( j < con_threads ){

		err = pthread_create(&(c_tid[j]), NULL, &convert, (void *)output_fp);
		if (err != 0){
			printf("Failed to create converting thread. Exiting...\n");
			return -1;
		}
		j++;

	}
	j = 0;

	// wait for parsing threads
	while ( i < par_threads ){
		pthread_join(p_tid[i], &status);
		i++;
	}
	PARSE_DONE = 1;

	// wait for converting threads
	while ( j < con_threads ){
		pthread_join(c_tid[j], &status);
		j++;
	}	

	
	free(INTER_BUFF);
	free(INPUT_BUFF);
	fclose(parlog_fp);
	fclose(output_fp);
	printf("\nFinished writing to %s \n\n", argv[4]);
	return 0;
}
