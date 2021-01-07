/**
 * @file driver_handler.c
 * @author Pedro Wozniak Lorice (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief This file contains the handler for the spi driver to obtain temperature readings.
 * @version 0.1
 * @date 2019-11-14
 *
 * @copyright Copyright (c) 2019
 *
 */

#include "../../inc/driver_handler.h"
#include "../../inc/bmp_280.h"
#include <sys/time.h>


/**
 * @brief msleep function
 *
 * @param ms time to sleep
 */
void msleep(unsigned int ms)
{
    int microsecs;
    struct timeval tv;

    microsecs = ms * 1000;
    tv.tv_sec = microsecs / 1000000;
    tv.tv_usec = microsecs % 1000000;

    select(0, NULL, NULL, NULL, &tv);
}


/**
 * @brief Procces that handles the spi hardware
 *
 * @param ctx Process context
 */
void child_driver_handler(ctx_t *ctx)
{
    TRACE_MID("Child Driver Handler started with PID: %d\n", getpid());

    float current_temp = 0;
    uint32_t control_reg_value = 0x02;

    set_bmp280_control_reg(control_reg_value); /* Write in control register */

    while(1)
    {
        current_temp = get_bmp280_temp();

        sem_take(SEM_MUTEX);
        ctx->shared_data_1->current_temp = current_temp; /* Update the value in shared memory */
        sem_give(SEM_MUTEX);
        sem_give(SEM_FULL);

        msleep(1000);
    }


    exit(EXIT_SUCCESS);
}