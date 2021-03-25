#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

pthread_mutex_t a_mutex;
pthread_cond_t a_cond;

typedef struct burst_node{
		int t_index;
		int burstIndex;
		double length;
		double time;
		struct burst_node* nextBurst;
		pthread_mutex_t list_lock;
}burst_node_t;

struct arg{
	int t_index;
	int burstCount;
	int threadCount;
	int minA;
	int minB;
	char algo[256];
	int avgA;
	int avgB;
};


burst_node_t* rq = NULL;
	
void printList(burst_node_t* head){
	printf("------ BURST LIST ------\n");
	burst_node_t* curr = head;
	
	while(curr != NULL){
		printf("Thread index: %d\n",curr->t_index);
		printf("Burst index: %d\n",curr->burstIndex);
		printf("Burst length: %f\n",curr->length);
		printf("Creation time: %f\n",curr->time);
		printf("------------------\n");
		curr = curr->nextBurst;
	}	
}

burst_node_t * popBurst( burst_node_t** head) {
   	burst_node_t * retBurst = NULL;
    burst_node_t * next_node = NULL;

   // if (*head == NULL) {
     //   return NULL;
    //}

    next_node = (*head)->nextBurst;
    retBurst = *head;
    //free(*head);
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
    //free(temp_node);

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
    //deleteNode(q, min);
    return minIndex;
}

void addBurstToEnd(burst_node_t** head,int t_index,int burstIndex,double length, double time){
	
	//pthread_mutex_lock(&((*head)->list_lock));
	if(*head == NULL){
		//pthread_mutex_init(&((*head)->list_lock),NULL);
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
	
	//pthread_mutex_lock(&((curr)->list_lock));
	
	while(curr->nextBurst != NULL){
		curr = curr -> nextBurst;
	}
	
	curr->nextBurst = (burst_node_t*)malloc(sizeof(burst_node_t));
	curr->nextBurst->t_index = t_index;
	curr->nextBurst->burstIndex = burstIndex;
	curr->nextBurst->length = length;
	curr->nextBurst->time = time;
	curr->nextBurst->nextBurst = NULL;
	//pthread_mutex_lock(&((curr)->list_lock));
}




//int val = 0;

void* cpuBurst(void* param){
	int burstIndex = 0;
	struct timeval tv;
	double currentTime = 0.0;
	
	printf("%d\n",((struct arg *)param)->t_index);
	printf("%d\n",((struct arg *)param)->avgA);
	printf("%d\n",((struct arg *)param)->avgB);
	printf("%s\n",((struct arg *)param)->algo);
	
	pthread_mutex_lock(&a_mutex);
	
	double expoRn;
	
	while(burstIndex < ((struct arg *)param)->burstCount){
		expoRn= (double)rand() / (double)RAND_MAX;
		printf("random value: %f\n",expoRn);
		
		double arrivalTime = (((struct arg *)param)->avgA*log(expoRn))*-1;
		usleep(arrivalTime*1000);
		double burstLength = (((struct arg *)param)->avgB*log(expoRn))*-1;
		printf("burst length: %f\n", burstLength);


		gettimeofday(&tv,NULL);
		currentTime = (tv.tv_sec)*1000 + (tv.tv_usec)/100;
		addBurstToEnd(&rq,((struct arg *)param)->t_index,burstIndex,burstLength,currentTime);
		pthread_cond_signal(&a_cond);
		burstIndex++;
	}
	
	pthread_mutex_unlock(&a_mutex);
	
	pthread_exit(NULL);
}

void* scheduleCpuBurst(void* param){
	printf("SCHEDULING THIS SHIT MA NIGGA\n");
	printf("MA INDEX NIGGA: %d\n",((struct arg *)param)->t_index);
	
	if(strcmp(((struct arg *)param)->algo,"FCFS") == 0){
		printf("SERVIG FCFS\n");
		//lock
		
		
		pthread_mutex_lock(&a_mutex);
		int servedCount = 0;
		while(servedCount < ((((struct arg *)param)->threadCount)*(((struct arg *)param)->burstCount))){
			while(rq == NULL){
				printf("Waiting for new cpu burst...\n");
				pthread_cond_wait(&a_cond, &a_mutex);
			}
		
		
			burst_node_t * burst = popBurst(&rq);
			servedCount++;
			printf("Serving %d of %d\n",burst->burstIndex, burst->t_index);
			usleep(burst->length*1000);
		}
		
		
		//unlock
		pthread_mutex_unlock(&a_mutex);
	}else if(strcmp(((struct arg *)param)->algo,"SJF") == 0){
		printf("SERVIG SJF\n");
		
		pthread_mutex_lock(&a_mutex);
		int servedCount = 0;
		while(servedCount < ((((struct arg *)param)->threadCount)*(((struct arg *)param)->burstCount))){
			while(rq == NULL){
				printf("Waiting for new cpu burst...\n");
				pthread_cond_wait(&a_cond, &a_mutex);
			}
		
		
			int minIndex = findMinBurstLengthIndex(rq);
   			burst_node_t* burst = removeBurstByIndex(&rq,minIndex);
			servedCount++;
			printf("Serving %d of %d\n",burst->burstIndex, burst->t_index);
			usleep(burst->length*1000);
		}
		
		
		//unlock
		pthread_mutex_unlock(&a_mutex);
	}else if(strcmp(((struct arg *)param)->algo,"PRIO") == 0){
		printf("SERVIG PRIO\n");
	}else if(strcmp(((struct arg *)param)->algo,"VRUNTIME") == 0){
		printf("SERVIG VRUNTIME\n");
	}
	
	pthread_exit(NULL);
}

int main(){
	srand (time(NULL));
	//rq = (burst_node_t*) malloc(sizeof(burst_node_t));
	
	int threadCount = 3;
	int burstCount = 5;
	int minA = 150;
	int minB = 200;
	char algo[256] = "SJF";
	int avgA = 300;
	int avgB = 450;
	pthread_t tids[threadCount + 1];
	struct arg args[threadCount + 1];
	pthread_mutex_init(&a_mutex,NULL);
	int thread;
	
	for(int i = 0; i < threadCount + 1; i++){
		args[i].t_index = i;
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
		
		//usleep(1000000.0);
	}

	
	for(int i = 0; i < threadCount + 1; i++){
		thread = pthread_join(tids[i],NULL);
	}
	
	//pthread_mutex_destroy(&a_mutex);
	
	//rq->value = rand();
	//rq->nextNum = NULL;
	
	
	 
   	/**for(int i=0; i<999; i++){
   		addValueToEnd(rq,rand());
   	}
   	//gettimeofday(&timeval1, NULL);
   	//struct timeval timeval1;
	//struct timeval timeval2;
   	/**gettimeofday(&timeval2, NULL);
   	
   	double timeTaken = (timeval2.tv_usec - timeval1.tv_usec) / 1000.0;
   	
   	printList(head);
   	
   	printf("--- TIME TAKEN: %f ms ---\n",timeTaken);**/
   //	printList(rq);
   	
   	//printf("Removed burst length %f\n",removed->length);
   	printList(rq);
	return 0;
}