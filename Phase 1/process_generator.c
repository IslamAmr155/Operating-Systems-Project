#include "headers.h"

void clearResources(int);

int msgqid;

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    struct processData arr[100];
    int counter = 0;

    // 1. Read the input files.
    printf("Opening file...\n");
    FILE * pFile = fopen("processes.txt", "r");
    if (pFile == NULL)
    {
        perror("Error in opening file");
        exit(EXIT_FAILURE);
    }
    printf("File opened!\n");
    // check if end of file is reached
    while(!feof(pFile))
    {
        printf("Reading line %d\n", counter + 1);
        // read first character to check if it is '#'
        char c = fgetc(pFile);
        if (c != '#')
        {
            // return the file pointer to the beginning of the line
            fseek(pFile, -1, SEEK_CUR);
            // read the id number
            int id;
            fscanf(pFile,"%d", &id);
            // read the arrival time
            int arrivalTime;
            fscanf(pFile,"%d", &arrivalTime);
            // read the running time
            int runningTime;
            fscanf(pFile,"%d", &runningTime);
            // read the priority
            int priority;
            fscanf(pFile, "%d", &priority);
            // create a process
            struct processData process;
            process.id = id;
            process.arrivaltime = arrivalTime;
            process.runningtime = runningTime;
            process.priority = priority;
            // add the process to the queue
            arr[counter] = process;
            counter++;
        }
        else
        {   
            char temp[256];
            fgets(temp, 256, pFile);
        }
    }
    printf("Read contents successfully!\n");
    fclose(pFile);
    printf("File closed!\n");
    // THIS IS FOR US
    // PRINTING THE ARRAY CONTENTS
    printf("Reading array contents...\n");
    for(int i = 0; i < counter; i++)
    {
        struct processData process = arr[i];
        printf("id: %d, arr: %d, run: %d, pri: %d\n", process.id, process.arrivaltime, process.runningtime, process.priority);
    }
    printf("Contents read successfully!\n");
    
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int algorithm;
    printf("Please enter the number of the desired scheduling algorithm:\n");
    printf("1. HPF\n");
    printf("2. SRTN\n");
    printf("3. RR\n");
    scanf("%d", &algorithm);
    // 3. Initiate and create the scheduler and clock processes.
    // int clock = execl("./clk.o", NULL);
    int schedulerPid;
    int clkPid = fork();
    if (clkPid == -1)
    {
        perror("Error in fork");
        exit(EXIT_FAILURE);
    }
    else if (clkPid == 0)
    {
        printf("Forked Clock!\n");
        char * args[] = {"./clk.o", NULL};
        execv(args[0], args);
    }
    else
    {
        printf("Clock running!\n");
        // run the scheduler.o file using exec
        schedulerPid = fork();
        if (schedulerPid == -1)
        {
            perror("Error in fork");
            exit(EXIT_FAILURE);
        }
        else if (schedulerPid == 0)
        {
            printf("Forked Scheduler!\n");
            char * args[] = {"./scheduler.o", NULL};
            execv(args[0], args);
        }
        // else
        // {
            printf("Scheduler running!\n");
        //     // wait for the scheduler to finish
        //     waitpid(schedulerPid, NULL, 0);
        // }
        // // wait for the clock to finish
        // waitpid(clkPid, NULL, 0);
    }
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // DONE ABOVE IN STRUCT processData
    // 6. Send the information to the scheduler at the appropriate time.
    // We are assuming that the processes will be sent in a message queue.
    key_t key = ftok("keyfile", 65);
    msgqid = msgget(key, 0666 | IPC_CREAT);
    if(msgqid == -1)
    {
        perror("Error in creating message queue");
        exit(-1);
    }
    int i = 0;
    while (counter != i)
    {
        // check if the process has arrived
        while (i < counter && arr[i].arrivaltime == getClk())
        {
            // send the process to the scheduler
            struct msgbuff message;
            message.mtype = 1;      // HAL HATEFRE2 FE 7AGA??
            message.process = arr[i];
            int send_val = msgsnd(msgqid, &message, sizeof(message.process), !IPC_NOWAIT);
            if(send_val == -1)
            {
                perror("Error in sending message");
                exit(-1);
            }
            i++;
        }
    }
    waitpid(schedulerPid, NULL, 0);
    waitpid(clkPid, NULL, 0);
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
    signal(SIGINT, clearResources);
    msgctl(msgqid, IPC_RMID, (struct msqid_ds *)0);
    kill(getpid(), SIGKILL);
}
