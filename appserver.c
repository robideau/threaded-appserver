/* Compilation instructions:
 * gcc -lpthread -o appserver appserver.c Bank.c
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "Bank.h"

#define MAX_COMMAND_LENGTH 1000
#define MAX_REQUEST_ARGS 20

#define ROOT_REQ_CODE 0
#define CHECK_REQ_CODE 1
#define TRANS_REQ_CODE 2

//Functions for main and workers
void* mainThread(void*);
void* workerThread(void*);

//Request queue
struct node* root;
int queueLength = 0;
pthread_mutex_t bufferMut;
pthread_cond_t main_cv;
pthread_cond_t worker_cv;
int requestID = 1;
int lastProcessedReq = 0;

//User account and account bank
struct account {
	pthread_mutex_t lock;
	int value;
};

//Pointer to location of all user accounts
struct account* userAccounts;

//Store user-given parameters
struct serverSettings {
	int numWorkerThreads, numAccounts;
	char* outputFile;
};

//Output file
FILE* fileOut;

//Single node for request list
struct node {
	int request; //Requst codes: 0 = root (no request), 1 = check, 2 = trans
	struct requestData* reqData;
	struct node* next;
	struct node* prev;
	int accountID;
	int reqID;
	int argCount;
	int isTaken;
};

//Specific request information
struct requestData {
	int request;
	char** checkArgs;
	int transArgs[MAX_REQUEST_ARGS];
};

int main(int argc, char** argv) {
	
	//Check appropriate syntax
	if (argc != 4) {
		printf("Syntax: appserver <# of worker threads> <# of accounts> <output file>\n");
		return 0;
	}
	
	//Read argument variables, prepare output file
	struct serverSettings* settings = malloc(sizeof(struct serverSettings));
	settings->numWorkerThreads = atoi(argv[1]);
	settings->numAccounts = atoi(argv[2]);
	settings->outputFile = argv[3];
	fileOut = fopen(settings->outputFile, "w");		
	
	//Initialize accounts
	initialize_accounts(settings->numAccounts);
	
	//Create main thread and wait for termination
	pthread_mutex_init(&bufferMut, NULL);
	pthread_t main_tid;
	pthread_create(&main_tid, NULL, mainThread, (void*) settings);
	pthread_join(main_tid, NULL);
	
	//Clean up and exit
	free(settings);
	return 0;
}

void* mainThread(void* arg) {
	
	//Read user-given paramters
	struct serverSettings* settings = (struct serverSettings*) arg;	

	//Create request list
	struct node* cursor;
	root = malloc(sizeof(struct node));
	root->request = ROOT_REQ_CODE;
	root->next = 0;
	root->prev = 0;
	cursor = root;
	
	//Initialize accounts
	userAccounts = malloc(sizeof(struct account) * settings->numAccounts);
	int a = 0;
	for (a = 0; a < settings->numAccounts; a++) {
		pthread_mutex_init(&(userAccounts + a)->lock, NULL);
	}
	
	//Create worker threads
	pthread_t worker_tid[settings->numWorkerThreads];	
	int i;
	for (i = 0; i < settings->numWorkerThreads; i++) {
		pthread_create(&worker_tid[i], NULL, workerThread, (void*) &cursor);
	}
	int finished = 0;

	//Read and process user requests until END request
	while(!finished) {
			
		//Prepare to read input
		char reqInput[MAX_COMMAND_LENGTH];
		char reqDelimiter[2] = " ";

		//Move cursor forward as necessary (prepare to add new request node)
		int c;
		cursor = root;
		while (cursor->next != 0) {
			cursor = cursor->next;
		}

		//Display prompt and take user input
		printf("> ");
		fgets(reqInput, MAX_COMMAND_LENGTH, stdin);
			
		//Interpret CHECK request
		if (strncmp("CHECK ", reqInput, 6) == 0) {
			int reqAdded = 0;					
			while (reqAdded == 0) {
				char* arg;	
				if (pthread_mutex_trylock(&bufferMut) == 0) {	
					
					//Read account ID argument
					char* acctID;
					char* args[MAX_REQUEST_ARGS];
					strtok(reqInput, reqDelimiter);
					acctID = strtok(NULL, reqDelimiter);
					args[0] = acctID;
					int accIDNum = (int) strtol(acctID, NULL, 10);			
						
					//Create new request and new node - add to linked list
					struct requestData* checkData;
					checkData = malloc(sizeof(struct requestData));		
					checkData->request = CHECK_REQ_CODE;
					checkData->checkArgs = args;
					struct node* checkNode;
					checkNode = malloc(sizeof(struct node));
					checkNode->accountID = accIDNum;
					checkNode->request = CHECK_REQ_CODE;
					checkNode->reqData = checkData;
					checkNode->next = 0;
					checkNode->prev = cursor;
					checkNode->reqID = requestID++;
					checkNode->isTaken = 0;
					cursor->next = checkNode;
					cursor = cursor->next;
					pthread_mutex_unlock(&bufferMut);
					queueLength++;
					pthread_cond_broadcast(&worker_cv);
					reqAdded = 1;
					printf("< ID %d\n", checkNode->reqID);
				}
			}
		}
		
		//Interpret TRANS request
		else if (strncmp("TRANS ", reqInput, 6) == 0) {	
			int reqAdded = 0;
			while (reqAdded == 0) {
				if (pthread_mutex_trylock(&bufferMut) == 0) {
					
					//Read all arguments
					int args[MAX_REQUEST_ARGS] = {0};
					int argCount = 0;			
					strtok(reqInput, reqDelimiter);
					char* currentArg = strtok(NULL, reqDelimiter);
					while (currentArg != NULL) {
						args[argCount] = (int) strtol(currentArg, NULL, 10);
						argCount++;
						currentArg = strtok(NULL, reqDelimiter);
					}

					//Create new request and new node - add to linked list
					struct requestData* transData;
					transData = malloc(sizeof(struct requestData));
					transData->request = TRANS_REQ_CODE;	
					memcpy(&(transData->transArgs), &args, sizeof((args)));
					struct node* transNode;
					transNode = malloc(sizeof(struct node));
					transNode->request = TRANS_REQ_CODE;
					transNode->reqData = transData;
					transNode->next = 0;
					transNode->prev = cursor;
					transNode->reqID = requestID++;
					transNode->argCount = argCount;
					transNode->isTaken = 0;
					cursor->next = transNode;					
					cursor = cursor->next;
					pthread_mutex_unlock(&bufferMut);
					queueLength++;
					pthread_cond_broadcast(&worker_cv);
					reqAdded = 1;
					printf("< ID %d\n", transNode->reqID);	
				}
			}
		}
		 		
		//Interpret END request
		else if (strncmp("END", reqInput, 3) == 0) {
			finished = 1;
		}
		//Interpret other requests
		else {
			printf("Please enter a valid request.\n");
		}	

		pthread_mutex_unlock(&bufferMut);
			
	}

	//Clean up and exit	
	free(root);
	free(userAccounts);	
	return NULL;
}

void* workerThread(void* arg) {
	
	//Create new cursor specific to thread
	struct node* cursor = malloc(sizeof(struct node));
	cursor = root;
	
	//Store request arguments
	int args[MAX_REQUEST_ARGS] = {0};

	WORKERLOOP:while(1) {	
		
		//Wait until new request appears
		while (queueLength == 0) {
			pthread_cond_wait(&worker_cv, &bufferMut);
		}	
		pthread_mutex_unlock(&bufferMut);
		
		pthread_mutex_lock(&bufferMut);
		
		//Check if unfinished requests are present
		if (queueLength != 0) {
			
			//Advance cursor until untaken request is reached						
			int c;
			cursor = cursor->next;
			while (cursor->isTaken != 0) {
				cursor = cursor->next;
			}		
				
			//Grab current request
			struct node* currentReq = cursor;
			int argCount = currentReq->argCount;	
			memcpy(&args, &(currentReq->reqData->transArgs), sizeof(currentReq->reqData->transArgs));	
				
			//Wait random amount of time to avoid multiple threads simultaneously grabbing request
			usleep(10000ULL * rand() / RAND_MAX);	
			
			//Check to make sure current request still exists
			if (currentReq == NULL) {
				cursor = root;
				goto WORKERLOOP;
			}
		
			//Check to make sure TRANS requests are being completed sequentially
			if (lastProcessedReq != (currentReq->reqID - 1)) {
				cursor = root;
				goto WORKERLOOP;
			}

			//Either secure request or yield to another worker
			if (currentReq->isTaken == 0) {
				currentReq->isTaken = 1;
			} else {
				cursor = root;
				goto WORKERLOOP;
			}
		
			pthread_mutex_unlock(&bufferMut);			
	
			//Process CHECK request
			if (currentReq->request == CHECK_REQ_CODE) {	
				struct timeval startTime, endTime;
				gettimeofday(&startTime, NULL);
				int lockStatus = pthread_mutex_trylock(&(userAccounts + currentReq->accountID)->lock);
				if (lockStatus == 0) {
					queueLength--;
					fprintf(fileOut, "%d BAL %d TIME ", 
						currentReq->reqID,
						read_account(currentReq->accountID));
					gettimeofday(&endTime, NULL);
					fprintf(fileOut, "%d.%06d %d.%06d\n", 
						startTime.tv_sec,
						startTime.tv_usec, 
						endTime.tv_sec, 
						endTime.tv_usec);	
					pthread_mutex_unlock(&(userAccounts + currentReq->accountID)->lock);
				}				
			}
			
			//Process TRANS request
			if (currentReq->request == TRANS_REQ_CODE) {
				struct timeval startTime, endTime;
				gettimeofday(&startTime, NULL);
				struct requestData* transData = malloc(sizeof(struct requestData));
				transData = (struct requestData*)currentReq->reqData;
				int accIDs[MAX_REQUEST_ARGS] = {0};
				int transferAmounts[MAX_REQUEST_ARGS] = {0};
				int preScanISF = 0;
				int ISF = 0;
				int i = 0;
				int i2 = 0;

				//Determine account IDs and transfer amounts
				for (i = 0; i2 < argCount-1; i++) {
					accIDs[i] = args[i2];
					transferAmounts[i] = args[i2+1];
					i2 += 2;
				}
				
				
				//Ensure all accounts are or can be locked before proceeding	
				int j = 0;
				for (j = 0; j < argCount/2; j++) {
					int lockStatus = pthread_mutex_trylock(&(userAccounts + accIDs[j])->lock);
					if (lockStatus == 1) {
						cursor = root;
						goto WORKERLOOP;
					}
				}
						
				//Check account balances to make sure no accounts are overdrawn
				queueLength--;
				int p = 0;
				int k = 0;
				int currentAccValue = 0;
				int currentTransferAmt = 0;
				int accountBalances[MAX_REQUEST_ARGS/2] = {0};
				for (p = 0; p < argCount/2; p++) {
					currentAccValue = read_account(accIDs[p]);
					currentTransferAmt = transferAmounts[p];
					if (currentAccValue + currentTransferAmt < 0) {
						preScanISF = 1;
						fprintf(fileOut, "%d ISF %d TIME ", currentReq->reqID, accIDs[p]);
						gettimeofday(&endTime, NULL);
						fprintf(fileOut, "%d.%06d %d.%06d\n",
							startTime.tv_sec,
							startTime.tv_usec,
							endTime.tv_sec,
							endTime.tv_usec);
					}
					accountBalances[p] = currentAccValue;
				}

				//If all accounts have sufficient funds, handle transfers
				if (preScanISF != 1) {
					for (k = 0; k < argCount/2; k++) {
						write_account(accIDs[k], accountBalances[k] + transferAmounts[k]);
					}
				}

				//Unlock accounts and print output
				int m = 0;
				for (m = 0; m < argCount/2; m++) {
					pthread_mutex_unlock(&(userAccounts + accIDs[m])->lock);
				}
				if (preScanISF != 1) {
					fprintf(fileOut, "%d OK TIME ", currentReq->reqID);
					gettimeofday(&endTime, NULL);
					fprintf(fileOut, "%d.%06d %d.%06d\n",
						startTime.tv_sec,
						startTime.tv_usec,
						endTime.tv_sec,
						endTime.tv_usec);
				}
					
			}
			lastProcessedReq = currentReq->reqID;
			
		}
		pthread_mutex_unlock(&bufferMut);	
		cursor = root;		
			
	}
	//Clean up and exit
	cursor = NULL;
	free(cursor);
	
	return NULL;
}
