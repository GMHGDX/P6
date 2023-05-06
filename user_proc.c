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
    int child = 0;
    int checkResponse;

    srand(time(0) + getpid()); //seed for random number generator

    //grab sh_key from oss for shared memory
    int sh_key = atoi(argv[1]);

    //get shared memory
    int shm_id = shmget(sh_key, sizeof(double), 0666);
    if(shm_id <= 0) { fprintf(stderr,"CHILD ERROR: Failed to get shared memory, shared memory id = %i\n", shm_id); exit(1); }

    //attatch memory we allocated to our process and point pointer to it 
    double *shm_ptr = (double*) (shmat(shm_id, 0, 0));
    if (shm_ptr <= 0) { fprintf(stderr,"Child Shared memory attach failed\n"); exit(1); }

    //read time from memory
    double readFromMem;
    readFromMem = *shm_ptr;

    //connect to message queue
    if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //get message queue key used in oss
    if ((msqid = msgget(msqkey, PERMS)) == -1) { perror("msgget in child"); exit(1); } //access oss message queue

    //request memory to random page
    page = randomNumberGenerator(32); //max pages a process can request is 32
    randomOffset = randomNumberGenerator(1023); //max offset is 1023
    memoryAddress = (page * 1024) + randomOffset;

    printf("This is your page number: %i. This is your memory address: %i", randomPage, memoryAddress);

    //Process chooses if it will read or write (more inclined to read)
    readWrite = randomNumberGenerator(100);
    if(readWrite < 70){
        readWrite = 1; //requesting read
        printf("the process is requesting read");

    } else{
        readWrite = 2; //requesting write
        printf("the process is requesting write");
    }
    
    //Convert integer to string
    char memory[50];
    char permission[50];
    snprintf(memory, sizeof(memory), "%i", memoryAddress);
    snprintf(permission, sizeof(permission), "%i", readWrite);

    //add seconds and nanoseconds together with a space in between to send as one message
    char *together;
    together = malloc(strlen(memory) + strlen(permission) + 1 + 1);
    strcpy(together, memory);
    strcat(together, " ");
    strcat(together, permission);
    printf("The string together with memory and permission: %s", together);

    //send our string to message queue
    strcpy(buf.strData, together); //copy our new string into string data buffer
    buf.intData = getpid();
    buf.mtype = (long)getppid();
    if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("msgsnd from child to parent failed\n"); exit(1); }

    //Recieve message back from oss on when child should terminate
    while(child == 0){
        if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) 
        {
             perror("2 failed to receive message from parent\n"); 
             exit(1);
        }
        checkResponse = atoi(buf.strData);

        if(checkResponse == 1){
            child++;
            printf("Child is terminating!");
        }
    }
    return 0;
}