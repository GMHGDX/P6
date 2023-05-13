#include <stdio.h>
#include <string.h> //remedy exit warning
#include <stdlib.h> //EXIT_FAILURE
#include <sys/shm.h> //Shared memory
#include <sys/msg.h> //message queues
#include <time.h> //to create system time
#include "oss.h"

#include <sys/wait.h> //wait

int main(int argc, char *argv[]){
    msgbuffer buf;
    buf.mtype = 1;
    int msqid = 0;
    key_t msqkey;

    int i; 
    int memoryAddress;
    int randomOffset;
    int page;
    int readWrite;
    int frameNumber;

    srand(time(0) + getpid()); //seed for random number generator

    //grab sh_key from oss for shared memory
    int sh_key = atoi(argv[1]);

    //Connect to message queue
    if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //Create key using ftok() for more uniquenes
    if((msqid = msgget(msqkey, PERMS)) == -1) { perror("msgget in child"); exit(1); } //access oss message queue

    //get shared memory
    int shm_id = shmget(sh_key, sizeof(struct Table), 0666);
    if(shm_id <= 0) { fprintf(stderr,"CHILD ERROR: Failed to get shared memory, shared memory id = %i\n", shm_id); exit(1); }

    //attatch memory we allocated to our process and point pointer to it 
    struct Table *shm_ptr = (struct Table*) (shmat(shm_id, 0, 0));
    if (shm_ptr <= 0) { fprintf(stderr,"Child Shared memory attach failed\n"); exit(1); }

    //intialize page table to zero
    int pageTable[32]; //Initialize and write page table as all zeros
    for(i = 0; i < 32; i++){
        pageTable[i] = -1;
    }
    //read system time from memory
    struct Table readFromMemWorker;
    struct Table writeToMemWorker;

    readFromMemWorker = *shm_ptr;
    //Systime = readFromMemWorker.currentTime;  
    int loopAgain;
    int firstLoop = 0;
    char memory[50];
    char permission[50];
    char pageNum[50];
    char deadProc[50];
    char *together;
    int dead = 404;

    while(1){ //Check to terminate after it loops 1000 times, randomly terminate
        loopAgain = randomNumberGenerator(100);
        if(loopAgain <= 70 || firstLoop == 0){////////////////////////////////////////////////////////////////////////////////////////////////////
            firstLoop++;
            //request memory to random page
            page = randomNumberGenerator(32); //max pages a process can request is 32
            randomOffset = randomNumberGenerator(1023); //max offset is 1023
            memoryAddress = (page * 1024) + randomOffset;

            if(memoryAddress > 32000){ memoryAddress = 32000; } //if memory address exceeds 32000, keep it at 32000

            //Process chooses if it will read or write (more inclined to read)
            readWrite = randomNumberGenerator(100);
            if(readWrite < 70){
                readWrite = 1; //requesting read

            } else{
                readWrite = 2; //requesting write
            }
            
            //Convert integer to string
            snprintf(memory, sizeof(memory), "%i", memoryAddress);
            snprintf(permission, sizeof(permission), "%i", readWrite);
            snprintf(pageNum, sizeof(pageNum), "%i", page);

            //add seconds and nanoseconds together with a space in between to send as one message
            together = malloc(strlen(memory) + strlen(permission) + strlen(pageNum) + 1 + 1 + 1);
            strcpy(together, memory);
            strcat(together, " ");
            strcat(together, permission);
            strcat(together, " ");
            strcat(together, pageNum);

            //send our string to message queue
            strcpy(buf.strData, together); //copy our new string into string data buffer
            buf.intData = getpid();
            buf.mtype = (long)getppid();
            if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("W2 msgsnd from child to parent failed\n"); exit(1); }

            if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) { perror("2 failed to receive message from parent\n"); exit(1); }
            frameNumber = atoi(buf.strData);
            if(readWrite == 2 && frameNumber <= 32){  //recieve frame from OSS

                //Print contents of page table
                printf("--Page Table--\n");
                for(i = 0; i < 32; i++){
                    printf("Page%i\t", i+1);
                    pageTable[page-1] = frameNumber-1;
                    printf("%i\t", pageTable[i]);
                    printf("\n");
                }
                writeToMemWorker.currentTime = readFromMemWorker.currentTime;
                *shm_ptr = writeToMemWorker;
            }
        }
        if(loopAgain > 70){///////////////////////////////////////////////////////////////
            snprintf(deadProc, sizeof(deadProc), "%i", dead);
            strcpy(buf.strData, deadProc); //copy our new string into string data buffer
            buf.intData = getpid();
            buf.mtype = (long)getppid();
            if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("W1 msgsnd from child to parent failed\n"); exit(1); }
            break;
        }
    }
    return 0;
}