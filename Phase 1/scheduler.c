#include "headers.h"
#include <math.h>

int capacity, msgq_id, *shm_addr, msgqIdProcess, quanta, Qtemp = 0, sum_waiting = 0, sum_running = 0;
float sum_wta = 0, *wta;
uint64_t current = 0, received_processes = 0;
struct msgbuff message;
FILE *pFile, *mFile;
struct pcb **processTable;
struct node **mem;
struct node *BTree;
CC_PQueue *waiting_list;

int calculateCapacity(struct node* root)
{
    return root->end - root->start + 1;
}

struct node* findBest(struct node *root, int size)
{
    if(!root || (!root->left && !root->right && !root->isHole) || calculateCapacity(root) < size)
        return (void *)0;
    
    printf("Finding best in %d...%d\n", root->start, root->end);
    struct node *left = findBest(root->left, size);
    struct node *right = findBest(root->right, size);

    if (!root->left && !root->right)
        return root;
    // if(!left && !right)
    //     return root;
    if(!left)
        return right;
    if(!right)
        return left;
    int capacity_left = calculateCapacity(left);
    int capacity_right = calculateCapacity(right);
    if(capacity_left <= capacity_right)
        return left;
    else
        return right;
}

struct node* createNode(int start, int end, struct node *parent)
{
    struct node *root = (struct node*)malloc(sizeof(struct node));
    root->start = start;
    root->end = end;
    root->left = (void *)0;
    root->right = (void *)0;
    root->parent = parent;
    root->isHole = true;
}

void splitMem(struct node *root)
{
    printf("Splitting %d...%d\n", root->start, root->end);
    int mid = root->start + (root->end - root->start)/2;
    root->left = createNode(root->start, mid, root);
    root->right = createNode(mid+1, root->end, root);
}

void mergeMem(struct node *root)
{
    if(root && root->left && root->left->isHole && root->right && root->right->isHole)
    {
        printf("Merging %d...%d\n", root->start, root->end);
        free(root->left);
        free(root->right);
        root->left = (void *)0;
        root->right = (void *)0;
        root->isHole = true;
        mergeMem(root->parent);
    }
}

void printMem(struct node* root){
    if(root && root->left == (void *)0 && root->right == (void *)0){
        printf("%d...%d %d ", root->start, root->end, root->isHole);
        return;
    }
    printMem(root->left);
    printMem(root->right);
}

struct node* allocate(short memsize)
{
    int approx = 1;
    while (approx < memsize)
        approx *= 2;
    
    printMem(BTree);
    struct node *best = findBest(BTree, approx);
    if(!best)
        return (void *)0; 
    while(calculateCapacity(best) != approx)
    {
        splitMem(best);
        printMem(BTree);
        best = findBest(best, approx);
        printf("Best = %d...%d\n", best->start, best->end);
    }
    printMem(BTree);
    best->isHole = false;
    struct node* temp = best->parent;
    temp->isHole = false;
    while (temp->parent)
    {
        temp = temp->parent;
        temp->isHole = false;
    }
    return best;
}

int compare_priority(const void *a, const void *b)
{
    struct pcb **p1 = (struct pcb **)a;
    struct pcb **p2 = (struct pcb **)b;
    if((*p1)->priority != (*p2)->priority)
        return (*p1)->priority < (*p2)->priority ? 1 : -1;
    else
        return (*p1)->id < (*p2)->id ? 1 : -1;
}

int compare_remaining_time(const void *a, const void *b)
{
    struct pcb **p1 = (struct pcb **)a;
    struct pcb **p2 = (struct pcb **)b;
    if((*p1)->remainingtime != (*p2)->remainingtime)
        return (*p1)->remainingtime < (*p2)->remainingtime ? 1 : -1;
    else
        return (*p1)->id < (*p2)->id ? 1 : -1;
}

int compare_memory(const void *a, const void *b)
{
    struct pcb **p1 = (struct pcb **)a;
    struct pcb **p2 = (struct pcb **)b;
    if ((*p1)->memsize != (*p2)->memsize)
        return (*p1)->memsize < (*p2)->memsize ? 1 : -1;
    else
        return (*p1)->id < (*p2)->id ? 1 : -1;
}

int getWait()
{
    return getClk() - processTable[current]->arrivaltime - (processTable[current]->runningtime - processTable[current]->remainingtime);
}

void logging(char *event) 
{
    fprintf(pFile, "At time %d process %d %s arr %d total %d remain %d wait %d", getClk(), processTable[current]->id, event, processTable[current]->arrivaltime, processTable[current]->runningtime, processTable[current]->remainingtime, getWait());            
    if (event != "finished")
        fprintf(pFile, "\n");
    else
        fprintf(pFile, " TA %d WTA %.2f\n", getClk() - processTable[current]->arrivaltime, (float)(getClk() - processTable[current]->arrivaltime) / processTable[current]->runningtime);
        
}

// Synchronizing the scheduler with the clock; if the process is over, it will be terminated
void sync()
{
    // If no process is running, return
    if(!current)
        return;

    // Synchronizing the scheduler with each process, so that each process receives its specific message
    message.mtype = processTable[current]->pid;
    message.process = *processTable[current];
    sendMsg(msgqIdProcess, &message, true);
    receiveSpecificMsg(msgqIdProcess, &message, true, getpid());
    *processTable[current] = message.process;
}

bool checkFinished()
{
    // If the process is over, terminate it
    if(current && !processTable[current]->remainingtime)
    {
        int status;
        int finished = wait(&status);
        status = status >> 8;
        printf("Process %d is finished!\n", status);
        logging("finished");
        Qtemp = 0;
        sum_waiting += getWait();
        printf("accumulating %d, sum_waiting = %d\n", getWait(), sum_waiting);
        sum_wta += (float)(getClk() - processTable[current]->arrivaltime) / processTable[current]->runningtime;
        sum_running += processTable[current]->runningtime;
        wta[current] = (float)(getClk() - processTable[current]->arrivaltime) / processTable[current]->runningtime;
        printf("accumulating %0.2f, sum_wta = %0.2f\n", wta[current], sum_wta);
        // Delete the process from the process table
        fprintf(mFile, "At time %d freed %d bytes from process %d from %d to %d\n", getClk(), processTable[current]->memsize, processTable[current]->id, mem[current]->start, mem[current]->end);
        free(processTable[current]);
        mem[current]->isHole = true;
        printf("memory freed : start:%d end:%d\n", mem[current]->start, mem[current]->end);
        mergeMem(mem[current]->parent);
        processTable[current] = NULL;
        current = 0;
        return true;
    }
    return false;
}

int receive()
{
    // receive all incoming processes
    int count = 0;
    while(receiveMsg(msgq_id, &message, false) != -1){
        message.process.state = "waiting";
        printf("Process received -> P_ID = %d, P_arr: %d, P_run: %d, P_pri: %d at %d\n", message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, getClk());
        received_processes++;
        processTable[received_processes] = malloc(sizeof(struct pcb));
        *processTable[received_processes] = message.process;
        printf("Process ID: %d is received\n", processTable[received_processes]->id);
        count++;
    }
    return count;
}

void run() 
{
    // If there is no pid saved in the process, it did not run before
    processTable[current]->state = "running";
    if(processTable[current]->pid == -1)
    {
        logging("started");
        processTable[current]->pid = fork();
        if (processTable[current]->pid == 0)
        {
            char str[100];
            printf("forking process %d\n", processTable[current]->id);
            sprintf(str, "%d", processTable[current]->id);
            char *args[] = {"./process.out", str, NULL};
            execv(args[0], args);
        }
        
    } 
    else 
    {
        logging("resumed");
    }
}

void hpf(CC_PQueue *cc_pq)
{
    int count_received;
    while(received_processes < capacity || cc_pq->size || current != 0)
    {
        // Receiving new processes
        count_received = receive();

        // Enqueueing the received processes
        while(count_received--)
        {
            printf("New process %ld is being created \n", received_processes - count_received);
            mem[received_processes - count_received] = allocate(processTable[received_processes - count_received]->memsize);
            if(mem[received_processes - count_received] == (void *)0){
                printf("Process %ld added to waiting list.\n", received_processes - count_received);
                cc_pqueue_push(waiting_list, &(processTable[received_processes - count_received]));
            } else {
                printf("pushing %ld\n", received_processes - count_received);
                printf("memory allocated : start:%d end:%d\n", mem[received_processes - count_received]->start, mem[received_processes - count_received]->end);
                fprintf(mFile, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), processTable[received_processes - count_received]->memsize, processTable[received_processes - count_received]->id, mem[received_processes - count_received]->start, mem[received_processes - count_received]->end);
                cc_pqueue_push(cc_pq, &(processTable[received_processes - count_received]));
            }
        }
        
        // Context Switching
        printf("size of pq now: %ld\n", cc_pq->size);
        if(cc_pq->size && !current){

            // Schedule a new process to run
            void *p;
            cc_pqueue_pop(cc_pq, &p);
            current = (*(struct pcb **)p)->id;

            // Fork a new process to run
            run();
        }

        // Synchronizing the process with the scheduler.
        sync();

        // Synchronizing the scheduler with the clock
        down(semclk);

        // Check if the current process is finished
        bool finish = checkFinished();
        int size = waiting_list->size;
        while(finish && size--)
        {
            void *p;
            cc_pqueue_pop(waiting_list, &p);
            mem[(*(struct pcb **)p)->id] = allocate((*(struct pcb **)p)->memsize);
            if(mem[(*(struct pcb **)p)->id] == (void *)0){
                cc_pqueue_push(waiting_list, &(processTable[(*(struct pcb **)p)->id]));
            } else {
                printf("pushing %d\n", (*(struct pcb **)p)->id);
                fprintf(mFile, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), (*(struct pcb **)p)->memsize, (*(struct pcb **)p)->id, mem[(*(struct pcb **)p)->id]->start, mem[(*(struct pcb **)p)->id]->end);
                cc_pqueue_push(cc_pq, &(processTable[(*(struct pcb **)p)->id]));
            }
        }
    }
}

void srtn(CC_PQueue *cc_pq)
{
    int count_received;
    while(received_processes < capacity || cc_pq->size || current != 0)
    {
        // Receiving new processes
        count_received = receive();

        // If there is a new process, then I need to check if I need to context switch
        // If there is no new process, then I continue only if there is a process running as I am sure it has the least remaining time
        // or there are no processes in the queue so I need to check if there are new processes to receive
        if(count_received || (!current && cc_pq->size))
        {
            // If there is a process running, then I need to check if I need to context switch
            if (current != 0)
                cc_pqueue_push(cc_pq, &(processTable[current]));

            // Enqueueing the received processes
            while(count_received--)
            {
                printf("New process %ld is being created \n", received_processes - count_received);
                mem[received_processes - count_received] = allocate(processTable[received_processes - count_received]->memsize);
                if(mem[received_processes - count_received] == (void *)0){
                    cc_pqueue_push(waiting_list, &(processTable[received_processes - count_received]));
                } else {
                    fprintf(mFile, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), processTable[received_processes - count_received]->memsize, processTable[received_processes - count_received]->id, mem[received_processes - count_received]->start, mem[received_processes - count_received]->end);
                    cc_pqueue_push(cc_pq, &(processTable[received_processes - count_received]));
                }
            }

            void *p;
            cc_pqueue_pop(cc_pq, &p);

            // If the process is the same as the current process, then I do not need to context switch
            if(current != (*(struct pcb **)p)->id)
            {
                // If the process is different from the current process, then I need to context switch, so log stop on the current process
                if(current != 0){
                    logging("stopped");
                    processTable[current]->state = "waiting";
                }
                
                current = (*(struct pcb **)p)->id;
                printf("Current: %ld\n", current);

                // Fork or resume a new process to run
                run();
            }
        }

        // Synchronizing the process with the scheduler.
        sync();

        // Synchronizing the scheduler with the clock
        down(semclk);
        
       // Check if the current process is finished
        
        bool finish = checkFinished();
        int size = waiting_list->size;
        while(finish && size--)
        {
            void *p;
            cc_pqueue_pop(waiting_list, &p);
            mem[(*(struct pcb **)p)->id] = allocate((*(struct pcb **)p)->memsize);
            if(mem[(*(struct pcb **)p)->id] == (void *)0){
                cc_pqueue_push(waiting_list, &(processTable[(*(struct pcb **)p)->id]));
            } else {
                fprintf(mFile, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), (*(struct pcb **)p)->memsize, (*(struct pcb **)p)->id, mem[(*(struct pcb **)p)->id]->start, mem[(*(struct pcb **)p)->id]->end);
                cc_pqueue_push(cc_pq, &(processTable[(*(struct pcb **)p)->id]));
            }
        }
    } 
}

void rr(CC_Rbuf *rbuf)
{
    int count_received;
    while(received_processes < capacity || !cc_rbuf_is_empty(rbuf) || current != 0)
    {   
        // Receiving new processes
        count_received = receive();

        // Enqueueing the received processes
        while(count_received--)
        {
            printf("New process %ld is being created\n", received_processes - count_received);
            mem[received_processes - count_received] = allocate(processTable[received_processes - count_received]->memsize);
            if(mem[received_processes - count_received] == (void *)0){
                cc_pqueue_push(waiting_list, &(processTable[received_processes - count_received]));
            } else {
                fprintf(mFile, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), processTable[received_processes - count_received]->memsize, processTable[received_processes - count_received]->id, mem[received_processes - count_received]->start, mem[received_processes - count_received]->end);
                cc_rbuf_enqueue(rbuf, received_processes - count_received); 
            }
        }
        
        // context switching
        if(!cc_rbuf_is_empty(rbuf) && Qtemp == 0){

            // Suspend current process
            if(current != 0){
                logging("stopped");
                processTable[current]->state = "waiting";
                cc_rbuf_enqueue(rbuf, current);
                current = 0;
            }

            // Schedule a new process to run
            cc_rbuf_dequeue(rbuf, &current);

            // fork or resume
            run();
            
            // Reset the quanta
            Qtemp = quanta;
        }

        // Synchronizing the process with the scheduler.
        sync();

        // Decrementing the quanta (capped at 0)
        Qtemp = Qtemp ? Qtemp - 1 : 0;

        // Synchronizing the scheduler with the clock
        down(semclk);

        // Check if the current process is finished
        bool finish = checkFinished();
        int size = waiting_list->size;
        while(finish && size--)
        {
            void *p;
            cc_pqueue_pop(waiting_list, &p);
            mem[(*(struct pcb **)p)->id] = allocate((*(struct pcb **)p)->memsize);
            if(mem[(*(struct pcb **)p)->id] == (void *)0){
                cc_pqueue_push(waiting_list, &(processTable[(*(struct pcb **)p)->id]));
            } else {
                fprintf(mFile, "At time %d allocated %d bytes for process %d from %d to %d\n", getClk(), (*(struct pcb **)p)->memsize, (*(struct pcb **)p)->id, mem[(*(struct pcb **)p)->id]->start, mem[(*(struct pcb **)p)->id]->end);
                cc_rbuf_enqueue(rbuf, (*(struct pcb **)p)->id);
            }
        }
    }
}

int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    int algorithm = atoi(argv[1]);
    capacity = atoi(argv[2]);
    quanta = atoi(argv[3]);

    processTable = malloc((capacity+1) * sizeof(struct pcb *));
    mem = malloc((capacity+1) * sizeof(struct node *));
    wta = malloc((capacity+1) * sizeof(float));

    msgq_id = msgget(GENKEY, 0666 | IPC_CREAT);
    semclk = semget(SEMKEY, 1, 0666);
    msgqIdProcess = msgget(MSGKEY, 0666 | IPC_CREAT);
    
    if(msgqIdProcess == -1)
    {
        perror("Error in creating message queue of processes");
        exit(-1);
    }

    struct msgbuff message;

    pFile = fopen("scheduler.log", "w");
    if(pFile == NULL)
    {
        perror("Error in opening scheduler.log");
        exit(-1);
    }

    mFile = fopen("memory.log", "w");
    if(mFile == NULL)
    {
        perror("Error in opening memory.log");
        exit(-1);
    }

    BTree = createNode(0, 1023, (void *)0);
    cc_pqueue_new(&waiting_list, compare_memory);
    CC_PQueue *cc_pq;
    switch (algorithm)
    {
    case 1:
        cc_pqueue_new(&cc_pq, compare_priority);
        hpf(cc_pq);
        break;
    case 2:
        cc_pqueue_new(&cc_pq, compare_remaining_time);
        srtn(cc_pq);
        break;
    default:
        CC_Rbuf *cc_rbuf;
        quanta = atoi(argv[3]);
        cc_rbuf_new(&cc_rbuf);
        rr(cc_rbuf);
        break;
    }

    fclose(pFile);
    fclose(mFile);

    pFile = fopen("scheduler.perf", "w");
    if(pFile == NULL)
    {
        perror("Error in opening scheduler.log");
        exit(-1);
    }

    float avg_wta = (float)sum_wta / capacity;
    // CPU utilization
    fprintf(pFile, "CPU Utilization = %.2f%%\n", ((float)sum_running / getClk()) * 100);
    fprintf(pFile, "Avg WTA = %.2f\n", avg_wta);
    fprintf(pFile, "Avg Waiting = %.2f\n", (float)sum_waiting / capacity);

    float std_wta = 0;
    for (int i = 1; i <= capacity; i++)
        std_wta += ((wta[i]-avg_wta) * (wta[i]-avg_wta));
    
    fprintf(pFile, "Std WTA = %.2f\n", sqrt(std_wta / capacity));

    fclose(pFile);
    destroyClk(true);
    return 0;
}
