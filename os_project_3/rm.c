#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "rm.h"

// global variables
int DA;	// indicates if deadlocks will be avoided or not
int N;	// number of processes
int M;	// number of resource types
int ExistingRes[MAXR];	// Existing resources vector
int deadlocked_threads = 0;

int Available[MAXR];	// Available resources vector
int Allocation[MAXP][MAXR];	// Allocation matrix
int Request[MAXP][MAXR];	// Request matrix
int MaxDemand[MAXP][MAXR];	// Maximum demand matrix
int Need[MAXP][MAXR];	// Need matrix
int tempReq[MAXP][MAXR];
pthread_t resource_threads[MAXP];	// Array to store pthread_t identifiers for the threads
pthread_mutex_t resource_mutex;	// Mutex to protect access to shared data structures
pthread_cond_t resource_cond;	// Condition variable to signal when resources are released

// Utility functions

int has_source(int tid){
	for(int i = 0; i < M ; ++i){
		if(Allocation[tid][i] > 0)
			return 1;
	
	}
	return 0;
}
int is_safe_state()
{
	int work[MAXR];
	int finish[MAXP];
	int i, j;

	for (i = 0; i < M; ++i)
	{
		work[i] = Available[i];
	}

	for (i = 0; i < N; ++i)
	{
		finish[i] = 0;
	}

	while (1)
	{
		int found = 0;
		for (i = 0; i < N; ++i)
		{
			if (!finish[i])
			{
				int can_finish = 1;
				for (j = 0; j < M; ++j)
				{
					if (Need[i][j] > work[j])
					{
						can_finish = 0;
						break;
					}
				}

				if (can_finish)
				{
					found = 1;
					finish[i] = 1;
					for (j = 0; j < M; ++j)
					{
						work[j] += Allocation[i][j];
					}
				}
			}
		}

		if (!found)
		{
			break;
		}
	}

	for (i = 0; i < N; ++i)
	{
		if (!finish[i])
		{
			return 0;
		}
	}

	return 1;
}

int get_tid(pthread_t thread_id)
{
    for (int i = 0; i < N; ++i)
    {
        if (pthread_equal(resource_threads[i], thread_id))
        {
            return i;
        }
    }
    return -1;
}

// Library functions
int rm_init(int p_count, int r_count, int r_exist[], int avoid)
{
	int i, j;
	int ret = 0;

	if (p_count > MAXP || r_count > MAXR)
	{
		return -1;
	}

	DA = avoid;
	N = p_count;
	M = r_count;

	// Initialize (create) resources
	for (i = 0; i < M; ++i)
	{
		if (r_exist[i] < 0)
		{
			return -1;
		}

		ExistingRes[i] = r_exist[i];
		Available[i] = r_exist[i];
	}

	// Initialize Allocation, Request, MaxDemand, and Need matrices
	for (i = 0; i < N; ++i)
	{
		for (j = 0; j < M; ++j)
		{
			Allocation[i][j] = 0;
			Request[i][j] = 0;
			MaxDemand[i][j] = 0;
			Need[i][j] = 0;
		}
	}

	// Initialize mutex and condition variable
	pthread_mutex_init(&resource_mutex, NULL);
	pthread_cond_init(&resource_cond, NULL);

	return ret;
}

int rm_thread_started(int tid)
{
    if (tid < 0 || tid >= N)
    {
        return -1;
    }

    pthread_mutex_lock(&resource_mutex);
    resource_threads[tid] = pthread_self();
    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int rm_thread_ended()
{
	int ret = 0;
	pthread_mutex_lock(&resource_mutex);
	pthread_t thread_id = pthread_self();
	int tid = get_tid(thread_id);
	if (tid >= 0)
	{
		for (int i = 0; i < M; ++i)
		{
			Available[i] += Allocation[tid][i];
			Allocation[tid][i] = 0;
		}

		pthread_cond_signal(&resource_cond);
	}
	else
	{
		ret = -1;
	}

	pthread_mutex_unlock(&resource_mutex);
	return ret;
}

int rm_claim(int claim[])
{
    pthread_mutex_lock(&resource_mutex);
    pthread_t thread_id = pthread_self();
    int tid = get_tid(thread_id);
    if (tid < 0)
    {
        pthread_mutex_unlock(&resource_mutex);
        return -1;
    }

    for (int i = 0; i < M; ++i)
    {
        if (claim[i] > ExistingRes[i])
        {
            pthread_mutex_unlock(&resource_mutex);
            return -1;
        }

        MaxDemand[tid][i] = claim[i];
    }

    pthread_mutex_unlock(&resource_mutex);
    return 0;
}

int rm_request(int request[])
{
	pthread_mutex_lock(&resource_mutex);
	pthread_t thread_id = pthread_self();
	int tid = get_tid(thread_id);
	if (tid < 0)
	{
		pthread_mutex_unlock(&resource_mutex);
		return -1;
	}

	for (int i = 0; i < M; ++i)
	{
		if (request[i] > ExistingRes[i])
		{
			pthread_mutex_unlock(&resource_mutex);
			return -1;
		}

		Request[tid][i] = request[i];
		Need[tid][i] = MaxDemand[tid][i] - Allocation[tid][i];
	}
int wait_flag = 0;
	while (1)
	{
		if( !DA ||is_safe_state()){

			if(!is_safe_state()){
				printf("Deadlock detected, releasing all resources for Thread %d\n", tid);
				deadlocked_threads++;
			}

			for (int i = 0; i < M; ++i){
				Available[i] -= Request[tid][i];
				Allocation[tid][i] += Request[tid][i];
				Need[tid][i] -= Request[tid][i];
				tempReq[tid][i] = Request[tid][i];
				Request[tid][i] = 0;
			}

			pthread_mutex_unlock(&resource_mutex);	
			if( DA || wait_flag){
				pthread_cond_broadcast(&resource_cond);
				wait_flag = 0;
			}
			return 1;// check et
		}
		else{
			if(has_source(tid)){

				for(int i = 0; i < M ; ++i){
					Available[i] += Allocation[tid][i];
					Allocation[tid][i] = 0;
					Request[tid][i] = request[i];
				}
				
			}
			else{
				wait_flag = 1;
				pthread_cond_wait(&resource_cond, &resource_mutex);
    			deadlocked_threads = 0;

			}
		}
	}
}



int rm_release(int release[])
{
	pthread_mutex_lock(&resource_mutex);
	pthread_t thread_id = pthread_self();
	int tid = get_tid(thread_id);
	if (tid < 0)
	{
		pthread_mutex_unlock(&resource_mutex);
		return -1;
	}

	for (int i = 0; i < M; ++i)
	{
		if (release[i] > Allocation[tid][i])
		{
			pthread_mutex_unlock(&resource_mutex);
			return -1;
		}

		Allocation[tid][i] -= release[i];
		Available[i] += release[i];
	}

	pthread_cond_signal(&resource_cond);

	pthread_mutex_unlock(&resource_mutex);
	return 0;

}

int rm_detection()
{
    pthread_mutex_lock(&resource_mutex);
    if (!is_safe_state())
    {
        for (int i = 0; i < N; ++i)
        {
            int waiting = 1;
            for (int j = 0; j < M; ++j)
            {
                if (Request[i][j] <= Available[j])
                {
                    waiting = 0;
                    break;
                }
            }

            if (waiting)
            {
                deadlocked_threads++;
            }
        }
    }

    pthread_mutex_unlock(&resource_mutex);
    return deadlocked_threads;
}

void rm_print_state(char headermsg[])
{
	pthread_mutex_lock(&resource_mutex);

	printf("########################## %s ###########################\n", headermsg);
	printf("Exist:\n");
	for (int i = 0; i < M; ++i)
	{

		printf("R%d%-3s", i,"");
	}
	printf("\n");
	for (int i = 0; i < M; ++i)
	{
		printf("%-5d", ExistingRes[i]);
	}

	printf("\n");

	printf("Available:\n");
	for (int i = 0; i < M; ++i)
	{
		printf("%-5sR%d", "",i);
	}

		printf("\n");

	for (int i = 0; i < M; ++i)
	{
		printf("%7d", Available[i]);
	}
		printf("\n");
	printf("Allocation:\n");
	for (int i = 0; i < M; ++i)
	{
		printf("%-5sR%d", "",i);
	}

	printf("\n");
	for (int i = 0; i < N; ++i)
	{
		printf("T%d:%-2s", i,"");
		for (int j = 0; j < M; ++j)
		{
			printf("%-7d", Allocation[i][j]);
		}

		printf("\n");
	}

	printf("Request:\n");
	for (int i = 0; i < M; ++i)
	{
		printf("%-5sR%d", "",i);
	}

	printf("\n");

	for (int i = 0; i < N; ++i)
	{
		printf("T%d:%-2s", i,"");
		for (int j = 0; j < M; ++j)
		{
			printf("%-7d", tempReq[i][j]);
		}

		printf("\n");
	}

	printf("MaxDemand:\n");
	for (int i = 0; i < M; ++i)
	{
		printf("%-5sR%d", "",i);
	}

	printf("\n");
	for (int i = 0; i < N; ++i)
	{
		printf("T%d:%-2s", i,"");
		for (int j = 0; j < M; ++j)
		{
			printf("%-7d", MaxDemand[i][j]);
		}

		printf("\n");
	}

	printf("Need:\n");
	for (int i = 0; i < M; ++i)
	{
		printf("%-5sR%d", "",i);
	}

	printf("\n");
	for (int i = 0; i < N; ++i)
	{
		printf("T%d:%-2s", i,"");
		for (int j = 0; j < M; ++j)
		{
			printf("%-7d", Need[i][j]);
		}

		printf("\n");
	}

	printf("################################################################\n");

	pthread_mutex_unlock(&resource_mutex);
}
