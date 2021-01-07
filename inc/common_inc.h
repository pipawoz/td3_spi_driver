#ifndef COMMON_INC_H
#define COMMON_INC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/prctl.h>

/* Semaphore primitives */
void sem_initialise(int sem_num, int val);
void sem_take(int sem_num);
void sem_give(int sem_num);


/** Struct para manejo de memoria compartida */
typedef struct
{
    float current_temp;
} shared_mem_t;


typedef struct
{
    uint32_t backlog;
    uint32_t max_conn;
    uint32_t read_interval;
    uint32_t samples;
} config_file_t;


/** Webserver context */
typedef struct
{
    int pipefd[2];
    uint32_t backlog;
    uint32_t max_conn;
    uint32_t read_interval;
    uint32_t samples;
    shared_mem_t *shared_data_1;
    shared_mem_t *shared_data_2;
    config_file_t *config_file;
} ctx_t;


typedef struct
{
    char timestamp[20];
    float temp;
} last_temp_t;


struct pollfd fdinfo[1];

/** IPC flag for file.cfg modifications */
extern bool flag_sigusr1;

#define SEM_MUTEX 0
#define SEM_FULL 1
#define SEM_EMPTY 2


#define ERROR -1
#define READ 0
#define WRITE 1


/* Depuration level define */
#define DEBUG_HIG 2
#define DEBUG_MID 1
#define DEBUG_LOW 0

#define DEBUG_LVL DEBUG_HIG

#ifndef DEBUG_LVL
#    define DEBUG_LVL (DEBUG_HIG + 1)
#endif

#define TRACE_MSG(msg, ...)                    \
    fprintf(stderr,                            \
            "[%s:%s:%d]-(T%ld:S%d:F%d)::" msg, \
            __FILE__,                          \
            __func__,                          \
            __LINE__,                          \
            syscall(SYS_gettid),               \
            getpid(),                          \
            getppid(),                         \
            ##__VA_ARGS__);

#if(DEBUG_LVL == DEBUG_HIG)
#    define TRACE_HIG(msg, ...) TRACE_MSG(msg, ##__VA_ARGS__)
#    define TRACE_MID(msg, ...)
#    define TRACE_LOW(msg, ...)
#elif(DEBUG_LVL == DEBUG_MID)
#    define TRACE_HIG(msg, ...) TRACE_MSG(msg, ##__VA_ARGS__)
#    define TRACE_MID(msg, ...) TRACE_MSG(msg, ##__VA_ARGS__)
#    define TRACE_LOW(msg, ...)
#elif(DEBUG_LVL == DEBUG_LOW)
#    define TRACE_HIG(msg, ...) TRACE_MSG(msg, ##__VA_ARGS__)
#    define TRACE_MID(msg, ...) TRACE_MSG(msg, ##__VA_ARGS__)
#    define TRACE_LOW(msg, ...) TRACE_MSG(msg, ##__VA_ARGS__)
#else
#    define TRACE_HIG(msg, ...)
#    define TRACE_MID(msg, ...)
#    define TRACE_LOW(msg, ...)
#endif


#endif