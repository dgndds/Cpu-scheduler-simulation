#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

pthread_mutex_t a_mutex;
pthread_cond_t a_cond;
double* waitTimes;

typedef struct burst_node{
		int t_index;
		int burstIndex;
		double length;
		struct timeval time;
		struct burst_node* nextBurst;
		pthread_mutex_t list_lock;
}burst_node_t;

struct arg{
	//mode flag
	int mode;
	
	//args of command line
	int t_index;
	int burstCount;
	int threadCount;
	int minA;
	int minB;
	char algo[256];
	int avgA;
	int avgB;
	
	//args of file
	int lineCount;
	int* startPos;
	int* lines;
	int* bursts_waits;
};


burst_node_t* rq = NULL;
	
void printList(burst_node_t* head){
	printf("------ BURST LIST ------\n");
	burst_node_t* curr = head;
	
	while(curr != NULL){
		printf("Thread index: %d\n",curr->t_index);
		printf("Burst index: %d\n",curr->burstIndex);
		printf("Burst length: %f\n",curr->length);
		printf("Creation time: %ld\n",curr->time.tv_usec);
		printf("------------------\n");
		curr = curr->nextBurst;
	}	
}

burst_node_t * popBurst( burst_node_t** head) {
   	burst_node_t * retBurst = NULL;
    burst_node_t * next_node = NULL;

    next_node = (*head)->nextBurst;
    retBurst = *head;
    *head = next_node;

    return retBurst;
}


burst_node_t* removeBurstByIndex(burst_node_t ** head, int n) {
    int i = 0;
    burst_node_t* retval = NULL;
    burst_node_t* current = *head;
    burst_node_t* temp_node = NULL;

    if (n == 0) {
        return popBurst(&(*head));
    }

    for (i = 0; i < n-1; i++) {
        if (current->nextBurst == NULL) {
            return NULL;
        }
        current = current->nextBurst;
    }

    temp_node = current->nextBurst;
    retval = temp_node;
    current->nextBurst = temp_node->nextBurst;

    return retval;
}


int findMinBurstLengthIndex(burst_node_t* head) {
    burst_node_t* temp;
    double min = head->length;
    temp = head;
    int minIndex = 0;
    int counter = 0;
    
    while (temp != NULL) {
        if (temp->length <= min) {
            min = temp->length;
            minIndex = counter;
        }
        
        temp = temp->nextBurst;
        counter++;
    }
    
    printf("The min is at %d\n", minIndex);
    return minIndex;
}

int findMinThreadPriorityIndex(burst_node_t* head) {
    burst_node_t* temp;
    int minThread = head->t_index;
    temp = head;
    int counter = 0;
    
    while (temp != NULL) {
        if (temp->t_index <= minThread) {
            minThread = temp->t_index;
        }
        
        temp = temp->nextBurst;
        counter++;
    }
    
    temp = head;
    counter = 0;
    int minBurstIndex = head->burstIndex;
	int index = 0;
    
     while (temp != NULL) {
        if(temp->t_index == minThread){
        	if (temp->burstIndex <= minBurstIndex) {
		    	minBurstIndex = temp->burstIndex;
		    	index = counter;
        	}
        }
        
        temp = temp->nextBurst;
        counter++;
    }

    return index;
}

int findMinRunTimeIndex(burst_node_t* head, double threadRunTimes[], int threadCount){
	
	int notAllZero = 0;
	
	for(int i = 0; i < threadCount; i++){
		if(threadRunTimes[i] != 0.0){
			notAllZero = 1;
			break;
		}
	}
	
	if(notAllZero){
		return 0;
	}
	
	double minRunTime = 0.0;
	int minRunTimeIndex = 0;
	
	for(int i = 0; i < threadCount; i++){
		if(threadRunTimes[i] <= minRunTime){
			minRunTime = threadRunTimes[i];
			minRunTimeIndex = i;
		}
	}
	
	burst_node_t* temp;
	temp =  head;
	
	int counter = 0;
	int index = 0;
	
	while (temp != NULL) {
        if(temp->t_index == minRunTimeIndex){
        		return counter;    	
        }
        
        temp = temp->nextBurst;
        counter++;
    }
	
	
	return 0;
}

void addBurstToEnd(burst_node_t** head,int t_index,int burstIndex,double length, struct timeval time){

	if(*head == NULL){
		burst_node_t * new_node;
		new_node = (burst_node_t *) malloc(sizeof(burst_node_t));

		new_node->t_index = t_index;
		new_node->burstIndex = burstIndex;
		new_node->length = length;
		new_node->time = time;
		new_node->nextBurst = NULL;
		*head = new_node;
		pthread_mutex_unlock(&((*head)->list_lock));
		return;
	}
	
	burst_node_t* curr = *head;
	
	while(curr->nextBurst != NULL){
		curr = curr -> nextBurst;
	}
	
	curr->nextBurst = (burst_node_t*)malloc(sizeof(burst_node_t));
	curr->nextBurst->t_index = t_index;
	curr->nextBurst->burstIndex = burstIndex;
	curr->nextBurst->length = length;
	curr->nextBurst->time = time;
	curr->nextBurst->nextBurst = NULL;
}

void* cpuBurst(void* param){
	int burstIndex = 0;
	struct timeval tv;
	//double currentTime;
	
	if(((struct arg *)param)->mode){
		pthread_mutex_lock(&a_mutex);
		
		for(int i = 0; i < ((struct arg *)param)->lines[((struct arg *)param)->t_index-1]*2; i++){
			if(i % 2 == 0){
				pthread_mutex_unlock(&a_mutex);
				usleep(((struct arg *)param)->bursts_waits[((struct arg *)param)->startPos[((struct arg *)param)->t_index-1] + i]*1000);
				pthread_mutex_lock(&a_mutex);
			}else{
				
				printf("burst length added %d of %d: %d\n",burstIndex,((struct arg *)param)->t_index,((struct arg *)param)->bursts_waits[((struct arg *)param)->startPos[((struct arg *)param)->t_index-1] + i]);
				
				gettimeofday(&tv,NULL);
				//currentTime = tv;//(tv.tv_sec)*1000 + (tv.tv_usec)/100;
				addBurstToEnd(&rq,((struct arg *)param)->t_index,burstIndex,((struct arg *)param)->bursts_waits[((struct arg *)param)->startPos[((struct arg *)param)->t_index-1] + i],tv);
				pthread_cond_signal(&a_cond);
				burstIndex++;
			}
			
		}
		
		pthread_mutex_unlock(&a_mutex);
	}else{
		pthread_mutex_lock(&a_mutex);
		double expoRn;
		
		while(burstIndex < ((struct arg *)param)->burstCount){
			expoRn= (double)rand() / (double)RAND_MAX;
				
			double arrivalTime = (((struct arg *)param)->avgA*log(expoRn))*-1;
			
			while(arrivalTime <= ((struct arg *)param)->minA){
				expoRn= (double)rand() / (double)RAND_MAX;
				arrivalTime = (((struct arg *)param)->avgA*log(expoRn))*-1;
			}
			
			pthread_mutex_unlock(&a_mutex);
			usleep(arrivalTime*1000);
			pthread_mutex_lock(&a_mutex);
			
			expoRn= (double)rand() / (double)RAND_MAX;
			double burstLength = (((struct arg *)param)->avgB*log(expoRn))*-1;
			
			while(burstLength <= ((struct arg *)param)->minB){
				expoRn= (double)rand() / (double)RAND_MAX;
				burstLength = (((struct arg *)param)->avgB*log(expoRn))*-1;
			}
			
			printf("burst length added %d of %d: %f\n",burstIndex,((struct arg *)param)->t_index,burstLength);

			gettimeofday(&tv,NULL);
			//currentTime = tv;//(tv.tv_sec)*1000 + (tv.tv_usec)/100;
			addBurstToEnd(&rq,((struct arg *)param)->t_index,burstIndex,burstLength,tv);
			pthread_cond_signal(&a_cond);
			burstIndex++;
		}
		
		pthread_mutex_unlock(&a_mutex);
	}
	
	pthread_exit(NULL);
}

void* scheduleCpuBurst(void* param){
	
	struct timeval tv;
	
	if(strcmp(((struct arg *)param)->algo,"FCFS") == 0){
		printf("SERVIG FCFS\n");
		
		pthread_mutex_lock(&a_mutex);
		int totalBurstCount;
		
		if(((struct arg *)param)->mode){
			totalBurstCount = ((struct arg *)param)->lineCount;
		}else{
			totalBurstCount = ((((struct arg *)param)->threadCount)*(((struct arg *)param)->burstCount));
		}
		
		int servedCount = 0;
		while(servedCount < totalBurstCount){
			while(rq == NULL){
				printf("Waiting for new cpu burst...\n");
				pthread_cond_wait(&a_cond, &a_mutex);
			}
		
		
			burst_node_t * burst = popBurst(&rq);
			servedCount++;
			printf("Serving %d of %d\n",burst->burstIndex, burst->t_index);
			pthread_mutex_unlock(&a_mutex);
			usleep(burst->length*1000);
			pthread_mutex_lock(&a_mutex);
			
			gettimeofday(&tv,NULL);
			double finishTime = (tv.tv_sec - burst->time.tv_sec) * 1000.0 + (tv.tv_usec - burst->time.tv_usec) / 1000.0;;//(tv.tv_sec)*1000 + (tv.tv_usec)/100;
			waitTimes[burst->t_index - 1] += finishTime;
		}
		
		pthread_mutex_unlock(&a_mutex);
	}else if(strcmp(((struct arg *)param)->algo,"SJF") == 0){
		printf("SERVIG SJF\n");
		
		pthread_mutex_lock(&a_mutex);
		
		int totalBurstCount;
		
		if(((struct arg *)param)->mode){
			totalBurstCount = ((struct arg *)param)->lineCount;
		}else{
			totalBurstCount = ((((struct arg *)param)->threadCount)*(((struct arg *)param)->burstCount));
		}
		
		int servedCount = 0;
		while(servedCount < totalBurstCount){
			while(rq == NULL){
				printf("Waiting for new cpu burst...\n");
				pthread_cond_wait(&a_cond, &a_mutex);
			}
		
		
			int minIndex = findMinBurstLengthIndex(rq);
   			burst_node_t* burst = removeBurstByIndex(&rq,minIndex);
			servedCount++;
			printf("Serving %d of %d\n",burst->burstIndex, burst->t_index);
			pthread_mutex_unlock(&a_mutex);
			usleep(burst->length*1000);
			pthread_mutex_lock(&a_mutex);
			
			gettimeofday(&tv,NULL);
			double finishTime = (tv.tv_sec - burst->time.tv_sec) * 1000.0 + (tv.tv_usec - burst->time.tv_usec) / 1000.0;;//(tv.tv_sec)*1000 + (tv.tv_usec)/100;
			waitTimes[burst->t_index - 1] += finishTime;
		}
		
		pthread_mutex_unlock(&a_mutex);
	}else if(strcmp(((struct arg *)param)->algo,"PRIO") == 0){
		printf("SERVIG PRIO\n");
		
		pthread_mutex_lock(&a_mutex);
		
		int totalBurstCount;
		
		if(((struct arg *)param)->mode){
			totalBurstCount = ((struct arg *)param)->lineCount;
		}else{
			totalBurstCount = ((((struct arg *)param)->threadCount)*(((struct arg *)param)->burstCount));
		}
		
		int servedCount = 0;
		while(servedCount < totalBurstCount){
			while(rq == NULL){
				printf("Waiting for new cpu burst...\n");
				pthread_cond_wait(&a_cond, &a_mutex);
			}
		
			int minIndex = findMinThreadPriorityIndex(rq);
   			burst_node_t* burst = removeBurstByIndex(&rq,minIndex);
			servedCount++;
			printf("Serving %d of %d\n",burst->burstIndex, burst->t_index);
			pthread_mutex_unlock(&a_mutex);
			usleep(burst->length*1000);
			pthread_mutex_lock(&a_mutex);
			
			gettimeofday(&tv,NULL);
			double finishTime = (tv.tv_sec - burst->time.tv_sec) * 1000.0 + (tv.tv_usec - burst->time.tv_usec) / 1000.0;;//(tv.tv_sec)*1000 + (tv.tv_usec)/100;
			waitTimes[burst->t_index - 1] += finishTime;
		}
		
		pthread_mutex_unlock(&a_mutex);
		
	}else if(strcmp(((struct arg *)param)->algo,"VRUNTIME") == 0){
		printf("SERVIG VRUNTIME\n");
		
		double threadRunTimes[((struct arg *)param)->threadCount];
		
		pthread_mutex_lock(&a_mutex);
		
		int totalBurstCount;
		
		if(((struct arg *)param)->mode){
			totalBurstCount = ((struct arg *)param)->lineCount;
		}else{
			totalBurstCount = ((((struct arg *)param)->threadCount)*(((struct arg *)param)->burstCount));
		}
		
		int servedCount = 0;
		while(servedCount < totalBurstCount){
			while(rq == NULL){
				printf("Waiting for new cpu burst...\n");
				pthread_cond_wait(&a_cond, &a_mutex);
			}
		
		
			int minIndex = findMinRunTimeIndex(rq,threadRunTimes,((struct arg *)param)->threadCount);
   			burst_node_t* burst = removeBurstByIndex(&rq,minIndex);
			servedCount++;
			printf("Serving %d of %d with runtime %f\n",burst->burstIndex, burst->t_index,threadRunTimes[burst->t_index - 1]);
			threadRunTimes[burst->t_index - 1] += burst->length*(0.7 + 0.3*(burst->t_index));
			pthread_mutex_unlock(&a_mutex);
			usleep(burst->length*1000);
			pthread_mutex_lock(&a_mutex);
			
			gettimeofday(&tv,NULL);
			double finishTime = (tv.tv_sec - burst->time.tv_sec) * 1000.0 + (tv.tv_usec - burst->time.tv_usec) / 1000.0;;//(tv.tv_sec)*1000 + (tv.tv_usec)/100;
			waitTimes[burst->t_index - 1] += finishTime;
		}
		
		pthread_mutex_unlock(&a_mutex);
	}
	
	pthread_exit(NULL);
}

void printUsage(){
	printf("schedule <N> <Bcount> <minB> <avgB> <minA> <avgA> <ALG>\n");
	printf("schedule <N> <ALG> -F <File>\n");
}

int main(int argc, char *argv[]){
	
	if(argc != 5){
			if(argc != 8){
			printf("%d\n",argc);
			printUsage();
			return -1;
		}
	}
	
	srand (time(NULL));
	
	if(argc == 8){
		
		if( atoi(argv[3]) == 0){
			printf("minB value  must be integer\n");
			return -1;
		}
			
		if( atoi(argv[5]) == 0){
			printf("minA value  must be integer\n");
			return -1;
		}
			
		if(atoi(argv[3]) < 100){
			printf("minB can not be less than 100 ms");
			return -1;
		}	
			
		if(atoi(argv[5]) < 100){
			printf("minA can not be less than 100 ms");
			return -1;
		}
			
		if(strcmp(argv[7],"FCFS") != 0 && strcmp(argv[7],"SJF") != 0 && strcmp(argv[7],"PRIO") != 0 && strcmp(argv[7],"VRUNTIME")){
			printf("Undefined Scheduling Algorithm\n");
			return -1;
		}
			
		if( atoi(argv[1]) <= 0){
			printf("Thread count must be integer and greater than zero\n");
			return -1;
		}
			
		if( atoi(argv[2]) <= 0){
			printf("Burst count must be integer and greater than zero\n");
			return -1;
		}
			
		if( atoi(argv[4]) <= 0){
			printf("avgB value must be integer and greater than zero\n");
			return -1;
		}
			
			
		if( atoi(argv[6]) <= 0){
			printf("avgA value must be integer and greater than zero\n");
			return -1;
		}
			
		int threadCount = atoi(argv[1]);
		int burstCount = atoi(argv[2]);
		int minA = atoi(argv[5]);
		int minB = atoi(argv[3]);
		char algo[256];
		strcpy(algo,argv[7]);
		int avgA = atoi(argv[6]);
		int avgB = atoi(argv[4]);
		waitTimes = malloc(sizeof(double)*threadCount);
		
		pthread_t tids[threadCount + 1];
		struct arg args[threadCount + 1];
		pthread_mutex_init(&a_mutex,NULL);
		int thread;
		
		for(int i = 0; i < threadCount + 1; i++){
			args[i].mode = 0;
			args[i].t_index = i + 1;
			args[i].burstCount = burstCount;
			args[i].threadCount = threadCount;
			args[i].minA = minA;
			args[i].minB = minB;
			strcpy(args[i].algo,algo);
			args[i].avgA = avgA;
			args[i].avgB = avgB;
			
			if(i + 1 == threadCount + 1){
				thread = pthread_create(&(tids[i]),NULL,scheduleCpuBurst,(void*)&(args[i]));
			}else{
				thread = pthread_create(&(tids[i]),NULL,cpuBurst,(void*)&(args[i]));
			}
		}

		
		for(int i = 0; i < threadCount + 1; i++){
			thread = pthread_join(tids[i],NULL);
		}
		
		pthread_mutex_destroy(&a_mutex);
		pthread_cond_destroy(&a_cond);
		
		for(int i = 0; i < threadCount; i++){
		 	printf("Average wait of Thread %d: %f ms.\n", i+1,waitTimes[i]/burstCount);
		}
	
	}else if(argc == 5){
		if(atoi(argv[1]) <= 0){
			printf("Thread count must be integer and greater than zero\n");
			return -1;
		}
		
		if(strcmp(argv[2],"FCFS") != 0 && strcmp(argv[2],"SJF") != 0 && strcmp(argv[2],"PRIO") != 0 && strcmp(argv[2],"VRUNTIME")){
			printf("Undefined Scheduling Algorithm\n");
			return -1;
		}
		
		if(strcmp(argv[3],"-f") != 0){
			printf("Undefined Option\n");
			return -1;
		}
		
		int threadCount = atoi(argv[1]);
		int lines[threadCount];
		int startPos[threadCount];
		memset(lines, 0, sizeof(int)*threadCount);
		int totalLines = 0;
		
		for(int i = 0; i  < threadCount; i++){
			int ch = 0;
			FILE *inputFile;
			char index[15];

   			sprintf(index, "%s-%d.txt",argv[4], i+1);
	    	inputFile = fopen(index, "r");
	    	
	    	if(inputFile == NULL){
	    		printf("Failed to open %s\n",index);
	    		return -1;
	    	}
	    	
	    	while(!feof(inputFile))
			{
			  ch = fgetc(inputFile);
			  if(ch == '\n')
			  {
				lines[i]++;
				totalLines++;
			  }
			}
		}
		
		int numberArray[totalLines*2];
		int lastIndex = 0;
		
		for(int i = 0; i < threadCount; i++){
			FILE *inputFile;
			char index[15];

   			sprintf(index, "%s-%d.txt", argv[4],i+1);
	    	inputFile = fopen(index, "r");
	    	
	    	if(inputFile == NULL){
	    		printf("Failed to open %s\n",index);
	    		return -1;
	    	}
	    	
	    	startPos[i] = lastIndex;
	    	
			for (int j = 0 ; j < lines[i]*2; j++)
			{
				if( i == 0){
					fscanf(inputFile, "%d", &numberArray[j]);
				}else{
					fscanf(inputFile, "%d", &numberArray[lastIndex]);
				}
				
				lastIndex++;
			}
		}
		
		pthread_t tids[threadCount + 1];
		struct arg args[threadCount + 1];
		pthread_mutex_init(&a_mutex,NULL);
		char algo[256];
		strcpy(algo,argv[2]);
		int thread;
		waitTimes = malloc(sizeof(double)*threadCount);
	
		for(int i = 0; i < threadCount + 1; i++){
			args[i].mode = 1;
			args[i].t_index = i + 1;
			args[i].burstCount = lines[i];
			args[i].threadCount = threadCount;
			args[i].lineCount = totalLines;
			strcpy(args[i].algo,algo);
			args[i].startPos = startPos;
			args[i].lines = lines;
			args[i].bursts_waits = numberArray;
			
			if(i + 1 == threadCount + 1){
				thread = pthread_create(&(tids[i]),NULL,scheduleCpuBurst,(void*)&(args[i]));
			}else{
				thread = pthread_create(&(tids[i]),NULL,cpuBurst,(void*)&(args[i]));
			}
		}

		
		for(int i = 0; i < threadCount + 1; i++){
			thread = pthread_join(tids[i],NULL);
		}
		
		pthread_mutex_destroy(&a_mutex);
		pthread_cond_destroy(&a_cond);
		
		for(int i = 0; i < threadCount; i++){
		 	printf("Average wait of Thread %d: %f ms.\n",i+1, waitTimes[i]/lines[i]);
		}
		
	}
	
	return 0;
}
