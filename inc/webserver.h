/**
 * @file webserver.h
 * @author pedro wozniak (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief header file for webserver.c
 * @version 0.1
 * @date 2019-09-04
 *
 * @copyright Copyright (c) 2019
 *
 * TODO: Merge config_file_t into ctx_t structure
 *
 */


#ifndef WEBSERVER_H
#define WEBSERVER_H

#define _GNU_SOURCE

#include <time.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/stat.h>

#include "driver_handler.h"
#include "common_inc.h"
#include "plot_handler.h"

#define ERROR -1

#define DEVICE_PATH "/dev/spi_td3"

/* Webserver defines */
#define PORT_NUMBER 80
#define MAX_DATA_SIZE 100

/* Semaphore and Mutex */
#define NUMSEMS 3


int semaphore_set;

union semun
{
    int val;                 /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array;   /* Array for GETALL, SETALL */
    struct sem_info *__buff; /* Buffer for IPC_INFO (Linux specific) */
};


/** Total number of live childs */
static uint8_t g_number_of_childs = 0;
static uint32_t g_number_of_threads = 0;
static uint32_t cpid_config_file, cpid_driver_handler, cpid_plot_handler;


/* Shared Memory Segments */
extern void *shared_mem_1;
extern void *shared_mem_2;


typedef struct
{
    ctx_t *ctx_client;
    uint32_t sock_fd;
} args_t;


/* Webserver variables */
uint32_t sock_fd;
socklen_t socket_length;
struct sockaddr_in server_addr;


struct thread_data
{
    int duration;
    int num;
} td;


static const char response_http_template[] = {
    "HTTP/1.1 200 OK \
Date: Mon, 27 Jul 2009 12:28:53 GMT \
Server: Apache/2.2.14 (Win32) \
Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT \
Content-Length: %lu \
Content-Type: text/html \
Connection: Closed \
\n\n \
%s"
};

static const char image_http_template[] = {
    "HTTP/1.1 200 OK\r\n \
Content-Type: image/png\r\n \
Content-Length: %lu\r\n \
\r\n \
%s"
};

static const char invalid_response_template[] = {
    "HTTP/1.1 404 Not Found\
Date: Mon, 27 Jul 2009 12:28:53 GMT \
Server: Apache/2.2.14 (Win32) \
Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT \
Content-Length: %lu \
Content-Type: text/html \
Connection: Closed \
\n\n \
%s"
};


static const char response_page_template[] = {
    "<!DOCTYPE html> \
<html> \
<head> \
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /> \
    <meta charset=\"utf-8\" /> \
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> \
    <title>UTN FRBA - Trabajo Práctico Técnicas Digitales 3 - 2do Cuatrimestre</title> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:100\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:200\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:300\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:400\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:500\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:600\" rel=\"stylesheet\"> \
    <link href=\"https://fonts.googleapis.com/css?family=Roboto:700\" rel=\"stylesheet\"> \
    <link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/css/bootstrap.min.css\" \
        integrity=\"sha384-Gn5384xqQ1aoWXA+058RXPxPg6fy4IWvTNh0E263XmFcJlSAwiGgFAW/dAiS6JXm\" crossorigin=\"anonymous\"> \
    <style> \
        .measure-box { \
            border-radius: 10px; \
            padding: 20px 20px 20px 20px; \
            background-color: #cccccc; \
        } \
    </style> \
</head> \
<body> \
    <div class=\"row\"> \
        <div class=\"container\"> \
            <div class=\"jumbotron\"> \
                <h2>UTN FRBA - Técnicas Digitales 3 - Trabajo 2do cuatrimestre</h2> \
                <p> \
                    Alumno: %s \
                    Legajo: %s \
                </p> \
            </div> \
            <label class=\"alert alert-success\">La última medición exitosa fue tomada: %s</label> \
            <div class=\"row\"> \
                <div class=\"col-4\"> \
                </div> \
                <div class=\"col-4 measure-box text-center\"> \
                    <h4>Valor de medición:</h4> \
                    <h5>%s</h5> \
                </div> \
                <div class=\"col-4\"> \
                </div> \
                <div class=\"container \"><div class=\"col-md-12\"></div> \
                <div class=\"col-md-12 px-0 center-block\"> \
                    <img class=\"img-responsive \" src=\"data:image/png;base64,%s\" alt=\"Plot\"/> \
                </div></div> \
            </div> \
        </div> \
        <script src=\"https://code.jquery.com/jquery-3.2.1.slim.min.js\" \
            integrity=\"sha384-KJ3o2DKtIkvYIK3UENzmM7KCkRr/rE9/Qpg6aAZGJwFDMVNA/GpGFF93hXpG5KkN\" \
            crossorigin=\"anonymous\"></script> \
        <script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.12.9/umd/popper.min.js\" \
            integrity=\"sha384-ApNbgh9B+Y1QKtv3Rn7W3mgPxhU9K/ScQsAP7hUibX39j7fakFPskvXusvfa0b4Q\" \
            crossorigin=\"anonymous\"></script> \
        <script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js\" \
            integrity=\"sha384-JZR6Spejh4U02d8jOt6vLEHfe/JQGiRRSQQxSfFWpi1MquVdAyjUar5+76PVCmYl\" \
            crossorigin=\"anonymous\"></script> \
</body> \
</html>"
};


typedef struct
{
    char *method;
    char *file;
    char *extension;
} request_handler_t;


ssize_t configure_signals(void);
ssize_t read_config_file(config_file_t *config_file, const char *file);
ssize_t configure_shared_mem(void);

void child_config_file(ctx_t *ctx);
void child_driver_handler(ctx_t *ctx);

void SIGUSR1_handler(int signal);
void SIGCHLD_handler(int signal);

#endif
