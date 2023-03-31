#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>


#include "shared_memory.h"
#include "macros.h"

#define READING_TIME 0.02 		//20ms

extern int lines_per_segment;
extern int number_of_segments;
extern int requests_per_child;


//Function that returns wall time difference
double getTime(double time_0){
	struct timeval current_time;
	gettimeofday(&current_time,0);
	return (current_time.tv_sec + current_time.tv_usec*1e-6 - time_0);
}


void child(struct shared_memory* sharedMemory, sem_t* availabletoServe, sem_t*writeRequest, sem_t*segmentLoaded, sem_t*doneReading, sem_t**sem_array, int ID, double time_0){

	//Open child report file
	char filename[20];
	sprintf(filename,"childReport%d.txt",ID); 	//Name format.
	FILE* Report_File = fopen(filename,"w");
	if(Report_File == NULL){
		perror("error in opening file");
		exit(1);
	}


	srand(time(NULL)-getpid());
	
	//Generate a random request
	int requested_segment = rand()%number_of_segments;
	int requested_line = rand()%lines_per_segment;
	

	//Request Loop
	for(int i=0;i<requests_per_child;i++){
		double begin_time = getTime(time_0);

		
		SEM_WAIT_FAIL_CHECK(sem_array[requested_segment]);			//If another child has requested the same segment, wait
		sharedMemory->reader_counter[requested_segment]++;	
		if(sharedMemory->reader_counter[requested_segment]==1){		//if this is the first segment request, wait until producer is available
			SEM_WAIT_FAIL_CHECK(availabletoServe);
			sharedMemory->segment_request = requested_segment;		//Declare the requested segment
			SEM_POST_FAIL_CHECK(writeRequest);
			SEM_WAIT_FAIL_CHECK(segmentLoaded);						//Wait until the segment is loaded in the sh. m.
		}
		SEM_POST_FAIL_CHECK(sem_array[requested_segment]);			//Notify other processes that are waiting
		


		//Write a line in the report file
		char report_buffer[100];
		double end_time = getTime(time_0);
		sprintf(report_buffer,"<%d,%d>: Request Time: %.4fs, Response Time: %.4fs\n",requested_segment,requested_line,begin_time,end_time);
		fwrite(report_buffer,strlen(report_buffer),1,Report_File);
		
		
		sleep(READING_TIME);		//Simulate reading of the segment


		SEM_WAIT_FAIL_CHECK(sem_array[requested_segment]);
		sharedMemory->reader_counter[requested_segment]--;
		sharedMemory->total_serves++;
		if(sharedMemory->reader_counter[requested_segment]==0){
			SEM_POST_FAIL_CHECK(doneReading);						//If this is the last reader, notify the producer
		}
		SEM_POST_FAIL_CHECK(sem_array[requested_segment]);
		

		//0.7 probability for same segment
		//0.3 probability for random segment
		if(rand()%10 >=7){
			requested_segment = rand()%number_of_segments;
		}
		requested_line = rand()%lines_per_segment;
	}
	fclose(Report_File);
}