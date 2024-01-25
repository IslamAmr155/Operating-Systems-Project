#include "headers.h"

void clearResources(int);

int msgqid = -1;
struct pcb* arr;

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // CC_List processes;
    int counter = 0;
    printf("YOU MAY START...\n");

    // 1. Read the input files.
    FILE * pFile = fopen("processes.txt", "r");
    if (pFile == NULL)
    {
        perror("Error in opening file");
        exit(EXIT_FAILURE);
    }

    // Determine no. of processes
    int count = 0;
    while(!feof(pFile))
    {
        for (char c = getc(pFile); c != EOF; c = getc(pFile)){
            if (c == '\n'){ // Increment count if this character is newline
                count = count + 1;
                printf("Count = %d\n", count);
            }
        }
    }
    arr = malloc((count) * sizeof(struct pcb));
    printf("before seek\n");
    fseek(pFile, 0, SEEK_SET);
    printf("after seek\n");
    while(!feof(pFile))
    {
        char c = fgetc(pFile);
        if (c == '#' || (int)c == -1){
            fscanf(pFile, "%*[^\n]");
            continue;
        }
        printf("Reading line %d\n", counter + 1);
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
        // read the memsize
        int memsize;
        fscanf(pFile, "%d", &memsize);

        fgetc(pFile);

        // create a process
        struct pcb process;
        process.pid = -1;
        process.id = id;
        process.arrivaltime = arrivalTime;
        process.runningtime = runningTime;
        process.remainingtime = runningTime;
        process.priority = priority;
        process.memsize = memsize;
        // add the process to the queue
        arr[counter] = process;
        counter++;
    }
    fclose(pFile);

    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int algorithm, quantum = 0;
    printf("Please enter the number of the desired scheduling algorithm:\n");
    printf("1. HPF\n");
    printf("2. SRTN\n");
    printf("3. RR\n");
    scanf("%d", &algorithm);
    if(algorithm == 3)
    {
        printf("Please enter the quantum:\n");
        scanf("%d", &quantum);
    }

    msgqid = msgget(GENKEY, 0666 | IPC_CREAT);
    if(msgqid == -1)
    {
        perror("Error in creating message queue");
        exit(-1);
    }

    struct msgbuff message;

    int i = 1;
    while (i <= counter && arr[i - 1].arrivaltime == 0)
    {
        // send the process to the scheduler
        message.mtype = i;
        message.process = arr[i - 1];
        sendMsg(msgqid, &message, false);
        printf("Process sent -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority,0);
        i++;
    }

    // 3. Initiate and create the scheduler and clock processes.
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
        char * args[] = {"./clk.out", NULL};
        execv(args[0], args);
    }
    printf("Clock running!\n");
    schedulerPid = fork();
    if (schedulerPid == -1)
    {
        perror("Error in fork");
        exit(EXIT_FAILURE);
    }
    else if (schedulerPid == 0)
    {
        printf("Forked Scheduler!\n");
        char str[100], str2[100], str3[100];
        sprintf(str, "%d", algorithm);
        sprintf(str2, "%d", counter);
        sprintf(str3, "%d", quantum);
        char * args[] = {"./scheduler.out", str, str2, str3, NULL};
        execv(args[0], args);
    }

    // 4. Use this function after creating the clock process to initialize clock
    initClk();

    // To get time use this
    int x = getClk();

    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // DONE ABOVE IN STRUCT pcb

    // 6. Send the information to the scheduler at the appropriate time.
    // We are assuming that the processes will be sent in a message queue.
    while (counter+1 != i)
    {
        // check if the process has arrived
        while (i <= counter && arr[i - 1].arrivaltime == getClk())
        {
            // send the process to the scheduler
            message.mtype = i;
            message.process = arr[i - 1];
            sendMsg(msgqid, &message, false);
            printf("Process sent -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d, P_memsize: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, message.process.memsize, getClk());
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
    free(arr);
    if(msgqid != -1)
        msgctl(msgqid, IPC_RMID, (struct msqid_ds *)0);
    int msgqProcess = msgget(MSGKEY, 0666);
    if(msgqProcess != -1)
        msgctl(msgqProcess, IPC_RMID, (struct msqid_ds *)0);
    // delete semaphores
    int semclk = semget(SEMKEY, 1, 0666);
    if(semclk != -1)
        semctl(semclk, 0, IPC_RMID, NULL);
    kill(getpid(), SIGKILL);
}
