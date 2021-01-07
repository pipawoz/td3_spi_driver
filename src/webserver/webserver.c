/**
 * @file webserver.c
 * @author Pedro Wozniak Lorice (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief Core code for the webserver
 * @version 0.8
 * @date 2019-09-03
 *
 * @copyright Copyright (c) 2019
 *
 */


#include "../../inc/webserver.h"


bool flag_sigusr1 = false;


/**
 * @brief Web server initialization
 *
 * @param ctx Process context
 * @return ssize_t Return value
 */
ssize_t web_server_init(ctx_t *ctx)
{
    const char *server_ip_addr = "127.0.0.1";
    const uint32_t server_port = 80;
    int yes = 1;

    if((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) == ERROR)
    {
        perror("socket error");
        return EXIT_FAILURE;
    }

    memset(( char * )&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // server_addr.sin_addr.s_addr = inet_addr(server_ip_addr);
    server_addr.sin_port = htons(server_port);


    if((setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int))) == ERROR)
    {
        perror("setsockopt error");
        return EXIT_FAILURE;
    }

    if((setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == ERROR)
    {
        perror("setsockopt error");
        return EXIT_FAILURE;
    }

    if(bind(sock_fd, ( struct sockaddr * )&server_addr, sizeof(server_addr)) == ERROR)
    {
        perror("bind error");
        return EXIT_FAILURE;
    }

    socket_length = sizeof(server_addr);

    if(getsockname(sock_fd, ( struct sockaddr * )&server_addr, &socket_length) == ERROR)
    {
        perror("getsockname error");
        return EXIT_FAILURE;
    }

    if(listen(sock_fd, ctx->backlog) == ERROR)
    {
        perror("listen error");
        return EXIT_FAILURE;
    }

    TRACE_HIG("Server started with addr: %s:%d\n",
              inet_ntoa(server_addr.sin_addr),
              ntohs(server_addr.sin_port));

    return EXIT_SUCCESS;
}


/**
 * @brief Generate an ok response data with the given parameters
 *
 * @param sock_client Client socket number
 * @param student_name Student name for the template
 * @param legajo Legajo for the template
 * @param last_temp Struct with the last temperature and timestamp
 */
void send_response_data(uint32_t sock_client,
                        const char *student_name,
                        const char *legajo,
                        last_temp_t *last_temp)
{
    char write_buf[8192] = { 0 }, write_buf2[8192] = { 0 };
    char out_buf[8192] = { 0 }, out_buf2[8192] = { 0 };
    char dev_temp[10];
    char *image_data;
    struct stat filestat;
    char filesize[7];
    int fd;
    char file_buf[1024] = { 0 };
    char result;
    FILE *fp;

    if(((fd = open("./sup/data.png", O_RDONLY)) < -1) || (fstat(fd, &filestat) < 0))
    {
        printf("Error in measuring the size of the file");
    }

    sprintf(filesize, "%zd", filestat.st_size);

    fp = fopen("./sup/data.png", "r");
    if(fp == NULL)
    {
        perror("fopen error");
        exit(EXIT_FAILURE);
    }

    fread(file_buf, sizeof(char), filestat.st_size + 1, fp);
    fclose(fp);

    image_data = get_base64_plot("./sup/data.png");

    sprintf(dev_temp, "%.2f°C", last_temp->temp);
    sprintf(write_buf,
            response_page_template,
            student_name,
            legajo,
            last_temp->timestamp,
            dev_temp,
            image_data);

    sprintf(out_buf, response_http_template, strlen(write_buf), write_buf);
    write(sock_client, out_buf, strlen(out_buf));
}


/**
 * @brief Generates an invalid response
 *
 * @param sock_client Client socket
 */
void send_invalid_response(uint32_t sock_client)
{
    char write_buf[128] = { 0 };
    char out_buf[4096] = { 0 };
    sprintf(write_buf, "POST method is not supported\n");
    sprintf(out_buf, invalid_response_template, strlen(write_buf), write_buf);
    write(sock_client, out_buf, strlen(out_buf));
}


/**
 * @brief Thread that handles the client conection
 *
 * @param arg Thread arguments
 * @return void* Return value
 */
void *client_thread(void *arg)
{
    const char *student_name = "Pedro Wozniak Lorice";
    const char *legajo = "140-728.4";

    uint32_t read_msg_length = 0;
    uint32_t thread_success = 0, thread_error = 1;
    last_temp_t *last_temp = malloc(sizeof(last_temp_t));

    args_t *args = arg;

    uint32_t sock_client = args->sock_fd;
    TRACE_MID("New client with socket_id: %d\n", sock_client);

    char read_buf[4096] = { 0 };
    char *method;

    get_last_temp(last_temp);

    read_msg_length = read(sock_client, read_buf, sizeof(read_buf));

    char *tmp_msg = strdup(read_buf);
    method = strtok(tmp_msg, " /");

    if(read_msg_length <= 0)
    {
        perror("rcv");
        if(errno == EPIPE) /* Connection broken */
        {
            printf("Client error\n");
            goto thread_error;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK) /* Timeout in reception */
        {
            printf("Timeout in client connection\n");
            goto thread_error;
        }

        printf("Client ended connection\n");
        goto thread_error;
    }

    if(!strcasecmp(method, "GET"))
    {
        generate_temperature_plot();
        send_response_data(sock_client, student_name, legajo, last_temp);
    }
    else if(!strcasecmp(method, "POST"))
    {
        send_invalid_response(sock_client);
        goto thread_error;
    }

    close(sock_client);
    g_number_of_threads--;
    free(last_temp);
    pthread_exit(&thread_success);

thread_error:
    g_number_of_threads--;
    close(sock_client);
    free(last_temp);
    pthread_exit(&thread_error);
}


/**
 * @brief Main function
 *
 * @param argc Argument count
 * @param argv Config file
 * @return int Return value
 */
int main(int argc, char *argv[])
{
    ctx_t ctx_a;
    ctx_t *ctx = &ctx_a;
    uint32_t sock_client;

    ctx->config_file = malloc(sizeof(ctx->config_file));

    /* Set default values */
    ctx->backlog = 2;
    ctx->max_conn = 1000;
    ctx->read_interval = 1;
    ctx->samples = 5;

    if(read_config_file(ctx->config_file, (argc == 2 ? argv[1] : NULL)) == ERROR)
    {
        perror("read config file error"); /* Dont exit and use default values */
    }

    /* Update context values from config file */
    ctx->backlog = ctx->config_file->backlog;
    ctx->max_conn = ctx->config_file->max_conn;
    ctx->read_interval = ctx->config_file->read_interval;
    ctx->samples = ctx->config_file->samples;

    if(pipe(ctx->pipefd) == ERROR)
    {
        perror("ctx pipe error");
        close(ctx->pipefd[WRITE]);
        close(ctx->pipefd[READ]);
        exit(EXIT_FAILURE);
    }

    if(configure_signals() == ERROR)
    {
        perror("signal configure error");
        exit(EXIT_FAILURE);
    }

    if(configure_shared_mem() == ERROR)
    {
        perror("shared mem configure error");
        exit(EXIT_FAILURE);
    }

    /* Create a semaphore set with NUMSEMS semaphores */
    semaphore_set = semget(IPC_PRIVATE, NUMSEMS, IPC_CREAT | 0666);

    if(semaphore_set == ERROR)
    {
        perror("semget error");
        exit(EXIT_FAILURE);
    }

    /* Initialize semaphores */
    sem_initialise(SEM_MUTEX, 1);
    sem_initialise(SEM_FULL, 0);
    sem_initialise(SEM_EMPTY, 10);

    /* Assign shared memory pointer */
    ctx->shared_data_1 = ( shared_mem_t * )shared_mem_1;
    ctx->shared_data_2 = ( shared_mem_t * )shared_mem_2;

    /* Create pipe */
    fdinfo[0].fd = ctx->pipefd[READ];
    fdinfo[0].events = POLLIN;
    fdinfo[0].revents = 0;

    TRACE_MID("Server started with PID: %d\n", getpid());

    /* GNU Plot Handler Process */
    cpid_plot_handler = fork();

    if(cpid_plot_handler == ERROR)
    {
        perror("cpid_plot_handler fork error");
        exit(EXIT_FAILURE);
    }

    g_number_of_childs++;

    if(cpid_plot_handler == 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGHUP); /* Process dies if parent dies */
        close(ctx->pipefd[WRITE]);
        strcpy(argv[0], "webserver child_plot_handler"); /* Rename child process */
        child_plot_handler(ctx);
    }

    /* Start Config File Process */
    cpid_config_file = fork();

    if(cpid_config_file == ERROR)
    {
        perror("cpid_config_file fork error");
        exit(EXIT_FAILURE);
    }

    g_number_of_childs++;

    if(cpid_config_file == 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGHUP);                /* Process dies if parent dies */
        strcpy(argv[0], "webserver child_config_file"); /* Rename child process */
        close(ctx->pipefd[READ]);
        child_config_file(ctx);
    }

    /* Start Driver Handler */
    cpid_driver_handler = fork();

    if(cpid_driver_handler == ERROR)
    {
        perror("cpid_driver_handler fork error");
        exit(EXIT_FAILURE);
    }

    g_number_of_childs++;

    if(cpid_driver_handler == 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGHUP);                   /* Process dies if parent dies */
        strcpy(argv[0], "webserver child_driver_handler"); /* Eename child process */
        close(ctx->pipefd[READ]);
        child_driver_handler(ctx);
    }

    strcpy(argv[0], "webserver parent"); /* Rename parent process */

    /* Start the webserver */
    if(web_server_init(ctx) == ERROR)
    {
        perror("web_server_init error");
        exit(EXIT_FAILURE);
    }

    struct timeval tv = { 5, 0 };

    uint32_t old_max_conn = ctx->max_conn;
    pthread_t *thread_id; /* Allocate max_conn threads */
    pthread_attr_t attr;

    thread_id = malloc(ctx->max_conn * sizeof(pthread_t));

    while(1)
    {
        uint32_t nbr_fds;
        args_t args_client;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_fd, &readfds);
        nbr_fds = select(sock_fd + 1, &readfds, NULL, NULL, &tv);

        if((nbr_fds < 0) && (errno != EINTR))
        {
            close(sock_client), perror("select error");
            exit(EXIT_FAILURE);
        }
        else if(!FD_ISSET(sock_fd, &readfds))
        {
            continue;
        }

        if((sock_client = accept(sock_fd, ( struct sockaddr * )&server_addr, &socket_length)) ==
           ERROR)
        {
            close(sock_client);
            perror("accept error");
            exit(EXIT_FAILURE);
        }

        g_number_of_threads++;

        if(g_number_of_threads > ctx->config_file->max_conn)
        {
            printf("Cannot accept more clients\n");
            close(sock_client); /* TODO: Enviar mensaje de error al cliente ej: 404? */
            g_number_of_threads--;
            continue;
        }

        args_client.ctx_client = ctx;
        args_client.sock_fd = sock_client;

        /* Create dynamic threads */
        if(ctx->max_conn > old_max_conn ||
           (ctx->max_conn < old_max_conn && ctx->max_conn > g_number_of_threads))
        {
            thread_id = realloc(thread_id, ctx->max_conn * sizeof(pthread_t));
        }

        pthread_attr_init(&attr); /* Allocate attrib */

        if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
        {
            close(sock_client);
            perror("pthread_attr_setdetachstate error");
            exit(EXIT_FAILURE);
        }

        /* Create detached thread */
        if((*thread_id = pthread_create(
              &thread_id[g_number_of_threads], &attr, &client_thread, ( void * )&args_client)) ==
           ERROR)
        {
            close(sock_client);
            perror("pthread_create error");
            exit(EXIT_FAILURE);
        }

        pthread_attr_destroy(&attr); /* Free attrib */
    }

    if(shmdt(shared_mem_1) == ERROR || shmdt(shared_mem_2) == ERROR)
    {
        perror("shmdt error");
        exit(EXIT_FAILURE);
    }

    close(ctx->pipefd[READ]);
    close(ctx->pipefd[WRITE]);
    free(ctx->config_file);

    exit(EXIT_SUCCESS);
}

/**
 * @brief Process that handles the config file
 *
 * @param ctx Webserver context
 */
void child_config_file(ctx_t *ctx)
{
    TRACE_MID("Child Config File started with PID: %d\n", getpid());

    config_file_t *config_file = malloc(sizeof(config_file));

    while(1)
    {
        if(flag_sigusr1 == true)
        {
            if(read_config_file(config_file, NULL) == ERROR)
            {
                perror("read config file error");
                exit(EXIT_FAILURE);
            }
            write(ctx->pipefd[WRITE], config_file, 2 * sizeof(config_file));
            flag_sigusr1 = false;

            printf("Se actualizo pipefd\n");
        }
        sleep(1);
    }

    close(ctx->pipefd[WRITE]);
    free(config_file);
    exit(EXIT_SUCCESS);
}
