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

    int resourceLim = 20;
    int resourceAsk[10];

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

    //message queue
    if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //get message queue key used in oss
    if ((msqid = msgget(msqkey, PERMS)) == -1) { perror("msgget in child"); exit(1); } //access oss message queue

    //Create a random number for how many instances of each resource the process wants and add it to an array
    int i;
    for(i=0;i<10;i++){
        resourceAsk[i] = randomNumberGenerator(resourceLim); 
    }

    //concatenate all the resources requested by the process in one string
    char *together;
    char resourceAsk_string[50];
    together = malloc(strlen(resourceAsk_string)*10 + 1 + 1);
    
    //Add random number of resources to one string
    for(i=0;i<10;i++){
        snprintf(resourceAsk_string, sizeof(resourceAsk_string), "%i", resourceAsk[i]); //convert integer to char string 
        if(i == 0){
            strcpy(together, resourceAsk_string);
        }else{
            strcat(together, resourceAsk_string);
        }
        if(i < 9){
            strcat(together, " ");
        }
    }

    //copy our new string into mtext
    strcpy(buf.strData, together);

    //for sending message to the parent
    buf.intData = getpid();
    buf.mtype = (long)getppid();

    //send our string to message queue
    if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("msgsnd from child to parent failed\n"); exit(1); }
    if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) { perror("1 failed to receive message from parent\n"); exit(1); } // receive a message from oss, but only one for our PID

    int checkResponse = atoi(buf.strData);
    
    while(checkResponse != 1){
        if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) { perror("2 failed to receive message from parent\n"); exit(1); }
        checkResponse = atoi(buf.strData);
    }
    sleep(1);

    //send our string to message queue
    strcpy(buf.strData, "0");
    buf.intData = getpid();
    buf.mtype = (long)getppid();

    //send last message to let oss know it is terminating
    if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("msgsnd from child to parent failed\n"); exit(1); }
    return 0;
}