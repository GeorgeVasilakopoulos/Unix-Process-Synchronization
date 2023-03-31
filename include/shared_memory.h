struct shared_memory{
	int* reader_counter;	//for each segment
	char** segment;			//an array of lines of a single segment
	int segment_request;	//in this variable, the children declare the requested segment
	int total_serves;		
};








