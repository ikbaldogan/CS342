#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "linkedList.h"


#define SNAME "shmname"
#define MAXWORDLENGTH 64        // max length of a word


char **inpFileNames;

int main(int argc, char *argv[]) {
    pid_t n;
    int k = atoi(argv[1]); // 1 < K < 1000 number of words to find
    int count = atoi(argv[3]); // 1 < N < 10   number of input files

    char word[64];
    struct Node *child_list;
    child_list = NULL;

    struct Node *parent_list;
    parent_list = NULL;

    char in_name[64];
    FILE *in_file;

    inpFileNames = malloc(sizeof(char *) * count);

    if (!inpFileNames) { /* If list == 0 after the call to malloc, allocation failed for some reason */
        perror("Error allocating memory");
        abort();

    }
    for (int i = 0; i < count; ++i) {
        inpFileNames[i] = argv[4 + i];
    }

    const int SEGSIZE = sizeof(int) + k * ((sizeof(char) * MAXWORDLENGTH) + sizeof(int));
    const int TOTALSIZE = SEGSIZE * count;// count = N ; NUMBER OF INPUT FILES
    /* shared memory file descriptor */
    int shm_fd;
    shm_fd = shm_open(SNAME, O_CREAT | O_RDWR, 0666);

    /* pointer to shared memory object */
    void *ptr;

    /* configure the size of the shared memory object */
    ftruncate(shm_fd, TOTALSIZE);

    if (TOTALSIZE == 0){
        // Create empty file
        wroteListToFile(argv[2], NULL, k);
        return 0;
    }


    /* memory map the shared memory object */
    ptr = mmap(0, TOTALSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (ptr == MAP_FAILED) {
        printf("Map failed\n");
        return -1;
    }


    //fork
    for (int i = 0; i < count; ++i) // count is N
    {
        n = fork();
        if (n < 0) {
            printf("fork() failed\n");
            exit(-1);
        }
        //child code
        if (n == 0) {
            strcpy(in_name, inpFileNames[i]);
            in_file = fopen(in_name, "r");
            if (in_file == NULL)
                printf("Can't open %s for reading.\n", in_name);

            else {
                while (fscanf(in_file, "%s", word) != EOF) {
                    // Convert word to uppercase
                    for (int j = 0; word[j] != '\0'; j++) {
                        if (word[j] >= 'a' && word[j] <= 'z') {
                            word[j] = word[j] - 32;
                        }
                    }
                    append(&child_list, word);
                }
                fclose(in_file);
            }

            ptr += i * SEGSIZE; // ptr points to current child's memory adress

            int child_listSize = getSize(child_list);

            *((int *) ptr) = child_listSize;
            ptr += sizeof(int);
            struct Node *tmp = child_list; // temp pointer to traverse list
            for (int m = 0; m < k && m < child_listSize; m++) {
                sprintf(ptr, "%s", tmp->word);
                ptr += sizeof(char) * MAXWORDLENGTH;
                *((int *) ptr) = tmp->freq;
                ptr += sizeof(int);
                tmp = tmp->next;
            }

            // deallocate memory
            freeList(child_list);

            // Deallocate input file name list
            free(inpFileNames);
            exit(0);
        }

            //parent code
        else {

        }

    }

    // wait for all children to terminate
    for (int i = 0; i < count; ++i)
        wait(NULL);

    char tmpWord[64];
    int tmpFreq;
    for (int m = 0; m < count; ++m) {
        void *bfrPtr = ptr;
        int inpSize = *(int *) ptr;
        if (k < inpSize) {
            inpSize = k;
        }
        ptr += sizeof(int);
        for (int l = 0; l < inpSize; ++l) {
            strcpy(tmpWord, (char *) ptr);
            ptr += sizeof(char) * MAXWORDLENGTH;
            tmpFreq = *(int *) ptr;
            ptr += sizeof(int);

            appendWordWithFreq(&parent_list, tmpWord, tmpFreq);
        }
        ptr = bfrPtr;
        ptr += SEGSIZE;
    }

    if (shm_unlink(SNAME) == -1) {
        printf("Error removing %s\n", SNAME);
        exit(-1);
    }


    // Print Parent list
    if (count > 0) {
        wroteListToFile(argv[2], parent_list, k);
    } else {
        // Create empty file
        wroteListToFile(argv[2], NULL, k);
    }

    // Deallocate parent list
    freeList(parent_list);

    // Deallocate input file name list
    free(inpFileNames);

    return 0;
}