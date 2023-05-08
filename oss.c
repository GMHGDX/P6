// Name: Gabrielle Hieken
// Class: 4760 Operating Systems
// Date: 4/28/2023

#include <stdio.h>
#include <getopt.h> //Needed for optarg function
#include <stdlib.h> //EXIT_FAILURE
#include <unistd.h> //for pid_t and exec
#include <sys/types.h>
#include <time.h> //to create system time
#include <sys/shm.h> //Shared memory
#include <stdbool.h> //bool values
#include <sys/wait.h> //wait
#include <string.h> //remedy exit warning
#include <sys/msg.h> //message queues
#include "oss.h"

//msgbuffer for message queue
typedef struct queue {
    int pid;
    int resources[10];
} queue;

int main(int argc, char *argv[]){
    char* logFile = "logfile"; //logfile declaration
    FILE *fileLogging; //for the file 
    pid_t childpid = 0; //child process ID 

    srand(time(0)); //Seed the random number generator

    //variables for our system clock
    struct timespec start, stop;
    double sec;
    double nano;
    int milliLim = 500; //time limit for random forked processes in milliseconds

    //Create shared memory key and message buffer for message queue
    const int sh_key = 3147550;     
    key_t msqkey;
    int msqid;
    msgbuffer buf;

    //Create message queue
    if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //Create key using ftok() for more uniquenes
    if((msqid = msgget(msqkey, PERMS | IPC_CREAT)) == -1) { perror("Failed to create new private message queue"); exit(1); } //open an existing message queue or create a new one

    //Parse through command line options
	char opt;
    while((opt = getopt(argc, argv, "hf:")) != -1 )
    {
        switch (opt)
        {
        case 'h': //help message
			printf("To run this project: \n\n");
            printf("run the command: ./oss -n num -s num -t num\n\n");
                    printf("\t-f = name of logfile you wish to write the process table in oss.c to\n\n"); 
                    printf("If you leave out '-f' in the command line prompt it will default to 'logfile'\n\n");
                    printf("Have fun :)\n\n");

                    exit(0);
            break;
        case 'f': //logfile
            logFile = optarg; 
            break;
        default:
            printf ("Invalid option %c \n", optopt);
            return (EXIT_FAILURE);
        }
    }
    fileLogging = fopen(logFile, "w+"); //Open the log file before input begins 

    //create shared memory
    int shm_id = shmget(sh_key, sizeof(struct Table), IPC_CREAT | 0666);
    if(shm_id <= 0) {fprintf(stderr,"ERROR: Failed to get shared memory, shared memory id = %i\n", shm_id); exit(1); }

    //attatch memory we allocated to our process and point pointer to it 
    struct Table *shm_ptr = (struct Table*) (shmat(shm_id, 0, 0));
    if (shm_ptr <= 0) { fprintf(stderr,"Shared memory attach failed\n"); exit(1); }

    printf("OSS - shm_ptr is %p\n", shm_ptr);


    //start the simulated system clock
    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) { perror( "clock gettime" ); return EXIT_FAILURE; }

    //intialize page table to zero
    int pageTable[32]; //Initialize and write page table as all zeros
    int i;
    printf("--Page Table--\n");
    for(i = 0; i < 32; i++){
        printf("initPage%i\t", i+1);
        pageTable[i] = -1;
        printf("%i\t",pageTable[i]);
        printf("\n");
    }

    //Write page table to memory
    struct Table writeToMem;
    printf("OSS - Wrote to the page table in memory\n");
    for(i = 0; i < 32; i++){
        writeToMem.pageTable[i] = pageTable[i];
    }
    writeToMem.currentTime = 0;
    *shm_ptr = writeToMem;

    //intialize values for use in while loop
    double currentTime; //time going into shared memory
    double limitReach = 0; //random time next child is forked 
    int numofchild = 0;
    int milliSec = 0; //milliseconds used in time limit
    char readWriteStr[10];
    struct Table readFromMem; // To read from shared ememory

    while(1) {
        //stop simulated system clock and get seconds and nanoseconds
        if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) { perror( "clock gettime" ); return EXIT_FAILURE; }
        sec = (stop.tv_sec - start.tv_sec); 
        nano = (double)( stop.tv_nsec - start.tv_nsec);

        //if start time nanosecond is greater than stop, carry the one to get positive nanosecond
        if(start.tv_nsec > stop.tv_nsec){
            sec = (stop.tv_sec - start.tv_sec) - 1;
            nano = (double)( stop.tv_nsec - start.tv_nsec) + ((double)(1)*BILLION);
        }
        currentTime = sec + nano/BILLION;

        //Write the current time to memory for children to read
        printf("OSS - System time from memory: %lf\n", currentTime);
        readFromMem = *shm_ptr;
        writeToMem.currentTime = currentTime;
        for(i = 0; i < 32; i++){
            writeToMem.pageTable[i] = readFromMem.pageTable[i];
            printf("WRITTENP%i\t", i+1);
            printf("%i\t",writeToMem.pageTable[i]);
            printf("\n");
        }
        *shm_ptr = writeToMem;

        for(i = 0; i < 32; i++){
            writeToMem.pageTable[i] = 5;
        }

        writeToMem = *shm_ptr;

        printf("OSS - teSTUR System time from memory: %lf\n", writeToMem.currentTime);
        for(i = 0; i < 32; i++){
            writeToMem.pageTable[i];
            printf("WROTTEMN%i\t", i+1);
            printf("%i\t",writeToMem.pageTable[i]);
            printf("\n");
        }

        if(numofchild < 1){ //launch only one child for now //&& limitReach >= currentTime
            numofchild++;
            milliSec = randomNumberGenerator(milliLim); //create random number for next child to fork at 
            limitReach = sec + (double)(milliSec/1000) + (double)(nano/BILLION); //combine sec, millisec, and nanosec as one decimal to get new time to fork process

            childpid = fork(); //fork child

            if (childpid == -1) { perror("Failed to fork"); return 1; }
            if (childpid == 0){  //send shared memory key to user_proc for children to use 
                char sh_key_string[50];
                snprintf(sh_key_string, sizeof(sh_key_string), "%i", sh_key);

                char *args[] = {"user_proc", sh_key_string, NULL};
                execvp("./user_proc", args); //exec function to send children to user_proc along with our shared memory key

                return 1;
            }
            if(childpid != 0 ){               
                printf("OSS - Child PID: %ld\n", childpid);
            }
        }

        msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0); // IPC_NOWAIT receive a message from user_proc, but only one for our PID, dont wait for a message
        printf("OSS - message recived is : %s\n", buf.strData);
        int seperate = 0;
        int memory;
        int readWrite;
        int page;

        // Master: P2 requesting read of address 25237 at time xxx:xxx
        // Master: Address 25237 in frame 13, giving data to P2 at time xxx:xxx
        // Master: P5 requesting write of address 12345 at time xxx:xxx
        // Master: Address 12345 in frame 203, writing data to frame at time xxx:xxx
        // Master: P2 requesting write of address 03456 at time xxx:xxx
        // Master: Address 12345 is not in a frame, pagefault
        // Master: Clearing frame 107 and swapping in p2 page 3
        // Master: Dirty bit of frame 107 set, adding additional time to the clock
        // Master: Indicating to P2 that write has happened to address 03456
        // Current memory layout at time xxx:xxx is:
        // Occupied DirtyBit HeadOfFIFO
        // Frame 0: No 0
        // Frame 1: Yes 1
        // Frame 2: Yes 0 *
        // Frame 3: Yes 1

        //seperate the message by white space and assign it ot page number, memory address, and read/write
        char * procChoice = strtok(buf.strData, " ");
        while( procChoice != NULL ) {
            printf("message is now: %s\n", procChoice);
            seperate++;
            if(seperate == 1){
                memory = atoi(procChoice); //assign second as an integer
                procChoice = strtok(NULL, " ");
            }
            if(seperate == 2){
                readWrite = atoi(procChoice); //assign nanosecond as an integer
                procChoice = strtok(NULL, " ");
            }
            if(seperate == 3){
                page = atoi(procChoice); //assign nanosecond as an integer
                procChoice = strtok(NULL, " ");
                break;
            }   
        }
        if(readWrite == 1){ //assign read string
            strcpy(readWriteStr, "read");
        }
        if(readWrite == 2){ //assign write string
            strcpy(readWriteStr, "write");
        }
        printf("OSS: PID %ld requesting %s of address %i at time %lf\n",childpid, readWriteStr, memory, currentTime);

        printf("OSS - I recieved the message: Page number (%i), permission: (%i), memory address (%i)\n", page, readWrite, memory);

        strcpy(buf.strData, "1");
        buf.intData = getpid();
        buf.mtype = childpid;
        printf("OSS - The buf.str data: %s\n", buf.strData);
        if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("msgsnd from child to parent failed\n"); exit(1); }
        sleep(1);
        
        break;
    }
    //remove this and below
    printf("deleting memory\n");
    shmdt( shm_ptr ); // Detach from the shared memory segment
    shmctl( shm_id, IPC_RMID, NULL ); // Free shared memory segment shm_id 

    //Removes the message queue immediately
    if (msgctl(msqid, IPC_RMID, NULL) == -1) { perror("msgctl"); return EXIT_FAILURE; }

    fclose(fileLogging); //close the log file
    return 0;
}