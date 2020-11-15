#include <stdio.h> 
#include <stdlib.h> 
  
struct entry { 
    int val; 
    struct entry* next; 
}; 
  
struct queue { 
    int maxlen;
    int currlen;
    struct entry *head;
    struct entry *tail; 
}; 
  
struct entry* newentry(int val) 
{ 
    struct entry* temp = (struct entry*)malloc(sizeof(struct entry)); 
    temp->val = val; 
    temp->next = NULL; 
    return temp; 
} 

struct entry* dequeue(struct queue* q) 
{  
    if (q->head == NULL) 
        return NULL; 
   
    struct entry* temp = q->head; 
  
    q->head = q->head->next; 
   
    if (q->head == NULL) 
        q->tail = NULL; 
  
    return temp; 
}

void decaptiate(struct queue* q)
{
    struct entry* temp = dequeue(q);
    free(temp);
}

struct queue* createqueue(int maxlen) 
{ 
    struct queue* q = (struct queue*)malloc(sizeof(struct queue));
    q->maxlen = maxlen;
    q->currlen = 0; 
    q->head = NULL;
    q->tail = NULL; 
    return q; 
} 
  
void enqueue(struct queue* q, int val) 
{  
    struct entry* temp = newentry(val); 
    if (q->tail == NULL) { 
        q->head = temp;
        q->tail = temp; 
        return; 
    } 
   
    q->tail->next = temp; 
    q->tail = temp;
    q->currlen++;
    if (q->currlen > q->maxlen)
        decaptiate(q); 
} 

void freequeue(struct queue* q)
{
    for (int i = 0; i < q->currlen; i++) {
        decaptiate(q);
    }

    free(q);
}
