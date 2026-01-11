#include "segel.h"
#include "request.h"
#include "log.h"

typedef struct{
    int connfd;
    struct timeval arrivalTime; 
    server_log log;
} job_t;




typedef struct{
    job_t** jobsArray;
    int head;
    int tail;
    int pendingRequest;
    int capacity;

} queue_t;


typedef struct{
    queue_t* queue;
    int* workingRequests;
    server_log log;
    threads_stats t;
} thread_args;


pthread_mutex_t mutex;
pthread_cond_t isEmpty;
pthread_cond_t isFull;

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// Parses command-line arguments
void getargs(int *port, int* threads, int* queueSize, int* sleepTime, int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queueSize = atoi(argv[3]);
    *sleepTime = atoi(argv[4]);
    

    if (*port < 1024 || *port > 65535) {
        fprintf(stderr, "Invalid port number");
        exit(1);
    }

    if (*threads <= 0 || *queueSize <= 0) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size>\n", argv[0]);
        exit(1); // חובה לעשות exit עם שגיאה!
}
}
// TODO: HW3 — Initialize thread pool and request queue
// This server currently handles all requests in the main thread.
// You must implement a thread pool (fixed number of worker threads)
// that process requests from a synchronized queue.

int curSize(queue_t* q){
    return q->pendingRequest;
}

void* workerThread(void* arg) {
    thread_args* args = (thread_args*)arg;

    queue_t* q = args->queue;
    int* wr = args->workingRequests;
    server_log log =  args->log;
    threads_stats stats_t = (threads_stats)args->t;
    free(arg);
    
    while(1){
        pthread_mutex_lock(&mutex);
        while(curSize(q) == 0){
            pthread_cond_wait(&isEmpty, &mutex);
        }
        job_t* job = q->jobsArray[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->pendingRequest--;
        
        (*wr)++;
        struct timeval dispatch_time;

        gettimeofday(&dispatch_time, NULL);

        pthread_mutex_unlock(&mutex);

        time_stats ts;
        ts.task_dispatch = dispatch_time;
        ts.task_arrival = job->arrivalTime;
        
        requestHandle(job->connfd, ts, stats_t, log);

        pthread_mutex_lock(&mutex);
        Close(job->connfd);
        free(job);
        (*wr)--;
        pthread_cond_signal(&isFull);
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}





int main(int argc, char *argv[])
{
    // Create the global server log
    int workingRequest = 0;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&isEmpty, NULL);
    pthread_cond_init(&isFull, NULL);

    int listenfd, connfd, port, clientlen, threadsCount, queueSize, sleepTime;
    struct sockaddr_in clientaddr;

    getargs(&port, &threadsCount, &queueSize, &sleepTime, argc, argv);
    server_log request_log = create_log(sleepTime);

    queue_t queue;

   
    pthread_t* workerHandles = malloc(sizeof(pthread_t) * threadsCount);
    
    if (workerHandles == NULL){
        fprintf(stderr, "malloc failed");
        exit(1);
    }

    queue.capacity = queueSize;
    queue.jobsArray= malloc(sizeof(job_t*)*queueSize);
    queue.head = 0;
    queue.tail = 0;
    queue.pendingRequest = 0;
    
    for (unsigned int i = 0; i < threadsCount; i++){
        thread_args* args = malloc(sizeof(thread_args));
        if (args == NULL){
            fprintf(stderr, "malloc failed");
            exit(1);
        }
        threads_stats stats = (threads_stats)malloc(sizeof(struct Threads_stats));
        if(stats == NULL){
            fprintf(stderr, "malloc failed");
            exit(1);
        }
        stats->id = i + 1;             // Thread ID (placeholder)
        stats->stat_req = 0;       // Static request count
        stats->dynm_req = 0;       // Dynamic request count
        stats->post_req = 0;        // Post request count
        stats->total_req = 0; 
             // Total request count
        args->queue = &queue;
        args->workingRequests = &workingRequest;
        args->log = request_log;
        args->t = stats;
        pthread_create(&workerHandles[i], NULL, workerThread, (void*)args);
    }

    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t*) &clientlen);

        struct timeval arrival;
        gettimeofday(&arrival, NULL);

        pthread_mutex_lock(&mutex);

        while((workingRequest + curSize(&queue)) >= queue.capacity){
            pthread_cond_wait(&isFull, &mutex);
        }
        
        job_t* new_job = malloc(sizeof(job_t));
        if (new_job == NULL){
            fprintf(stderr, "malloc failed");
            exit(1);
        }
        queue.jobsArray[queue.tail] = new_job;
        new_job->connfd = connfd;
        new_job->arrivalTime= arrival;
        queue.tail = (queue.tail + 1) % queue.capacity;
        queue.pendingRequest++;

        pthread_cond_signal(&isEmpty);
        pthread_mutex_unlock(&mutex);
    }

    // Clean up the server log before exiting
    while (queue.pendingRequest){
        job_t* job = queue.jobsArray[queue.head];
        queue.head = (queue.head + 1) % queue.capacity;
        queue.pendingRequest--;
        free(job);
    }
    free(queue.jobsArray);
    destroy_log(request_log);

    for (int i = 0; i < threadsCount; i++) {
        pthread_join(workerHandles[i], NULL);
    }
    free(workerHandles);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&isEmpty);
    pthread_cond_destroy(&isFull);

    // TODO: HW3 — Add cleanup code for thread pool and queue
}

