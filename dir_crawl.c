#include "dir_crawl_helper.h"
#include <stdbool.h>
#include <pthread.h>
#include <inttypes.h>

#define NUM_THREADS 5

// Node in the linked list
struct node {
        void* pData;
        struct node* pNext;
};

struct queue {
        struct node* pHead;
        struct node* pTail;
        int num;
        pthread_mutex_t mutex;
        pthread_cond_t cv;
};

struct queue workQueue;

enum wstate {
        WORKER_INIT = 0,
        WORKER_BUSY = 1,
        WORKER_DONE = 2
};

struct thread_info {
        pthread_t tid;
        int tnum;
        enum wstate state;
};

struct thread_info threads[NUM_THREADS];

int t_file_count = 0;
pthread_mutex_t tfcMtx = PTHREAD_MUTEX_INITIALIZER;

bool allWorkersDone = false;

void
enQueue(struct queue* q, void* pData)
{
        if (NULL == q) {
                printf("Invalid argument\n");
                exit(-1);
        }

        struct node* node = (struct node*)malloc(sizeof(struct node));
        if (!node) {
                printf("Failed to allocate new node\n");
                exit(1);
        }

        node->pData = pData;
        node->pNext = NULL;

        pthread_mutex_lock(&(q->mutex));

        if (NULL == q->pTail) {
                q->pTail = node;
                q->pHead = node;
        } else {
                q->pTail->pNext = node;
                q->pTail = node;
        }

        q->num++; // Queue is no longer empty

        pthread_cond_signal(&(q->cv));
        pthread_mutex_unlock(&(q->mutex));
}

struct node*
deQueue(struct queue* q)
{
        struct node* pElem = NULL;
        if (NULL == q) {
                printf("Invalid argument\n");
                exit(-1);
        }

        //        pthread_mutex_lock(&(q->mutex));

        if (NULL == q->pHead) {
                // Empty queue
                return NULL;
        }

        if (q->pHead == q->pTail) {
                // Only one element in the queue being removed
                pElem = q->pHead;
                q->pHead = NULL;
                q->pTail = NULL;
                pElem->pNext = NULL;
        } else {
                pElem = q->pHead;
                q->pHead = q->pHead->pNext;
                pElem->pNext = NULL;
        }

        q->num--;

        //pthread_mutex_unlock(&(q->mutex));

        return pElem;
}

static void
dir_crawl(char *root)
{
        //printf("Starting dir_crawl\n");
        enQueue(&workQueue, root);
}

const char* printState(enum wstate st)
{
        switch(st) {
        case WORKER_INIT:
                return "WORKER_INIT";
        case WORKER_BUSY:
                return "WORKER_BUSY";
        case WORKER_DONE:
                return "WORKER_DONE";
        default:
                return "Unknown state";
        }
}

static void
threadCrawlHelper(char *file, struct thread_info* pTInfo)
{
	char *subfile = NULL;

        //printf("[tid=%d] Working on path=%s\n", pTInfo->tnum, file);

        if (file_type(file) == FILE_TYPE_DIR) { 
                DIR *dir = opendir(file);

                if (dir == NULL) {
                        printf("opendir syscall failed\n");
                        exit(-1);
                }

                while ((subfile = next_file(file, dir)) != NULL) {
                        if (file_type(subfile) == FILE_TYPE_DIR) {
                                //printf("[tid=%d] Found DIR  subfile=%s is a directory, enqueuing\n", pTInfo->tnum, subfile);
                                enQueue(&workQueue, subfile);
                        } else {
                                //printf("[tid=%d] Found FILE subfile=%s is as a file\n", pTInfo->tnum, subfile);
                                free(subfile);
                                pthread_mutex_lock(&tfcMtx);
                                t_file_count++; // Anything other than DIR, treat it as regular file.
                                pthread_mutex_unlock(&tfcMtx);
                        }
                }

                //printf("[tid=%d] Done crawling directory\n", pTInfo->tnum);
                closedir(dir);
        } else {
                //printf("Error condition in enqueue, only directories should be enqueued\n");
                // ASSERT here
        }

        free(file); // Free the dequeued string
}

static void*
threadEntryFunc(void *args)
{
        // Do nothing for now
        struct thread_info* pTInfo = (struct thread_info*)args;

        //printf("[tid=%d] Starting work, state=%s\n", pTInfo->tnum, printState(pTInfo->state));

        // Pick up work items if queue is not empty and process them
        pthread_mutex_lock(&(workQueue.mutex));
        while (!allWorkersDone) {
                if (0 == workQueue.num) {
                        //printf("[tid=%d] Waiting for work\n", pTInfo->tnum);

                        struct timeval now;
                        gettimeofday(&now, NULL);
                        long int abstime_ns_large = now.tv_usec*1000 + 2000000;
                        struct timespec abstime = { now.tv_sec + (abstime_ns_large / 1000000000), abstime_ns_large % 1000000000 };
                        pthread_cond_timedwait(&(workQueue.cv), &(workQueue.mutex), &abstime);

                        //printf("[tid=%d] Thread woken up, state=%s, queueItems=%d\n", pTInfo->tnum, printState(pTInfo->state), workQueue.num);
                }

                // Dequeue work item from queue, we hold lock when extracting work items
                struct node* pElem = deQueue(&workQueue);

                if (NULL == pElem) {
                        // Someone else raced to remove element from the queue
                        // OR the queue is empty
                        if (allWorkersDone && (0 == workQueue.num)) {
                                // ASSERT that the workQueue is empty
                                //printf("[tid=%d] All workers done\n", pTInfo->tnum);

                                break;
                        }

                        //printf("[tid=%d] Someone else dequeued before us, back to waiting for work, state=%s\n", pTInfo->tnum, printState(pTInfo->state));
                        continue;
                }

                // Change the worker state to busy
                pTInfo->state = WORKER_BUSY;

                // Unlock before doing the actual crawl (because we can call enQueue there which needs this mutex again)
                pthread_mutex_unlock(&workQueue.mutex);

                threadCrawlHelper((char *)(pElem->pData), pTInfo);

                pthread_mutex_lock(&workQueue.mutex);

                pTInfo->state = WORKER_DONE;
                bool allDone = false;
                int i = 0;
                for (i = 0; i < NUM_THREADS ; i++) {
                        if (i == pTInfo->tnum) {
                                //printf("[tid=%d] Skipping check for thread: %d in state: %s\n", pTInfo->tnum, i, printState(threads[i].state));
                                continue;
                        }

                        if (WORKER_BUSY == threads[i].state) {
                                //printf("[tid=%d] Found worker %d in state WORKER_BUSY\n", pTInfo->tnum, i);
                                allDone = false;
                                break;
                        } else {
                                // Threads in DONE or INIT state denote work is done
                                //printf("[tid=%d] Found worker %d in state %s\n", pTInfo->tnum, i, printState(threads[i].state));
                                allDone = true;
                        }
                }
                if (allDone) {
                        //printf("[tid=%d] Marking all workers as done\n", pTInfo->tnum);
                        allWorkersDone = true;
                } else {
                        //printf("[tid=%d] All workers NOT yet done\n", pTInfo->tnum);
                }

                //printf("[tid=%d] Thread done, state=%s\n", pTInfo->tnum, printState(pTInfo->state));
        } // end while
        pthread_mutex_unlock(&workQueue.mutex);

        //printf("[tid=%d] Thread exiting, state=%s\n", pTInfo->tnum, printState(pTInfo->state));

        return NULL;
}

int
main(int argc, char const *argv[])
{
        int i = 0;
        int file_count = 0;

	if (argc != 2) {
		printf("Usage: %s <directory name>\n", argv[0]);
		return -1;
	}

        pthread_mutex_init(&workQueue.mutex, NULL);
        pthread_cond_init(&workQueue.cv, NULL);
        workQueue.num = 0;
        workQueue.pHead = NULL;
        workQueue.pTail = NULL;

        for (i = 0; i < NUM_THREADS; i++) {
                threads[i].tnum = i;
                threads[i].state = WORKER_INIT;
                int ret = pthread_create(&threads[i].tid, NULL, &threadEntryFunc, &threads[i]);
                if (0 != ret) {
                        printf("Failure in creating the thread\n");
                }
        }

        //printf("Starting crawl\n");

	char *root_dir = (char *)argv[1];

	char *root = (char *)malloc(strlen(argv[1] + 1));
	strcpy(root, root_dir);

	dir_crawl(root);

        for (i = 0; i < NUM_THREADS; i++) {
                int ret = pthread_join(threads[i].tid, NULL);
                if (0 != ret) {
                        printf("Failure in joining the thread\n");
                }
        }

        file_count = t_file_count;

	if (file_count < 0) {
		printf("Failed to read the directory - %s\n", root_dir);
		return -1;
	}
	
	printf("Total File count in directory %s = %d\n", root_dir, file_count);

        pthread_cond_destroy(&workQueue.cv);
        pthread_mutex_destroy(&workQueue.mutex);

	return 0;
}
