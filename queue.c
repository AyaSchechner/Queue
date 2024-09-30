#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "queue.h"

typedef struct node{
    struct node* next;
    void* data;
}node;

typedef struct t_node{
    struct t_node* next;
    void* data;
    cnd_t cond;
}t_node;

typedef struct queue{
    node* queue_head;
    node * queue_tail;
}queue;

typedef struct t_queue{
    t_node* queue_head;
    t_node * queue_tail;
}t_queue;

struct queue my_queue;
struct t_queue thread_queue;
mtx_t mutex;
atomic_size_t total_visit;
atomic_size_t thread_wait;
atomic_size_t queue_size;


void initQueue(void){
    mtx_init(&mutex, mtx_plain);
    mtx_lock(&mutex);
    my_queue.queue_head = my_queue.queue_tail = NULL;
    thread_queue.queue_head = thread_queue.queue_tail = NULL;
    queue_size = 0;
    total_visit = 0;
    thread_wait = 0;
    mtx_unlock(&mutex);
}

void destroyQueue(void){
    mtx_lock(&mutex);
    node* curr; 
    node* temp;
    t_node* t_curr; 
    t_node* t_temp;

    curr = my_queue.queue_head;
    while (curr != NULL){
        temp = curr;
        curr = curr->next;
        free(temp);
    }
    my_queue.queue_head = my_queue.queue_tail = NULL;
    t_curr = thread_queue.queue_head;
    while (t_curr != NULL){
        t_temp = t_curr;
        t_curr = t_curr->next;
        free(t_temp);
    }
    thread_queue.queue_head = thread_queue.queue_tail = NULL;
    queue_size = 0;
    thread_wait = 0;
    mtx_unlock(&mutex);
    mtx_destroy(&mutex);
}


void enqueue(void* data){
    mtx_lock(&mutex);
    t_node* thread;
    node* newNode;
    // threads waiting
    if(thread_queue.queue_head != NULL){
        thread = thread_queue.queue_head;
        // at least 2 threads
        if (thread->next != NULL){
            thread_queue.queue_head = thread->next;
        }
        // only 1 thread
        else{
            thread_queue.queue_head = thread_queue.queue_tail = NULL;
        }
        thread->data = data;
        cnd_signal(&thread->cond);
    }
    // no threads waiting
    else{
        newNode = (node*)malloc(sizeof(node));
        newNode->data = data;
        newNode->next = NULL;
        // element queue is empty
        if(queue_size == 0){
            my_queue.queue_head = my_queue.queue_tail = newNode;
        } 
        else{
            my_queue.queue_tail->next = newNode;
            my_queue.queue_tail = newNode;
        }
    }
    queue_size++;
    mtx_unlock(&mutex);
}


void* dequeue(void){
    mtx_lock(&mutex);
    t_node* thread;
    node* currNode;
    void* data;
    // main queue is empty
    if(my_queue.queue_head == NULL){
        thread = (t_node*)malloc(sizeof(t_node));
        thread->next = NULL;
        cnd_init(&thread->cond);
        // threads waiting
        if (thread_queue.queue_head != NULL){
            thread_queue.queue_tail->next = thread;
            thread_queue.queue_tail = thread;
        }
        // no threads waiting
        else{
            thread_queue.queue_head = thread_queue.queue_tail = thread;
        }
        thread_wait++;
        cnd_wait(&thread->cond, &mutex);
        data = thread->data;
        thread_wait--;
        cnd_destroy(&thread->cond);
        free(thread);
        total_visit++;
        queue_size--;
    }
    // main queue not empty
    else{
        currNode = my_queue.queue_head;
        // one node to dequeue
        if (currNode->next == NULL){
            my_queue.queue_head = my_queue.queue_tail = NULL;
        }
        else{
            my_queue.queue_head = currNode->next;
        }
        data = currNode->data;
        free(currNode);
        total_visit++;
        queue_size--;
    }
    mtx_unlock(&mutex);
    return data;
}


bool tryDequeue(void** data){
    mtx_lock(&mutex);
    // queue is empty
    if (my_queue.queue_head == NULL){
        mtx_unlock(&mutex);
        return false;
    }
    node* currNode = my_queue.queue_head;
    my_queue.queue_head = currNode->next;
    // queue is now empty
    if (currNode->next == NULL){
        my_queue.queue_tail = NULL;
    }
    *data = currNode->data;
    free(currNode);
    total_visit++;
    queue_size--;
    mtx_unlock(&mutex);
    return true;
}


size_t size(void){
    return (size_t)queue_size;
}


size_t waiting(void){
    return (size_t)thread_wait;
}


size_t visited(void){
    return (size_t)total_visit;
}

    