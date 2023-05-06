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
    printf("Worker - The string together with memory and permission: %s\n", together);

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
            printf("Worker - Child is terminating!\n");
            break;
        }
    }
    return 0;
}