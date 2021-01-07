/**
 * @file functions.c
 * @author Pedro Wozniak Lorice (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief auxiliar functions for webserver.c
 * @version 0.1
 * @date 2019-11-14
 *
 * @copyright Copyright (c) 2019
 *
 */

#include "../../inc/functions.h"


/**
 * @brief Function to initialize the semaphores
 *
 * @param sem_num Semaphore number
 * @param val Value to assign
 */
void sem_initialise(int sem_num, int val)
{
    union semun semaphore_semun;
    semaphore_semun.val = val;
    if(semctl(semaphore_set, sem_num, SETVAL, semaphore_semun) == ERROR)
    {
        perror("semctl error");
        exit(EXIT_FAILURE);
    }
}


/**
 * @brief Take semaphore function
 *
 * @param sem_num Semaphore Number
 */
void sem_take(int sem_num)
{
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    if(semop(semaphore_set, &buf, 1) == ERROR)
    {
        perror("sem_wait error");
        exit(EXIT_FAILURE);
    }
}


/**
 * @brief Give Semaphore function
 *
 * @param sem_num Semaphore number
 */
void sem_give(int sem_num)
{
    struct sembuf buf;
    buf.sem_num = sem_num;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    if(semop(semaphore_set, &buf, 1) == ERROR)
    {
        perror("sem_give error");
        exit(EXIT_FAILURE);
    }
}


/**
 * @brief SIGCHLD Signal handler
 *
 * @param signal Signal number
 */
void SIGCHLD_handler(int signal)
{
    TRACE_LOW("Entro en SIGCHLD %d\n", signal);
    while(waitpid(-1, NULL, WNOHANG | WUNTRACED | WCONTINUED) > 0)
    {
        g_number_of_childs--;
    }
}

/**
 * @brief SIGUP Signal handler
 *
 * @param signal Signal number
 */
void SIGUP_handler(int signal)
{
    TRACE_LOW("SIGUP %d\n", signal);
    while(waitpid(-1, NULL, WNOHANG | WUNTRACED | WCONTINUED) > 0)
    {
        g_number_of_childs--;
    }

    TRACE_HIG("g_number_of_childs: %d\n", g_number_of_childs);
}


/**
 * @brief SIGUSR1 Signal Handler
 *
 * @param signal Signal number
 */
void SIGUSR1_handler(int signal)
{
    TRACE_LOW("SIGUSR1 %d\n", signal);
    flag_sigusr1 = true;
}


/**
 * @brief SIGINT signal handler
 *
 * @param signal Signal number
 */
void SIGINT_handler(int signal)
{
    TRACE_LOW("SIGINT %d\n", signal);
    kill(cpid_config_file, SIGKILL);
    kill(cpid_driver_handler, SIGKILL);
    exit(EXIT_FAILURE);
}


/**
 * @brief Update the process context from the config file
 *
 * @param ctx Process context
 */
void update_ctx_from_file(ctx_t *ctx)
{
    uint32_t rv, nread;

    rv = poll(fdinfo, 1, 10);

    if(rv == ERROR)
    {
        perror("poll error");
        exit(EXIT_FAILURE);
    }
    else if(fdinfo[0].revents & (POLLIN) && rv > 0)
    {
        fdinfo[0].revents = 0;
        nread = read(ctx->pipefd[READ], ctx->config_file, 2 * sizeof(ctx->config_file));

        if(nread == ERROR)
        {
            perror("pipe error");
            exit(EXIT_FAILURE);
        }
        else
        {
            ctx->backlog = ctx->config_file->backlog;
            ctx->max_conn = ctx->config_file->max_conn;
            ctx->read_interval = ctx->config_file->read_interval;
            ctx->samples = ctx->config_file->samples;
        }
    }
}


/**
 * @brief Get the current temp object from shared memory segment
 *
 * @param ctx Process context
 * @return float Current temperature
 */
float get_current_temp(ctx_t *ctx)
{
    float current_temp;

    /* Access critical data */
    sem_take(SEM_FULL);
    sem_take(SEM_MUTEX);
    current_temp = ctx->shared_data_1->current_temp;
    sem_give(SEM_MUTEX);

    TRACE_LOW("Temperature readed: %f\n", current_temp);

    return current_temp;
}


/* Shared mem pointer */
void *shared_mem_1 = ( void * )0;
void *shared_mem_2 = ( void * )0;


/**
 * @brief Configure shared memory segments
 *
 * @return ssize_t return value
 */
ssize_t configure_shared_mem(void)
{
    key_t key, key2;
    int shm_id, shm_id_2;

    /* Obtain shared memory key */
    key = ftok("/dev/zero", 'Y');
    key2 = ftok("/dev/zero", 'Z');

    if(key == ( key_t )ERROR || key2 == ( key_t )ERROR)
    {
        perror("ftok error");
        return ERROR;
    }

    /* Allocate shared memory segment */
    shm_id = shmget(key, sizeof(shared_mem_t), 0666 | IPC_CREAT);
    shm_id_2 = shmget(key2, sizeof(shared_mem_t), 0666 | IPC_CREAT);

    if(shm_id == ERROR || shm_id_2 == ERROR)
    {
        perror("shmget error");
        return ERROR;
    }

    /* Assign shared memory */
    shared_mem_1 = shmat(shm_id, ( void * )0, 0);
    shared_mem_2 = shmat(shm_id, ( void * )0, 0);

    if(shared_mem_1 == ( char * )(-1) || shared_mem_2 == ( char * )(-1))
    {
        perror("shmat error");
        return ERROR;
    }

    return EXIT_SUCCESS;
}


/**
 * @brief Configure sig action handlers
 *
 * @return ssize_t return value
 */
ssize_t configure_signals(void)
{
    struct sigaction ctrl_chld_viejo = { 0 };
    struct sigaction ctrl_chld_nuevo = { 0 };
    struct sigaction ctrl_usr1_viejo = { 0 };
    struct sigaction ctrl_usr1_nuevo = { 0 };

    sigset_t sig_block_mask;

    memset(&ctrl_chld_viejo, 0, sizeof(ctrl_chld_viejo));
    memset(&ctrl_chld_nuevo, 0, sizeof(ctrl_chld_nuevo));
    memset(&ctrl_usr1_viejo, 0, sizeof(ctrl_usr1_viejo));
    memset(&ctrl_usr1_nuevo, 0, sizeof(ctrl_usr1_nuevo));

    if(sigemptyset(&sig_block_mask) == ERROR)
    {
        perror("sigemptyset error");
        return ERROR;
    }

    if(sigaddset(&sig_block_mask, SIGCHLD) == ERROR)
    {
        perror("sigaddset SIGCHLD error");
        return ERROR;
    }

    if(sigaddset(&sig_block_mask, SIGUSR1) == ERROR)
    {
        perror("sigaddset SIGUSR1 error");
        return ERROR;
    }

    ctrl_chld_nuevo.sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    ctrl_chld_nuevo.sa_handler = SIGCHLD_handler;

    ctrl_usr1_nuevo.sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    ctrl_usr1_nuevo.sa_handler = SIGUSR1_handler;

    if(sigaction(SIGCHLD, &ctrl_chld_nuevo, &ctrl_chld_viejo) == ERROR)
    {
        perror("sigaction SIGCHLD error");
        return ERROR;
    }

    if(sigaction(SIGUSR1, &ctrl_usr1_nuevo, &ctrl_usr1_viejo) == ERROR)
    {
        perror("sigaction SIGCHLD error");
        return ERROR;
    }

    signal(SIGINT, SIGINT_handler); /* Controlled exit */

    return EXIT_SUCCESS;
}


/**
 * @brief Read and parse the config file
 *
 * @param config_file param structure
 * @param file file name
 * @return ssize_t return value
 */
ssize_t read_config_file(config_file_t *config_file, const char *file)
{
    /* Expected file format
    [backlog] = 2
    [max_conn] = 1000
    [read_interval] = 1
    [samples] = 5
    */
    const char *file_name = "./sup/file.cfg";
    FILE *fd_cfg;
    char buffer[20];
    char *token = malloc(sizeof(buffer));

    if(file != NULL)
    {
        TRACE_LOW("File received from stdin %s\n", file);
        fd_cfg = stdin;
        file_name = file;
    }

    fd_cfg = fopen(file_name, "r");

    if(fd_cfg == NULL)
    {
        perror("Cant open config file");
        exit(EXIT_FAILURE);
    }

    while(fgets(buffer, sizeof(buffer), fd_cfg))
    {
        token = strtok(buffer, "=");
        while(token != NULL)
        {
            if(strstr(token, "samples"))
            {
                token = strtok(NULL, "=");
                config_file->samples = ( int )strtol(token + 1, ( char ** )NULL, 10);
            }
            if(strstr(token, "backlog"))
            {
                token = strtok(NULL, "=");
                config_file->backlog = ( int )strtol(token + 1, ( char ** )NULL, 10);
            }
            if(strstr(token, "max_conn"))
            {
                token = strtok(NULL, "=");
                config_file->max_conn = ( int )strtol(token + 1, ( char ** )NULL, 10);
            }
            if(strstr(token, "read_interval"))
            {
                token = strtok(NULL, "=");
                config_file->read_interval = ( int )strtol(token + 1, ( char ** )NULL, 10);
            }
            token = strtok(NULL, "=");
        }
    }

    TRACE_LOW("Config file %s updated.\n", file_name);
    TRACE_LOW("Backlog: %d\n", config_file->backlog);
    TRACE_LOW("Max_conn: %d\n", config_file->max_conn);
    TRACE_LOW("Samples: %d\n", config_file->samples);
    TRACE_LOW("Read interval: %f\n", config_file->read_interval);

    fclose(fd_cfg);

    return EXIT_SUCCESS;
}


static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
                                 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                                 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                                 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                                 '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/' };
static char *decoding_table = NULL;
static int mod_table[] = { 0, 2, 1 };

void build_decoding_table()
{
    decoding_table = malloc(256);

    for(int i = 0; i < 64; i++)
        decoding_table[( unsigned char )encoding_table[i]] = i;
}


char *base64_encode(const unsigned char *data, size_t input_length, size_t *output_length)
{
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = malloc(*output_length * 2);
    if(encoded_data == NULL)
        return NULL;

    for(int i = 0, j = 0; i < input_length;)
    {
        uint32_t octet_a = i < input_length ? ( unsigned char )data[i++] : 0;
        uint32_t octet_b = i < input_length ? ( unsigned char )data[i++] : 0;
        uint32_t octet_c = i < input_length ? ( unsigned char )data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for(int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}


char *image_base64_encode(const unsigned char *data, size_t input_length)
{
    char *dest;
    size_t output_length = 0;

    dest = base64_encode(data, input_length, &output_length);

    return dest;
}


char *get_base64_plot(const char *file_path)
{
    FILE *fp;
    unsigned char *buffer;
    char *base64data;
    size_t file_length;

    fp = fopen(file_path, "rb");
    if(fp == NULL)
    {
        perror("fopen error");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    file_length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer = malloc(file_length + 1);
    if(!buffer)
    {
        perror("malloc error");
        fclose(fp);
        return NULL;
    }

    fread(buffer, file_length, 1, fp);
    base64data = image_base64_encode(buffer, file_length);

    fclose(fp);
    free(buffer);

    return base64data;
}