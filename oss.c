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
    int shm_id = shmget(sh_key, sizeof(double), IPC_CREAT | 0666);
    if(shm_id <= 0) {fprintf(stderr,"ERROR: Failed to get shared memory, shared memory id = %i\n", shm_id); exit(1); }

    //attatch memory we allocated to our process and point pointer to it 
    double *shm_ptr = (double*) (shmat(shm_id, NULL, 0));
    if (shm_ptr <= 0) { fprintf(stderr,"Shared memory attach failed\n"); exit(1); }

    //start the simulated system clock
    if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) { perror( "clock gettime" ); return EXIT_FAILURE; }

    //intialize values for use in while loop
    double currentTime; //time going into shared memory
    double limitReach = 0; //random time next child is forked 
    double writeToMem;
    int numofchild = 0;
    int milliSec = 0; //milliseconds used in time limit

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
        writeToMem = currentTime;
        *shm_ptr = writeToMem;

        if(numofchild < 1){ //launch only one child for now
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
                printf("OSS - Child PID: %ld ", childpid);
            }
        }
        //Connect to message queue
        if((msqkey = ftok("oss.h", 'a')) == (key_t) -1){ perror("IPC error: ftok"); exit(1); } //Create key using ftok() for more uniquenes
        if((msqid = msgget(msqkey, PERMS)) == -1) { perror("msgget in child"); exit(1); } //access oss message queue
        msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0); // IPC_NOWAIT receive a message from user_proc, but only one for our PID, dont wait for a message

        int seperate = 0;
        int memory;
        int readWrite;
        int page;

        //seperate the message by white space and assign it to seconds and nanoseconds respectively
        char * procChoice = strtok(buf.strData, " ");
        while( procChoice != NULL ) {
            seperate++;
            if(seperate == 1){
                memory = atoi(procChoice); //assign second as an integer
                procChoice = strtok(NULL, " ");
            }
            if(seperate == 2){
                readWrite = atoi(procChoice); //assign nanosecond as an integer
                procChoice = strtok(NULL, " ");
                break;
            }
            if(seperate == 3){
                page = atoi(procChoice); //assign nanosecond as an integer
                procChoice = strtok(NULL, " ");
                break;
            }
        }
         printf("OSS - I recieved the message: Page number (%i), permission: (%i), memory address (%i)\n",page,readWrite,memory);
        char one[1];
        int numOne = 1;
        strcpy(one, memory);
         buf.strData = one; //send message to process to terminate
         buf.intData = getpid();
         buf.mtype = (long)getppid();
         if(msgsnd(msqid, &buf, sizeof(msgbuffer), 0 == -1)){ perror("msgsnd from child to parent failed\n"); exit(1); }

         break;
    }
    //remove this and below
    printf("deleting memory");
    shmdt( shm_ptr ); // Detach from the shared memory segment
    shmctl( shm_id, IPC_RMID, NULL ); // Free shared memory segment shm_id 

    //Removes the message queue immediately
    if (msgctl(msqid, IPC_RMID, NULL) == -1) { perror("msgctl"); return EXIT_FAILURE; }

    fclose(fileLogging); //close the log file
    return 0;
}