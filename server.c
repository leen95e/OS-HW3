#include "segel.h"
#include "request.h"
#include "log.h"

typedef struct{
    int connfd;
    struct timeval arrivalTime; 
} job_t;

typedef struct{
    job_t* jobsArray;
    int head;
    int tail;
    int pendingRequest;
    int workingRequest;
    int capacity;
    
    pthread_mutex_t mutex;
    pthread_cond_t isEmpty;
    pthread_cond_t isFull;

} threadPool;

threadPool pool;
server_log request_log;



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
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queueSize = atoi(argv[3]);

    if (argc > 4){
        *sleepTime = atoi(argv[4]);
    }else{
        *sleepTime = 0;
    }
}
// TODO: HW3 — Initialize thread pool and request queue
// This server currently handles all requests in the main thread.
// You must implement a thread pool (fixed number of worker threads)
// that process requests from a synchronized queue.


void* workerThread(void* arg) {
    int threadId = *(int*)arg;
    free(arg);
    threads_stats t = malloc(sizeof(struct Threads_stats));
    t->id = threadId;
    t->stat_req = 0;
    t->dynm_req = 0;
    t->total_req = 0;
    
    while(1){
        pthread_mutex_lock(&pool.mutex);
        while(pool.pendingRequest == 0){
            pthread_cond_wait(&pool.isEmpty, &pool.mutex);
        }
        job_t job = pool.jobsArray[pool.head];
        pool.head = (pool.head + 1) % pool.capacity;
        pool.pendingRequest--;
        pool.workingRequest++;
        
        pthread_mutex_unlock(&pool.mutex);

        time_stats timeStats;
        timeStats.task_arrival = job.arrivalTime;
        gettimeofday(&timeStats.task_dispatch, NULL);
        requestHandle(job.connfd, timeStats, t, request_log);

        pthread_mutex_lock(&pool.mutex);
        pool.workingRequest--;
        pthread_cond_signal(&pool.isFull);
        pthread_mutex_unlock(&pool.mutex);
        Close(job.connfd);
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    // Create the global server log

    int listenfd, connfd, port, clientlen, threadsCount, queueSize, sleepTime;
    struct sockaddr_in clientaddr;

    getargs(&port, &threadsCount, &queueSize, &sleepTime, argc, argv);
    server_log requeslog = create_log(sleepTime);


    pool.capacity = queueSize;
    pool.jobsArray= malloc(sizeof(job_t)*queueSize);
    pool.head = 0;
    pool.tail = 0;
    pool.pendingRequest = 0;
    pool.workingRequest = 0;

    pthread_mutex_init(&pool.mutex, NULL);
    pthread_cond_init(&pool.isEmpty, NULL);
    pthread_cond_init(&pool.isFull, NULL);

    for (int i = 0; i < threadsCount; i++){
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_t tid;
        pthread_create(&tid, NULL, workerThread, id);
    }

    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t*) &clientlen);

        struct timeval arrival;
        gettimeofday(&arrival, NULL);
        pthread_mutex_lock(&pool.mutex);

        while(pool.workingRequest + pool.pendingRequest == pool.capacity){
            pthread_cond_wait(&pool.isFull, &pool.mutex);
        }

        pool.jobsArray[pool.tail].connfd = connfd;
        pool.jobsArray[pool.tail].arrivalTime= arrival;
        pool.tail = (pool.tail + 1) % pool.capacity;
        pool.pendingRequest++;

        pthread_cond_signal(&pool.isEmpty);
        pthread_mutex_unlock(&pool.mutex);
    }

    // Clean up the server log before exiting
    free(pool.jobsArray);
    destroy_log(requeslog);

    // TODO: HW3 — Add cleanup code for thread pool and queue
}

