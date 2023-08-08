#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORD_LENGTH 64
struct Node {
    char word[MAX_WORD_LENGTH];
    int freq;
    struct Node *next;
    struct Node *prev;
};

int getSize(struct Node* headNode);
void freeList(struct Node *headNode);

void appendWordWithFreq(struct Node **head_ref, char nword[], int incValue);

void append(struct Node **head_ref, char nword[]);

struct Node* sortList(struct Node* node_ref);

void wroteListToFile(char *outFileName, struct Node *list, int topKNum);
#endif