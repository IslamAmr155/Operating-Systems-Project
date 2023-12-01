#include "headers.h"

void clearResources(int);

int msgqid;

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    struct pcb arr[100];
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
            struct pcb process;
            process.id = id;
            process.arrivaltime = arrivalTime;
            process.runningtime = runningTime;
            process.remainingtime = runningTime;
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
        struct pcb process = arr[i];
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
        char str[100], str2[100];
        sprintf(str, "%d", algorithm);
        sprintf(str2, "%d", counter);
        char * args[] = {"./scheduler.o", str, str2, NULL};
        execv(args[0], args);
    }
    printf("Scheduler running!\n");

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

    struct msgbuff message;
    // message.mtype = algorithm;
    // sendMsg(msgqid, &message, true);
    // message.mtype = counter;
    // sendMsg(msgqid, &message, true);

    int i = 0;
    while (counter != i)
    {
        // check if the process has arrived
        while (i < counter && arr[i].arrivaltime == getClk())
        {
            // send the process to the scheduler
            message.mtype = algorithm;
            message.process = arr[i];
            sendMsg(msgqid, &message, false);
            printf("Process sent -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
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
