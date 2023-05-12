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
    int frameTable[256][4]; //Initialize frame table
    int frame;
    int addressInFrame;

    //Initialize empty frame table (all zeros)
    int i, j;
    printf("\t\tOccupied\tDirtyBit\tpage\n");
    for(i = 0; i < 256; i++){
        printf("Frame %i:\t", i);
        for(j = 0; j < 4; j++){
            frameTable[i][j] = 0;
            if(j!=3){
                printf("%i\t\t",frameTable[i][j]);
            }
        }
        printf("\n");
    }

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

    //start the simulated system clock
    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) { perror( "clock gettime" ); return EXIT_FAILURE; }

    //Write time tom memory
    struct Table writeToMem;

    //intialize values for use in while loop
    double currentTime; //time going into shared memory
    double limitReach = 0; //random time next child is forked 
    int numofchild = 0;
    int milliSec = 0; //milliseconds used in time limit
    char readWriteStr[10];
    int seperate = 0;
    int memoryAddress;
    int readWrite;
    int page;
    char * procChoice;
    int inFrame = 0;
    int headpointer = 0;
    char frameString[50];
    int dead;
    int processRunning = 0;

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
        writeToMem.currentTime = currentTime;
        *shm_ptr = writeToMem;

        if(numofchild <= 10 && processRunning < 18){ //launch only one child for now //&& limitReach >= currentTime
            numofchild++;
            processRunning++;
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
        }
        msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0); // IPC_NOWAIT receive a message from user_proc, but only one for our PID, dont wait for a message
        dead = atoi(buf.strData);
        if(dead == 404){
            processRunning--;
        }else{
            //Seperate the message by white space and assign it ot page number, memory address, and read/write
            procChoice = strtok(buf.strData, " ");
            seperate = 0;
            while( procChoice != NULL ) {
                seperate++;
                if(seperate == 1){
                    memoryAddress = atoi(procChoice); //Assign second as an integer
                    procChoice = strtok(NULL, " ");
                }
                if(seperate == 2){
                    readWrite = atoi(procChoice); //Assign nanosecond as an integer
                    procChoice = strtok(NULL, " ");
                }
                if(seperate == 3){
                    page = atoi(procChoice); //Assign nanosecond as an integer
                    procChoice = strtok(NULL, " ");
                    break;
                }   
            }
            if(readWrite == 1){ //Assign read string
                strcpy(readWriteStr, "read");
            }
            if(readWrite == 2){ //Assign write string
                strcpy(readWriteStr, "write");
            }
            printf("OSS: PID %d requesting %s of address %i at time %lf\n",childpid, readWriteStr, memoryAddress, currentTime);
            printf("OSS - I recieved the message: Page number (%i), permission: (%i), memory address (%i)\n", page, readWrite, memoryAddress);

            //Read/write from/to frame table----------------------------------------------------------------------------------------------------
            inFrame = 0;
            if(readWrite == 1){ //process is requesting to read
                for(i = 0; i < 256; i++){//Search frame Table for address
                    if(frameTable[i][3] == memoryAddress){
                        frame = i; 
                        inFrame++; 
                        printf("OSS: Address %i in frame %i, giving data to PID %d at time %lf\n", memoryAddress, frame, childpid, currentTime); 
                        frameTable[i][0] = 1; // set occupied to 1/yes  
                    }
                }  
                if(inFrame == 0){
                    printf("OSS: Address %i is not in a frame, giving data to PID %d at time %lf\n", memoryAddress, childpid, currentTime);
                }
                //Send message back to user process
                strcpy(buf.strData, "1");
                buf.intData = getpid();
                buf.mtype = childpid;
                if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("1 msgsnd from child to parent failed\n"); exit(1); } 
            }
            if(readWrite == 2){ //Process is requesting to write---------------------------------------------------------------------------------
                addressInFrame = 0;
                for(i = 0; i < 256; i++){//Search frame Table for address
                    if(frameTable[i][3] == memoryAddress){
                        addressInFrame = memoryAddress;
                        frame = i; 
                        frameTable[i][0] = 1; // set occupied to 1/yes
                        frameTable[i][1] = 1; // set dirtybit
                        frameTable[i][2] = page; //set new page
                        break;     
                    }
                }
                if(memoryAddress == addressInFrame){ //The address is in frame/////////////////////////////////////////////////////////
                    printf("OSS: Address %i in frame %i, writing data to frame at time %lf\n", memoryAddress, frame, currentTime);
                    printf("\t\tOccupied\tDirtyBit\tpage\n");
                    for(i = 0; i < 256; i++){
                        printf("Frame %i:\t", i);
                        for(j = 0; j < 4; j++){
                            if(j == 0){
                                if(frameTable[i][0] == 1){
                                    printf("Yes-");
                                }if(frameTable[i][0] == 0){
                                    printf("no-");
                                }
                            }
                            if(j!=3){
                                printf("%i\t\t",frameTable[i][j]);
                            }
                        }
                        printf("\n");
                    }
                }
                else{ //The address is not in frame////////////////////////////////////////////////////////////////////////////////////
                    printf("OSS: Address %i is not in a frame, pageFault. Searching with head where to put the new address\n", memoryAddress);
                    
                    frameTable[headpointer][3] = memoryAddress; //memory address
                    frameTable[headpointer][2] = page; //page number
                    frameTable[headpointer][1] = 1; //dirty bit
                    frameTable[headpointer][0] = 1; //unoccupied
                    headpointer++;
                    if(headpointer >= 256){
                            headpointer = 0;//Return headpointer to the top of the frameif it goes past 256
                    }          

                    printf("OSS: Clearing frame %i and swapping in PIDs %d page %i\n", headpointer ,childpid, page);
                    printf("OSS: Indicating to PID %d that write has happened to address %i\n", childpid, memoryAddress);

                    //print the frame table
                    printf("\t\tOccupied\tDirtyBit\tpage\n");
                    for(i = 0; i < 256; i++){
                        printf("Frame %i:\t", i);
                        for(j = 0; j < 4; j++){
                            if(j == 0){
                                if(frameTable[i][0] == 1){
                                    printf("Yes-");
                                }if(frameTable[i][0] == 0){
                                    printf("no-");
                                }
                            }
                            if(j!=3){
                                printf("%i\t\t",frameTable[i][j]);
                            }
                        }
                        printf("\n");
                    }
                    //Send message back to user process
                    snprintf(frameString, sizeof(frameString), "%i", headpointer);
                    strcpy(buf.strData, frameString);
                    buf.intData = getpid();
                    buf.mtype = childpid;
                    if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("2 msgsnd from child to parent failed\n"); exit(1); }
                        
                    printf("OSS: Head is now at frame %i\n", headpointer); //print head of process
                    printf("OSS: Indicating to %d that write has happened to address %i\n", childpid, memoryAddress);
                }
            }
        }
        if(currentTime >= 3 || (processRunning == 0 && numofchild == 10)){
            break;
        } 
    }

    printf("deleting memory\n");
    shmdt( shm_ptr ); // Detach from the shared memory segment
    shmctl( shm_id, IPC_RMID, NULL ); // Free shared memory segment shm_id 

    if (msgctl(msqid, IPC_RMID, NULL) == -1) { perror("msgctl"); return EXIT_FAILURE; } //Removes the message queue immediately

    fclose(fileLogging); //close the log file
    return 0;
}