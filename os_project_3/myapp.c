#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "rm.h"

#define NUM_THREADS 3
#define NUM_RESOURCES 5

int resources[NUM_RESOURCES] = {10, 5, 7, 8, 6};

void *thread_func(void *arg)
{
    int tid = (int)(intptr_t)arg;
    rm_thread_started(tid);

    int claim[NUM_RESOURCES] = {0};
    int request[NUM_RESOURCES] = {0};
    int release[NUM_RESOURCES] = {0};

    if (tid == 0) {
        claim[0] = 6;
        claim[1] = 0;
        claim[2] = 3;
        claim[3] = 0;
        claim[4] = 0;
    } else if (tid == 1) {
        claim[0] = 0;
        claim[1] = 3;
        claim[2] = 0;
        claim[3] = 3;
        claim[4] = 0;
    } else {
        claim[0] = 6;
        claim[1] = 0;
        claim[2] = 0;
        claim[3] = 0;
        claim[4] = 3;
    }

    rm_claim(claim);
    rm_print_state("Thread claimed resources");

    for (int r = 0; r < 10; ++r)
    {
        for (int i = 0; i < NUM_RESOURCES; ++i)
        {
            request[i] = rand() % (claim[i] + 1);
        }
        rm_request(request);
        rm_print_state("Thread requested resources");

        usleep(rand() % 100000);

        for (int i = 0; i < NUM_RESOURCES; ++i)
        {
            release[i] = request[i];
        }
        rm_release(release);
        rm_print_state("Thread released resources");
    }

    rm_thread_ended();
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s [flag: 0 for no avoidance, 1 for avoidance]\n", argv[0]);
        return 1;
    }

    int flag = atoi(argv[1]);
    if (flag != 0 && flag != 1)
    {
        printf("Invalid flag value. Use 0 for no avoidance, 1 for avoidance.\n");
        return 1;
    }

    if (rm_init(NUM_THREADS, NUM_RESOURCES, resources, flag) < 0)
    {
        printf("Error initializing resource manager.\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        if (pthread_create(&threads[i], NULL, thread_func, (void *)(intptr_t)i) != 0)
        {
            printf("Error creating thread %d.\n", i);
            return 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    int deadlocks = rm_detection();
    printf("Deadlocks detected: %d\n", deadlocks);

    return 0;
}

