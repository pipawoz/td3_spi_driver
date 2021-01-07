/**
 * @file functions.h
 * @author pedro wozniak (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief header file for functions.c
 * @version 0.1
 * @date 2019-09-04
 *
 * @copyright Copyright (c) 2019
 *
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "webserver.h"
#include "common_inc.h"

extern bool flag_sigusr1;

void update_ctx_from_file(ctx_t *ctx);
float get_current_temp(ctx_t *ctx);
ssize_t read_config_file(config_file_t *config_file, const char *file);
char *get_base64_plot(const char *file_path);

#endif