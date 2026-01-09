#include <stdlib.h>
#include <string.h>
#include "log.h"
#include <pthread.h>

// Opaque struct definition
struct Server_Log {
    pthread_mutex_t mutex;
    pthread_cond_t readAllowed;
    pthread_cond_t writeAllowed;

    int readersInside;
    int writersInside;
    int writersWaiting;

    char* data;
    int size;
};

// Creates a new server log instance (stub)
server_log create_log() {
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

    log->data =  NULL;
    log->size = 0;

    return log;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    if (!log) return;   

    pthread_mutex_destroy(&log->mutex);
    pthread_cond_destroy(&log->readAllowed);
    pthread_cond_destroy(&log->writeAllowed);

    if (log->data) {
        free(log->data);
    }
    free(log);

}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access
    pthread_mutex_lock(&log->mutex);

    while (log->writersInside > 0 || log->writersWaiting > 0){
        pthread_cond_wait(&log->readAllowed, &log->mutex);
    }
    log->readersInside++;

    *dst = malloc(log->size + 1);
    if (*dst && log->data) {
        memcpy(*dst, log->data, log->size);
        (*dst)[log->size] = '\0';
    } else if (*dst) {
         (*dst)[0] = '\0'; // לוג ריק
    }
    int len = log->size;

    // 3. Reader Exit Protocol
    log->readersInside--;
    if (log->readersInside == 0) {
        pthread_cond_signal(&log->writeAllowed);
    }
    pthread_mutex_unlock(&log->mutex);
    return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    // TODO: Append the provided data to the log
    // This function should handle concurrent access
    if (!log || !data){
        return;
    }

    pthread_mutex_lock(&log->mutex);

    log->writersWaiting++;

    while (log->writersInside + log->readersInside > 0)
    {
        pthread_cond_wait(&log->writeAllowed , &log->mutex);
    }
    log->writersWaiting--;
    log->writersInside++;

    int newSize = log->size + data_len;
    char* newData = malloc (newSize+1);

    if (newData){
        if(log->data){
            memcpy(newData, log->data, log->size);
            free(log->data);
        }
        memcpy(newData + log->size, data, data_len);
        newData[newSize] = '\0';

        log->data = newData;
        log->size = newSize;
    }

    log->writersInside--;
    if (log->writersWaiting > 0){
        pthread_cond_signal(&log->writeAllowed);
    }else {
        pthread_cond_broadcast(&log->readAllowed);
    }
    pthread_mutex_unlock(&log->mutex);
}
