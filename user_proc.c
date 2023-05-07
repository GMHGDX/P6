#include <stdio.h>
#include <string.h> //remedy exit warning
#include <stdlib.h> //EXIT_FAILURE
#include <sys/shm.h> //Shared memory
#include <sys/msg.h> //message queues
#include <time.h> //to create system time
#include "oss.h"

int main(int argc, char *argv[]){
    msgbuffer buf;
    buf.mtype = 1;
    int msqid = 0;
    key_t msqkey;

    int memoryAddress;
    int randomOffset;
    int page;
    int readWrite;
    int checkResponse;
    int pageTable[32][1]; //Initialize page table

    //Initialize empty page table (all zeros)
    int i, j;
    for(i = 0; i < 18; i++){
        for(j = 0; j < 10; j++){
           pageTable[i][j] = 0;
        }
    }
    //Print empty page table
    printf("--Page Table--\n");
    for(i = 0; i < 32; i++){
        printf("P%i\t", i);
        for(j = 0; j < 1; j++){
            printf("%i\t",pageTable[i][j]);
        }
        printf("\n");
    }

    srand(time(0) + getpid()); //seed for random number generator

    //grab sh_key from oss for shared memory
    int sh_key = atoi(argv[1]);

    //Connect to message queue
    if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //Create key using ftok() for more uniquenes
    if((msqid = msgget(msqkey, PERMS)) == -1) { perror("msgget in child"); exit(1); } //access oss message queue

    //get shared memory
    int shm_id = shmget(sh_key, sizeof(double), 0666);
    if(shm_id <= 0) { fprintf(stderr,"CHILD ERROR: Failed to get shared memory, shared memory id = %i\n", shm_id); exit(1); }

    //attatch memory we allocated to our process and point pointer to it 
    double *shm_ptr = (double*) (shmat(shm_id, 0, 0));
    if (shm_ptr <= 0) { fprintf(stderr,"Child Shared memory attach failed\n"); exit(1); }

    //read time from memory
    double readFromMem;
    readFromMem = *shm_ptr;

    printf("Worker - Time from memory: %lf\n", readFromMem);

    //request memory to random page
    page = randomNumberGenerator(32); //max pages a process can request is 32
    randomOffset = randomNumberGenerator(1023); //max offset is 1023
    memoryAddress = (page * 1024) + randomOffset;

    //Add it to the page table

    if(memoryAddress > 32000){
        memoryAddress == 32000;
    }

    printf("Worker - This is your page number: %i. This is your memory address: %i\n", page, memoryAddress);

    //Process chooses if it will read or write (more inclined to read)
    readWrite = randomNumberGenerator(100);
    if(readWrite < 70){
        readWrite = 1; //requesting read
        printf("Worker - The process is requesting read\n");

    } else{
        readWrite = 2; //requesting write
        printf("Worker - The process is requesting write\n");
    }
    
    //Convert integer to string
    char memory[50];
    char permission[50];
    char pageNum[50];
    snprintf(memory, sizeof(memory), "%i", memoryAddress);
    snprintf(permission, sizeof(permission), "%i", readWrite);
    snprintf(pageNum, sizeof(pageNum), "%i", page);

    //add seconds and nanoseconds together with a space in between to send as one message
    char *together;
    together = malloc(strlen(memory) + strlen(permission) + strlen(pageNum) + 1 + 1 + 1);
    strcpy(together, memory);
    strcat(together, " ");
    strcat(together, permission);
    strcat(together, " ");
    strcat(together, pageNum);
    printf("Worker - The string together with memory, permission, and page number: %s\n", together);

    //send our string to message queue
    strcpy(buf.strData, together); //copy our new string into string data buffer
    buf.intData = getpid();
    buf.mtype = (long)getppid();
    if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("msgsnd from child to parent failed\n"); exit(1); }

    //Recieve message back from oss on when child should terminate
    while(1){
        if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) 
        {
            perror("2 failed to receive message from parent\n"); 
            exit(1);
        }
        checkResponse = atoi(buf.strData);

        if(checkResponse == 1){
            //printf("Worker - Child is terminating!\n");
            break;
        }
    }
    int loopAmount = 0;
    int loopAgain;
    while(loopAmount < 1001){ //Check to terminate after it loops 1000 times, randomly terminates if 1000 passes
        loopAmount++;
        if(loopAmount == 1001){
            loopAgain = randomNumberGenerator(2);
            if(loopAgain == 1){
                loopAmount = 0;
            }
            if(loopAgain == 2){
                printf("Worker - Child is terminating!\n");
                break;
            }
        }
    }
    return 0;
}