#ifndef PLOT_HANDLER_H
#define PLOT_HANDLER_H
#include "common_inc.h"
#include "functions.h"

void child_plot_handler(ctx_t* ctx);
void get_last_temp(last_temp_t* last_temp);
void generate_temperature_plot();

#endif
