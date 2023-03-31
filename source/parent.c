#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shared_memory.h"
#include "macros.h"


#define CHARS_PER_LINE 100

#define NUMBER_OF_CHILDREN 30


int lines_per_segment;
int number_of_segments;
int requests_per_child;


//Definitions located at child.c
void child(struct shared_memory* sharedMemory, sem_t* availabletoServe, sem_t*writeRequest, sem_t*segmentLoaded, sem_t*doneReading, sem_t**sem_array, int ID,double time_0);
double getTime(double time_0);


static char* file_name;



static struct shared_memory* allocateSharedMemory(int number_of_segments){
	struct shared_memory* sharedMemory;

	int shmid;

	//Allocating space for shared memory
	if((shmid = shmget(IPC_PRIVATE,sizeof(struct shared_memory),0666 | IPC_CREAT))==-1){
		perror("shmget");
		exit(1);
	}
	sharedMemory = shmat(shmid,NULL,0);
	if(sharedMemory == (struct shared_memory*)-1){
		perror("shmat");
		exit(1);
	}
	sharedMemory->total_serves=0;


	if((shmid = shmget(IPC_PRIVATE,sizeof(char*)*lines_per_segment,0666 | IPC_CREAT))==-1){
		perror("shmget");
		exit(1);
	}	
	sharedMemory->segment = shmat(shmid,NULL,0);
	if(sharedMemory->segment == (char**)-1){
		perror("shmat");
		exit(1);
	}




	//Allocating memory for the readers counter 
	if((shmid = shmget(IPC_PRIVATE,sizeof(int)*number_of_segments,0666 | IPC_CREAT))==-1){
		perror("shmget");
		exit(1);
	}
	sharedMemory->reader_counter = shmat(shmid,NULL,0);
	if(sharedMemory->reader_counter == (int*)-1){
		perror("shmat");
		exit(1);
	}
	for(int i=0;i<number_of_segments;i++)sharedMemory->reader_counter[i]=0;





	//Allocating memory for each line
	for(int i = 0; i<lines_per_segment; i++){
		if((shmid = shmget(IPC_PRIVATE,sizeof(char)*CHARS_PER_LINE,0666 | IPC_CREAT))==-1){
			perror("shmget");
			exit(1);
		}

		sharedMemory->segment[i] = shmat(shmid,NULL,0);
		if(sharedMemory->segment[i] == (char*)-1){
			perror("shmat");
			exit(1);
		}
	}

	return sharedMemory;
}


static void deallocateSharedMemory(struct shared_memory* sharedMemory){
	for(int i=0;i<lines_per_segment;i++){
		if(shmdt((void*)sharedMemory->segment[i])==-1){
			perror("shmdt failure");
			exit(1);
		}
	}
	if(shmdt((void*)sharedMemory->segment)==-1){
		perror("shmdt failure");
		exit(1);
	}

	if(shmdt((void*)sharedMemory->reader_counter)==-1){
		perror("shmdt failure");
		exit(1);
	}
	

	if(shmdt((void*)sharedMemory)==-1){
		perror("shmdt failure");
		exit(1);
	}
}


//Load segment into the shared memory
static void getSegment(struct shared_memory* sharedMemory, int segment_number){
	FILE* file;
	if((file = fopen(file_name, "r"))==NULL){
		fprintf(stderr,"File \"%s\" not found", file_name);
		exit(1);
	}
	for(int i=0;i<segment_number*lines_per_segment;i++){	//Skip lines in segments 0 to segment_number-1
		fgets(sharedMemory->segment[0],CHARS_PER_LINE,file);
	}
	for(int i=0;i<lines_per_segment;i++){					//Read the lines of the segment
		fgets(sharedMemory->segment[i],CHARS_PER_LINE,file);
	}
	fclose(file);
}


static int countLinesinFile(){
	FILE* file;
	if((file = fopen(file_name, "r"))==NULL){
		fprintf(stderr,"File \"%s\" not found", file_name);
		exit(1);
	}
	char dummyarray[CHARS_PER_LINE];
	int counter = 0;
	while(fgets(dummyarray,CHARS_PER_LINE,file))counter++;
	fclose(file);
	return counter;
}


//Initializes an array of semaphores, of size "number_of_sems"
static sem_t** initializeSemaphores(int number_of_sems){
	sem_t** array_of_sems = (sem_t**) malloc(sizeof(sem_t*)*number_of_sems);
	char sem_name[20];
	for(int i=0;i<number_of_sems;i++){
		sprintf(sem_name,"seg%d",i);		//Semaphore names format
		array_of_sems[i]=sem_open(sem_name, O_CREAT | O_EXCL, SEM_PERMS, 1);
		SEM_OPEN_FAIL_CHECK(array_of_sems[i]);
	}
	return array_of_sems;
}

//Unlinks appropriately the semaphore array
static void unlinkSemaphores(sem_t** array_of_sems, int number_of_sems){
	char sem_name[20];
	for(int i=0;i<number_of_sems;i++){
		sprintf(sem_name,"seg%d",i);
		SEM_UNLINK_FAIL_CHECK(sem_name);
	}
	free(array_of_sems);
}

int main(int argc, char*argv[]){

	//Read the call inputs
	if(argc<4){
		fprintf(stderr, "Not enough arguments!\n");
		exit(EXIT_FAILURE);
	}
	file_name = argv[1];
	lines_per_segment = atoi(argv[2]);
	requests_per_child = atoi(argv[3]);
	int number_of_lines = countLinesinFile();
	number_of_segments=number_of_lines/lines_per_segment + (number_of_lines%lines_per_segment > 0);


	//Allocate sh. m.
	struct shared_memory* sharedMemory = allocateSharedMemory(number_of_segments);

	
	//Initialize Semaphores
	sem_t** sem_array = initializeSemaphores(number_of_segments);

	sem_t* availabletoServe = sem_open("availabletoServe",O_CREAT | O_EXCL, SEM_PERMS, 0);
	SEM_OPEN_FAIL_CHECK(availabletoServe);

	sem_t* writeRequest = sem_open("writeRequest",O_CREAT | O_EXCL, SEM_PERMS, 0);
	SEM_OPEN_FAIL_CHECK(writeRequest);

	sem_t* segmentLoaded = sem_open("segmentLoaded",O_CREAT | O_EXCL, SEM_PERMS, 0);
	SEM_OPEN_FAIL_CHECK(segmentLoaded);
	
	sem_t* doneReading = sem_open("doneReading",O_CREAT | O_EXCL, SEM_PERMS, 0);
	SEM_OPEN_FAIL_CHECK(doneReading);


	//Open parent report file
	FILE* Report_File = fopen("parentReport.txt","w");
	char report_buffer[100];
	if(Report_File == NULL){
		perror("error in opening file");
		exit(1);
	}


	//Get wall time at the beginning.
	double time_0 = getTime(0);


	//Generate child processes
	pid_t children_ids[NUMBER_OF_CHILDREN];
	for(int i =0;i<NUMBER_OF_CHILDREN;i++){
		if((children_ids[i] = fork())<0){
			perror("Process creation failed");
			exit(1);
		}
		if(children_ids[i] == 0){
			child(sharedMemory,availabletoServe, writeRequest,segmentLoaded,doneReading,sem_array,i+1,time_0);
			exit(0);
		}
	}


	//Serving loop
	while(sharedMemory->total_serves < NUMBER_OF_CHILDREN*requests_per_child){
		SEM_POST_FAIL_CHECK(availabletoServe);					//Declare availability
		SEM_WAIT_FAIL_CHECK(writeRequest);						//Wait for the child process to write the request
		getSegment(sharedMemory,sharedMemory->segment_request);	//Load the requested segment
		double load_time = getTime(time_0);	
		SEM_POST_FAIL_CHECK(segmentLoaded);						//Declare that the segment has been loaded
		SEM_WAIT_FAIL_CHECK(doneReading);						//Wait for the last child process to finish reading
		double unload_time = getTime(time_0);
		

		//Write a line in the report file
		sprintf(report_buffer,"Segment %d: Load Time: %.4fs, Exit Time: %.4fs\n",sharedMemory->segment_request,load_time,unload_time);
		fwrite(report_buffer,strlen(report_buffer),1,Report_File);
	}


	SEM_UNLINK_FAIL_CHECK("availabletoServe");
	SEM_UNLINK_FAIL_CHECK("writeRequest");
	SEM_UNLINK_FAIL_CHECK("segmentLoaded");
	SEM_UNLINK_FAIL_CHECK("doneReading");
	unlinkSemaphores(sem_array,number_of_segments);
	deallocateSharedMemory(sharedMemory);
	fclose(Report_File);
	return 0;
}

