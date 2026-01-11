#include <stdlib.h>
#include <string.h>
#include "log.h"
#include <pthread.h>
#include <unistd.h>

 struct Log_item {
    char* data;
    int len;
    struct Log_item* next;
} ;

// Opaque struct definition
struct Server_Log {
    pthread_mutex_t mutex;
    pthread_cond_t readAllowed;
    pthread_cond_t writeAllowed;

    int readersInside;
    int writersInside;
    int writersWaiting;
    struct Log_item* head;
    struct Log_item* tail;
    int size;
    int num_char;

    int sleepTime;
};

// Creates a new server log instance (stub)
server_log create_log(int sleepTime) {
    // TODO: Allocate and initialize internal log structure
    server_log log = malloc(sizeof(struct Server_Log));
    if (!log){
        return NULL;
    }
    
    pthread_mutex_init(&log->mutex, NULL);
    pthread_cond_init(&log->readAllowed, NULL);
    pthread_cond_init(&log->writeAllowed, NULL);

    log->readersInside = 0;
    log->writersInside = 0;
    log->writersWaiting = 0;
    log->size = 0;
    log->num_char = 0;
    log->head =  NULL;
    log->tail = NULL;

    log->sleepTime = sleepTime;

    return log;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    if (!log) return;   

    pthread_mutex_destroy(&log->mutex);
    pthread_cond_destroy(&log->readAllowed);
    pthread_cond_destroy(&log->writeAllowed);

   struct Log_item* current = log->head;
    while (current != NULL) {
        struct Log_item* next = current->next;
        free(current->data); 
        free(current);     
        current = next;
    }
    free(log);

}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst, time_stats* tm_stats) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access
    gettimeofday(&tm_stats->log_enter, NULL);
    pthread_mutex_lock(&log->mutex);

    while (log->writersInside > 0 || log->writersWaiting > 0){
        pthread_cond_wait(&log->readAllowed, &log->mutex);
    }
    log->readersInside++;
    pthread_mutex_unlock(&log->mutex);
    gettimeofday(&tm_stats->log_exit, NULL);
    
    *dst = malloc(log->num_char + log->size + 1);
    if (*dst != NULL) {
        char* ptr = *dst;
        struct Log_item* current = log->head;
        while (current != NULL) {
            memcpy(ptr, current->data, current->len);
            ptr += current->len; 
            *ptr = '\n';
            ptr++;
            current = current->next;
        }
        *ptr = '\0'; //null terminate
    }
    if (log->sleepTime > 0) {
        usleep(log->sleepTime * 1000000);
    }
    // 3. Reader Exit Protocol
    pthread_mutex_lock(&log->mutex);
    log->readersInside--;
    if (log->readersInside == 0) {
        pthread_cond_signal(&log->writeAllowed);
    }
    int len = log->num_char + log->size;
    pthread_mutex_unlock(&log->mutex);
    return len ;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len, time_stats* tm_stats) {
    // TODO: Append the provided data to the log
    // This function should handle concurrent access
    // if (!log || !data){
    //     return;
    // }
    gettimeofday(&tm_stats->log_enter, NULL);  
    pthread_mutex_lock(&log->mutex);

    log->writersWaiting++;

    while (log->writersInside > 0 || log->readersInside > 0)
    {
        pthread_cond_wait(&log->writeAllowed , &log->mutex);
    }
    log->writersWaiting--;
    log->writersInside++;
    pthread_mutex_unlock(&log->mutex);
    gettimeofday(&tm_stats->log_exit, NULL);

    struct Log_item* item = (struct Log_item*)malloc(sizeof(struct Log_item));
    item->data = (char*) malloc(data_len + 1);
    memcpy(item->data,data,data_len);
    item->data[data_len] = '\0';
    item->len = data_len;
    item->next = NULL;
    if (log->head == NULL) {
        log->head = item;
        log->tail = item;
    } else {
        log->tail->next = item;
        log->tail = item;
    }

    log->size++;
    log->num_char += data_len;
    if (log->sleepTime > 0) {
        usleep(log->sleepTime * 1000000);
    }
        
    pthread_mutex_lock(&log->mutex);
    log->writersInside--;
    if (log->writersWaiting > 0){
        pthread_cond_signal(&log->writeAllowed);
    }else {
        pthread_cond_broadcast(&log->readAllowed);
    }
    pthread_mutex_unlock(&log->mutex);
}