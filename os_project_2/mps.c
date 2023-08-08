#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>

struct Queue **readyQueues; // list of lists of the ready queues
struct Queue *finishedItemQueue; // list of finished items

char sap[2];
char alg[5];
int timeQuantum;
int m;
struct timeval tv;
struct timezone tz;
long start_ms;
int outputFile_flag;
FILE *outFile;

struct BurstItem {
    int pid; // process id
    int tid; // thread id
    int burstLength;
    long arrivalTime;
    int remainingTime;
    long finishTime;
    long turnaroundTime;
    long waitingTime;
};

struct QueueNode {
    struct BurstItem *i;
    struct QueueNode *next;
    struct QueueNode *prev;
};

struct Queue {
    struct QueueNode *front;
    struct QueueNode *back;
    int totalLoad;
    pthread_mutex_t th_mutex_queue;   /* mutex to protect queue */
};

void initQueue(struct Queue *q) {
    q->front = NULL;
    q->back = NULL;
    q->totalLoad = 0;
    pthread_mutex_init(&q->th_mutex_queue, NULL);
}

void freeQueue(struct Queue *queue) {
    struct QueueNode *currentNode = queue->front;
    while (currentNode != NULL) {
        struct QueueNode *temp = currentNode;
        currentNode = currentNode->next;
        free(temp->i);
        free(temp);
    }
    pthread_mutex_destroy(&queue->th_mutex_queue);  // add this line to release the mutex
    free(queue);
}

void enqueue(struct Queue *q, struct BurstItem *i) {
    pthread_mutex_lock(&q->th_mutex_queue);

    struct QueueNode *newNode = (struct QueueNode *) malloc(sizeof(struct QueueNode));
    newNode->i = i;
    newNode->next = NULL;

    if (q->back == NULL) {
        q->front = newNode;
        q->back = newNode;
        newNode->prev = NULL;
    } else {
        q->back->next = newNode;
        newNode->prev = q->back;
        q->back = newNode;
    }

    pthread_mutex_unlock(&q->th_mutex_queue);
}

struct BurstItem *dequeue(struct Queue *q) {
    pthread_mutex_lock(&q->th_mutex_queue);

    if (q->front == NULL) {
        pthread_mutex_unlock(&q->th_mutex_queue);
        return NULL;
    }

    struct QueueNode *frontNode = q->front;
    struct BurstItem *i = frontNode->i;

    if (q->front == q->back) {
        q->front = NULL;
        q->back = NULL;
    } else {
        q->front = q->front->next;
        q->front->prev = NULL;
    }

    if (frontNode->i->pid != -1) {
        q->totalLoad -= frontNode->i->burstLength;
    }
    free(frontNode);
    pthread_mutex_unlock(&q->th_mutex_queue);

    return i;
}

void removeNode(struct Queue *q, struct QueueNode *node) {
    if (node == q->front) {
        q->front = node->next;
    } else {
        node->prev->next = node->next;
    }

    if (node == q->back) {
        q->back = node->prev;
    } else {
        node->next->prev = node->prev;
    }

    if (node->i->pid != -1) {
        q->totalLoad -= node->i->burstLength;
    }
    free(node);
}

int isQueueEmpty(struct Queue *q) {
    return (q->front == NULL);
}

struct BurstItem *removeShortestJob(struct Queue *q) {
    pthread_mutex_lock(&q->th_mutex_queue);

    int shortestLength = INT_MAX;
    struct QueueNode *nodeToRemove = NULL;

    struct QueueNode *currentNode = q->front;
    while (currentNode != NULL) {
        if (currentNode->i->burstLength < shortestLength) {
            shortestLength = currentNode->i->burstLength;
            nodeToRemove = currentNode;
        }
        currentNode = currentNode->next;
    }


    struct BurstItem *i = NULL;
    if (nodeToRemove != NULL) {
        i = nodeToRemove->i;
        removeNode(q, nodeToRemove);
    }
    pthread_mutex_unlock(&q->th_mutex_queue);
    return i;
}

void sortQueueByPid(struct Queue *queue) {
    struct QueueNode *currentNode = queue->front->next;
    while (currentNode != NULL) {
        struct BurstItem *currentItem = currentNode->i;
        struct QueueNode *compareNode = currentNode->prev;
        while (compareNode != NULL && compareNode->i->pid > currentItem->pid) {
            compareNode->next->i = compareNode->i;
            compareNode = compareNode->prev;
        }
        if (compareNode == NULL) {
            queue->front->i = currentItem;
        } else {
            compareNode->next->i = currentItem;
        }
        currentNode = currentNode->next;
    }
}

// If outmode is specified, print to file; otherwise, print to screen
void my_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    if (outputFile_flag == 0) {
        vprintf(format, args);
    } else {
        vfprintf(outFile, format, args);
    }
    va_end(args);
}

void putBurstItemInNextQueue(int numOfQueues, char *qs, int *queueCnt, struct BurstItem *burstItem) {
    if (strcmp(qs, "RM") == 0) {
        enqueue(readyQueues[*queueCnt], burstItem);
        if (m == 3) {
            my_print("pid = %d is added to the queue number = %d", burstItem->pid, *queueCnt);
        }
        (*queueCnt)++;
        *queueCnt %= numOfQueues;
    } else if (strcmp(qs, "NA") == 0) {
        // queueCnt initialized to 0 in main code
        enqueue(readyQueues[*queueCnt], burstItem);
        if (m == 3) {
            my_print("pid = %d is added to the queue number = %d", burstItem->pid, *queueCnt);
        }
    } else if (strcmp(qs, "LM") == 0) {
        int min = readyQueues[0]->totalLoad;
        int minIndex = 0;
        for (int i = 1; i < numOfQueues; i++) {
            if (min > readyQueues[i]->totalLoad) {
                minIndex = i;
                min = readyQueues[i]->totalLoad;
            }
        }
        enqueue(readyQueues[minIndex], burstItem);
        readyQueues[minIndex]->totalLoad += burstItem->burstLength;
        if (m == 3) {
            my_print("pid=%d is added to the queue number=%d\n", burstItem->pid, minIndex);
        }
        return;
    }
    readyQueues[*queueCnt]->totalLoad += burstItem->burstLength;
}

// thread function will take a pointer to this structure
struct arg {
    int t_index;    // the index of the created thread
};

static long getTime() {
    gettimeofday(&tv, &tz);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000) - start_ms;
}

// this is the function to be executed by all the threads concurrently
static void *do_task(void *arg_ptr) {
    int queueIndex = ((struct arg *) arg_ptr)->t_index;
    long time;
    if (strcmp(sap, "S") == 0) {
        queueIndex = 0;
    }
    while (1) {
        struct BurstItem *burstItem = NULL;
        while (burstItem == NULL) {
            if (strcmp(alg, "SJF") != 0) {
                burstItem = dequeue(readyQueues[queueIndex]);
            } else {
                burstItem = removeShortestJob(readyQueues[queueIndex]);
            }
            if (burstItem == NULL) {
                usleep(1000); // 1 ms sleep
            }
        }
        if (burstItem->pid == -1) {
            if (isQueueEmpty(readyQueues[queueIndex])) {
                // put dummy item back (otherwise memory leak)
                enqueue(readyQueues[queueIndex], burstItem);
                break;
            } else {
                enqueue(readyQueues[queueIndex], burstItem);
                continue;
            }
        }
        int burstTime = burstItem->burstLength;
        if (strcmp(alg, "RR") == 0) {
            if (timeQuantum < burstItem->remainingTime) {
                burstTime = timeQuantum;
            }
        }
        time = getTime();
        burstItem->tid = ((struct arg *) arg_ptr)->t_index;
        if (m == 2) {
            my_print("time=%ld, cpu=%d, pid=%d, burstlen=%d, remainingtime=%d\n", time, burstItem->tid, burstItem->pid,
                   burstItem->burstLength, burstItem->remainingTime);
        } else if (m == 3) {
            my_print("time=%ld, cpu=%d, picked burst pid=%d, burstlen=%d, remainingtime=%d\n", time, burstItem->tid,
                   burstItem->pid,
                   burstItem->burstLength, burstItem->remainingTime);
        }
        usleep(burstTime * 1000);
        if (strcmp(alg, "RR") == 0 && burstItem->remainingTime > timeQuantum) {
            burstItem->remainingTime -= timeQuantum;
            enqueue(readyQueues[queueIndex], burstItem);
            if (m == 3 && burstItem->remainingTime > 0) {
                time = getTime();
                my_print("time slice is expired at time=%ld, in cpu=%d, for burst pid=%d, burstlen=%d, remainingtime=%d\n",
                       time, burstItem->tid, burstItem->pid,
                       burstItem->burstLength, burstItem->remainingTime);
            }
            continue;
        }
        if (m == 3 && burstItem->remainingTime > 0) {
            time = getTime();
            my_print("burst is finished at time=%ld, in cpu=%d, for burst pid=%d, burstlen=%d, remainingtime=%d\n", time,
                   burstItem->tid, burstItem->pid,
                   burstItem->burstLength, burstItem->remainingTime);
        }
        burstItem->remainingTime = 0;
        time = getTime();
        burstItem->finishTime = time;
        burstItem->turnaroundTime = burstItem->finishTime - burstItem->arrivalTime;
        burstItem->waitingTime = burstItem->turnaroundTime - burstItem->burstLength;
        enqueue(finishedItemQueue, burstItem);
    }
    pthread_exit(NULL); //  tell a reason to thread waiting in join
}

void assignBurst(int burstLength, int numOfQueues, char *qs, int *queueSelectCnt, int *pidCounter) {
    struct BurstItem *burstItem = malloc(sizeof(struct BurstItem));
    long time = getTime();
    burstItem->arrivalTime = time;
    burstItem->burstLength = burstLength;
    burstItem->remainingTime = burstItem->burstLength;
    burstItem->pid = *pidCounter;
    (*pidCounter)++;
    putBurstItemInNextQueue(numOfQueues, qs, queueSelectCnt, burstItem);
}

// Function to generate a random value x from an exponential distribution with rate parameter lambda
double random_exp(double lambda) {
    double u = (double) rand() / RAND_MAX;
    return ((-1.0) * log(1.0 - u)) / lambda;
}

// Function to generate random interarrival times and burst lengths
int *generate_random_times(int T, int T1, int T2, int L, int L1, int L2, int PC) {
    int *times = malloc(sizeof(int) * (1 + 2 * PC)); // Allocate memory for times array
    if (times == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    times[0] = PC; // Store the number of bursts in times[0]

    double lambda_t = 1.0 / T; // Calculate the rate parameter for the interarrival times
    double lambda_l = 1.0 / L; // Calculate the rate parameter for the burst lengths

    // Generate the interarrival times
    int i;
    for (i = 1; i <= PC; i++) {
        double x;
        do {
            double u = (double) rand() / RAND_MAX;
            x = random_exp(lambda_t);
        } while (x < T1 || x > T2);
        times[i + PC] = (int) round(x);
    }

    // Generate the burst lengths
    for (i = 1; i <= PC; i++) {
        double x;
        do {
            double u = (double) rand() / RAND_MAX;
            x = random_exp(lambda_l);
        } while (x < L1 || x > L2);
        times[i] = (int) round(x);
    }

    return times;
}





int main(int argc, char *argv[]) {

    //THIS PART SETS PARAMETERS TO DEFAULT IN THE BEGINNING
    int n = 2;// default number of processors is 2
    strcpy(sap, "M"); // default is M. can be at most 2 chars(i.e. S\0)
    char qs[3] = "RM";  // default is RM. can be at most 3 chars(i.e. NA\0)
    strcpy(alg, "RR"); //default is RR. can be at most 5 chars (FCFS\0)
    timeQuantum = 20; //default is 20.
    m = 1;  //default is 1.
    char i[64] = "in.txt"; //default is in.txt . Max char can be 63 ??
    char o[64] = "out.txt"; //default is out.txt . Max char can be 63 ??
    int t = 200;
    int t1 = 10;
    int t2 = 1000;
    int l = 100;
    int l1 = 10;
    int l2 = 500;
    int pc = 10;
    int mode = 0; // if mode <= 0 then it is -r mode, if it is 1 then it is -i mode
    outputFile_flag = 0;


    //THIS PART MODIFY THE PARAMETERS IF SPECIFIED
    for (int j = 1; j < argc; ++j) {
        if (strcmp(argv[j], "-n") == 0) {
            n = atoi(argv[j + 1]);
            j = j + 1;
        } else if (strcmp(argv[j], "-a") == 0) {
            strcpy(sap, argv[j + 1]);
            strcpy(qs, argv[j + 2]);
            j = j + 2;
        } else if (strcmp(argv[j], "-s") == 0) {
            strcpy(alg, argv[j + 1]);
            timeQuantum = atoi(argv[j + 2]);
            j = j + 2;
        } else if (strcmp(argv[j], "-i") == 0) {
            strcpy(i, argv[j + 1]);
            j = j + 1;
            mode++; // to make mode = 1
        } else if (strcmp(argv[j], "-m") == 0) {
            m = atoi(argv[j + 1]);
            j = j + 1;
        } else if (strcmp(argv[j], "-o") == 0) {
            strcpy(o, argv[j + 1]);
            j = j + 1;
            outputFile_flag = 1;
        } else if (strcmp(argv[j], "-r") == 0) {
            t = atoi(argv[j + 1]);
            t1 = atoi(argv[j + 2]);
            t2 = atoi(argv[j + 3]);
            l = atoi(argv[j + 4]);
            l1 = atoi(argv[j + 5]);
            l2 = atoi(argv[j + 6]);
            pc = atoi(argv[j + 7]);
            j = j + 7;
            mode--; // to make mode <= 0
        }
    }

    if (outputFile_flag == 1) {
        outFile = fopen(o, "w");
    }

    int numOfQueues = n;
    if (strcmp(sap, "S") == 0) {
        numOfQueues = 1;
    }

    // Init Queue lists
    readyQueues = malloc(sizeof(struct Queue) * numOfQueues);
    for (int j = 0; j < numOfQueues; ++j) {
        readyQueues[j] = malloc(sizeof(struct Queue));
        initQueue(readyQueues[j]);
    }
    finishedItemQueue = malloc(sizeof(struct Queue));
    initQueue(finishedItemQueue);

    // Create Threads
    pthread_t tids[n];    // thread ids
    struct arg t_args[n];    // thread function arguments
    int ret;
    for (int j = 0; j < n; ++j) {
        t_args[j].t_index = j;
        ret = pthread_create(&(tids[j]),
                             NULL, do_task, (void *) &(t_args[j]));
        if (ret != 0) {
            my_print("thread create failed \n");
            exit(1);
        }
    }

    // SIMULATION STARTS
    char word[64];
    int flag = 0; // 0 No info, 1 Burst info, 2 IAT info
    int pidCounter = 1;
    int queueSelectCnt = 0;
    start_ms = getTime();

    if (mode > 0) { // it means we are in input mode, do input file read operations
        FILE *inpFile;
        inpFile = fopen(i, "r");
        if (inpFile == NULL) {
            my_print("Can't open %s for reading.\n", i);
        } else {
            while (fscanf(inpFile, "%s", word) != EOF) {
                if (strcmp(word, "PL") == 0) {
                    flag = 1;
                } else if (strcmp(word, "IAT") == 0) {
                    flag = 2;
                } else if (flag == 1) {
                    assignBurst(atoi(word), numOfQueues, qs, &queueSelectCnt, &pidCounter);
                    flag = 0;
                } else if (flag == 2) {
                    // microseconds = milliseconds * 1000
                    usleep(atoi(word) * 1000);
                    flag = 0;
                }
            }
            fclose(inpFile);
        }
    } else { //do random operations inp file
        int *randomArr = generate_random_times(t, t1, t2, l, l1, l2,
                                               pc);// randomArr[0] = pc, arr[1] = first PL arr[1+pc] = first IAT

        for (int j = 1; j <= pc; ++j) {
            assignBurst(randomArr[j], numOfQueues, qs, &queueSelectCnt, &pidCounter);
            usleep(randomArr[j + pc] * 1000);
        }
        free(randomArr);
    }

    // put dummy items
    for (int j = 0; j < numOfQueues; ++j) {
        struct BurstItem *burstItem = malloc(sizeof(struct BurstItem));
        burstItem->burstLength = INT_MAX - 1;
        burstItem->pid = -1;
        enqueue(readyQueues[j], burstItem);
    }

    // printf("main: waiting all threads to terminate\n");
    for (int j = 0; j < n; ++j) {
        ret = pthread_join(tids[j], NULL);
        if (ret != 0) {
            my_print("thread join failed \n");
            exit(1);
        }
        // printf("thread terminated, msg = %s\n", retmsg);
    }

    // printf("main: all threads terminated\n");
    sortQueueByPid(finishedItemQueue);

    int itemCount = 0;
    // print header row
    my_print("%-8s%-8s%-12s%-12s%-12s%-14s%-12s\n", "pid", "cpu", "burstlen", "arv", "finish", "waitingtime",
           "turnaround");

    // print table data
    struct QueueNode *currentNode = finishedItemQueue->front;
    while (currentNode != NULL) {
        struct BurstItem item = *(currentNode->i);
        my_print("%-8d%-8d%-12d%-12ld%-12ld%-14ld%-12ld\n", item.pid, item.tid, item.burstLength,
               item.arrivalTime, item.finishTime, item.waitingTime, item.turnaroundTime);
        currentNode = currentNode->next;
    }

    // calculate and print average turnaround time
    long sum_turnaround = 0;
    currentNode = finishedItemQueue->front;
    while (currentNode != NULL) {
        sum_turnaround += currentNode->i->turnaroundTime;
        currentNode = currentNode->next;
        itemCount++;
    }

    long avg_turnaround = sum_turnaround / itemCount;
    my_print("average turnaround time: %ld ms\n", avg_turnaround);

    // Free Queue lists
    for (int j = 0; j < numOfQueues; ++j) {
        freeQueue(readyQueues[j]);
    }
    free(readyQueues);
    freeQueue(finishedItemQueue);

    if (outputFile_flag == 1){
        fclose(outFile);
    }

    return 0;
}