#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t mutex_main, mutex_thread, mutex_stdout;

void mutex_init(pthread_mutex_t* curr_mutex) {
    pthread_mutexattr_t attrs;
    int error;
    if ((error = pthread_mutexattr_init(&attrs)) != 0) {
        fprintf(stderr, "Failed to init mutex attributes: %s\n", strerror(error));
        exit(-1);
    }

    if ((error = pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_ERRORCHECK)) != 0) {
        fprintf(stderr, "Failed to set mutex attributes: %s\n", strerror(error));
        exit(-1);
    }

    if ((error = pthread_mutex_init(curr_mutex, &attrs)) != 0) {
        fprintf(stderr, "Failed to init mutex: %s\n", strerror(error));
        exit(-1);
    }
}

void mutex_lock(pthread_mutex_t* curr_mutex) 
{
    int error;
    if ((error = pthread_mutex_lock(curr_mutex)) != 0) {
        fprintf(stderr, "Failed to lock mutex: %s\n", strerror(error));
        exit(-1);
    }
}

void mutex_unlock(pthread_mutex_t* curr_mutex) {
    int error;
    if ((error = pthread_mutex_unlock(curr_mutex)) != 0) {
        fprintf(stderr, "Failed to unlock mutex: %s\n", strerror(error));
        exit(-1);
    }
}

void* thread_body(void* param) {
    mutex_lock(&mutex_stdout);
    
    for (int i = 0; i < 10; i++) {
        mutex_lock(&mutex_main);
        mutex_unlock(&mutex_stdout);
        mutex_lock(&mutex_thread);
        
        printf("Child\n");
        
        mutex_unlock(&mutex_main);
        mutex_lock(&mutex_stdout);
        mutex_unlock(&mutex_thread);
    }
    
    mutex_unlock(&mutex_stdout);
    
    return NULL;
}

int main (int argc, char* argv[]) {
    pthread_t thread;
    mutex_init(&mutex_main);
    mutex_init(&mutex_stdout);
    mutex_init(&mutex_thread);
    int code;

    mutex_lock(&mutex_main);
    mutex_lock(&mutex_thread);
    
    if ((code = pthread_create(&thread, NULL, thread_body, NULL)) != 0) {
        fprintf(stderr, "%s: creating thread: %s\n", argv[0], strerror(code));
        exit(1);
    }    
    
    for (int i = 0; i < 10; i++) {
        printf("Parent\n");
        
        mutex_unlock(&mutex_main);
        mutex_lock(&mutex_stdout);
        mutex_unlock(&mutex_thread);
        mutex_lock(&mutex_main);
        mutex_unlock(&mutex_stdout);
        mutex_lock(&mutex_thread);
    }   

    mutex_unlock(&mutex_main);
    mutex_unlock(&mutex_thread);

    pthread_join(thread, NULL);

    if ((code = pthread_mutex_destroy(&mutex_main)) != 0) {
        fprintf(stderr, "Failed to destroy mutex: %s\n", strerror(code));
        exit(-1);
    }
    
    if ((code = pthread_mutex_destroy(&mutex_thread)) != 0) {
        fprintf(stderr, "Failed to destroy mutex: %s\n", strerror(code));
        exit(-1);
    }
    
    if ((code = pthread_mutex_destroy(&mutex_stdout)) != 0) {
        fprintf(stderr, "Failed to destroy mutex: %s\n", strerror(code));
        exit(-1);
    }

    return 0;    
}
