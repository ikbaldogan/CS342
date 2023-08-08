#include "linkedList.h"


int getSize(struct Node *headNode) {
    int size = 0;
    struct Node *tmp;
    tmp = headNode;
    while (tmp != NULL) {
        size++;
        tmp = tmp->next;
    }
    return size;

}

void freeList(struct Node *headNode) {
    struct Node *currentNode;
    while (headNode != NULL) {
        currentNode = headNode;
        headNode = headNode->next;
        free(currentNode);
    }
}

void append(struct Node **head_ref, char nword[]) {
    appendWordWithFreq(head_ref, nword, 1);
}

void appendWordWithFreq(struct Node **head_ref, char nword[], int incValue) {

    //CHECK IF LIST IS NULL
    if (*head_ref == NULL) {
        struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));
        strcpy(new_node->word, nword);
        new_node->freq = incValue;
        new_node->next = NULL;
        new_node->prev = NULL;
        *head_ref = new_node;
        return;
    }


    struct Node *tmp = *head_ref;
    int found = -1;
    //CHECKs IF WORD EXISTS ON THE LIST
    while (tmp != NULL) {
        int result = strcmp(tmp->word, nword);
        if (result != 0)
            tmp = tmp->next;
        else {
            tmp->freq += incValue;
            tmp = sortList(tmp);
            if (tmp->prev == NULL)
                *head_ref = tmp;
            found = 1;
            break;
        }

    }
    //if word does not exist
    if (found == -1) {
        tmp = *head_ref;
        while (tmp->next != NULL)
            tmp = tmp->next;
        struct Node *new_node = (struct Node *) malloc(sizeof(struct Node));
        strcpy(new_node->word, nword);
        new_node->freq = incValue;
        new_node->next = NULL;
        new_node->prev = tmp;
        tmp->next = new_node;
        tmp = tmp->next;
        tmp = sortList(tmp);
        if (tmp->prev == NULL)
            *head_ref = tmp;
    }
}

struct Node *sortList(struct Node *node_ref) {
    struct Node *tmpNode2;
    tmpNode2 = node_ref;
    while (tmpNode2->prev != NULL) {
        if (tmpNode2->freq > tmpNode2->prev->freq ||
            (tmpNode2->freq == tmpNode2->prev->freq && strcmp(tmpNode2->word, tmpNode2->prev->word) < 0)) {
            int tmpint;
            char tmpWord[64];
            struct Node *tmpNode;

            tmpNode = tmpNode2->prev;
            tmpint = tmpNode->freq;
            strcpy(tmpWord, tmpNode->word);

            tmpNode->freq = tmpNode2->freq;
            strcpy(tmpNode->word, tmpNode2->word);

            tmpNode2->freq = tmpint;
            strcpy(tmpNode2->word, tmpWord);

            tmpNode2 = tmpNode;
        } else {
            break;
        }
    }
    return tmpNode2;
}

void wroteListToFile(char *outFileName, struct Node *list, int topKNum) {
    FILE *filePointer;

    filePointer = fopen(outFileName, "w");

    if (filePointer == NULL) {
        printf("File is failed to open.");
    } else {
        // Write the data into the file
        while (list != NULL) {
            fprintf(filePointer, "%s %d", list->word, list->freq);
            topKNum--;
            if (list->next != NULL && topKNum > 0) {
                fputs("\n", filePointer);
                list = list->next;
            } else {
                break;
            }
        }
        fclose(filePointer);
    }
}