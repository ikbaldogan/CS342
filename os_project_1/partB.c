#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linkedList.h"


struct Node **list;           // lists that will be filled by threads

char **inpFileNames;

// thread function will take a pointer to this structure
struct arg {
    int k;          // number of words to find
    int t_index;    // the index of the created thread
};

// this is the function to be executed by all the threads concurrently
static void *do_task(void *arg_ptr) {
    FILE *inpFile;
    char *retreason;
    char word[64];
    struct Node *tempList;
    tempList = NULL;

    inpFile = fopen(inpFileNames[((struct arg *) arg_ptr)->t_index], "r");

    if (inpFile == NULL)
        printf("Can't open %s for reading.\n", inpFileNames[((struct arg *) arg_ptr)->t_index]);
    else {
        while (fscanf(inpFile, "%s", word) != EOF) {
            // Convert word to uppercase
            for (int i = 0; word[i] != '\0'; i++) {
                if (word[i] >= 'a' && word[i] <= 'z') {
                    word[i] = word[i] - 32;
                }
            }
            append(&tempList, word);
        }
        fclose(inpFile);
    }
    list[((struct arg *) arg_ptr)->t_index] = tempList;

    // Delete the numbers that are not in K top numbers from the linked list
    int cnt = ((struct arg *) arg_ptr)->k;
    while (tempList != NULL && cnt > 0) {
        tempList = tempList->next;
        cnt--;
    }


    if (tempList != NULL) {
        // Previous of the tempList will be the end of the list
        if (tempList->prev != NULL) {
            tempList->prev->next = NULL;
        }
        // If there is more than k numbers in the linked list,
        // Then now the tempList points to the first element that will be deleted
        if (tempList == list[((struct arg *) arg_ptr)->t_index]) {
            list[((struct arg *) arg_ptr)->t_index] = NULL;
        }
        freeList(tempList);

    }
    retreason = (char *) malloc(200);
    strcpy(retreason, "normal termination of thread");
    pthread_exit(retreason); //  tell a reason to thread waiting in join
    // we could simply exit as below, if we don't want to pass a reason
    // pthread_exit(NULL);
    // then we would also modify pthread_join call.
}


int main(int argc, char *argv[]) {
    int count;                // number of threads
    int ret;
    char *retmsg;
    int topKNum;            // top K numbers

    if (argc < 5 && (argc < 4 && atoi(argv[3]) != 0)) {
        printf("usage: threadtopk <K> <outfile> <N> <infile1> .... <infileN>\n");
        exit(1);
    }


    count = atoi(argv[3]);    // number of threads to create

    if (count != (argc - 4)) {
        printf("There must be %d input files", count);
        exit(1);
    }

    // Putting input files into global list
    inpFileNames = malloc(sizeof(char *) * count);
    if (!inpFileNames) { /* If list == 0 after the call to malloc, allocation failed for some reason */
        perror("Error allocating memory");
        abort();
    }
    for (int i = 0; i < count; ++i) {
        inpFileNames[i] = argv[4 + i];
    }


    pthread_t tids[count];    // thread ids
    struct arg t_args[count];    // thread function arguments
    topKNum = atoi(argv[1]);

    list = malloc(sizeof(struct Node *) * count);
    if (!list) { /* If list == 0 after the call to malloc, allocation failed for some reason */
        perror("Error allocating memory");
        abort();
    }


    for (int i = 0; i < count; ++i) {
        t_args[i].k = topKNum;
        t_args[i].t_index = i;

        ret = pthread_create(&(tids[i]),
                             NULL, do_task, (void *) &(t_args[i]));

        if (ret != 0) {
            printf("thread create failed \n");
            exit(1);
        }
    }

    // printf("main: waiting all threads to terminate\n");
    for (int i = 0; i < count; ++i) {
        ret = pthread_join(tids[i], (void **) &retmsg);
        if (ret != 0) {
            printf("thread join failed \n");
            exit(1);
        }
        //printf("thread terminated, msg = %s\n", retmsg);
        // we got the reason as the string pointed by retmsg.
        // space for that was allocated in thread function.
        // now we are freeing the allocated space.
        free(retmsg);
    }

    //printf("main: all threads terminated\n");


    if (count > 0) {
        // Parent process will use the linked list of the first thread
        struct Node *parentList;
        parentList = list[0];
        for (int i = 1; i < count; ++i) {
            struct Node *temp = list[i];
            while (temp != NULL) {
                appendWordWithFreq(&parentList, temp->word, temp->freq);
                temp = temp->next;
            }
        }
        wroteListToFile(argv[2], parentList, topKNum);
    } else {
        // Create empty file
        wroteListToFile(argv[2], NULL, topKNum);
    }

    // Deallocate lists
    for (int i = 0; i < count; ++i) {
        freeList(list[i]);
    }
    free(list);

    // Deallocate input file name list
    free(inpFileNames);

    return 0;
}
