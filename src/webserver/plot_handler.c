/**
 * @file plot_handler.c
 * @author Pedro Wozniak Lorice (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief This file contains functions to handle the temperature plot
 * @version 1.0
 * @date 2019-11-14
 *
 * @copyright Copyright (c) 2019
 *
 */
#include "../../inc/plot_handler.h"

/* Includes to avoid gcc warnings */
extern FILE *popen(const char *command, const char *modes);
extern int pclose(FILE *stream);


/**
 * @brief Generate the temperature plot based on the .dat file and store it in a png image
 *
 */
void generate_temperature_plot()
{
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char *setup_gnuplot[] = { "set style lines 2",
                              "set terminal png size 640,480",
                              "set title 'Sensor Temperature'",
                              "set xlabel 'time [m]'",
                              "set ylabel 'temperature [°C]'",
                              "set output \"./sup/data.png\"",
                              "set yrange[10:45]",
                              "set timefmt '%H:%M:%S'",
                              "set xdata time",
                              "set xtics format '%H:%M'" };

    uint32_t number_of_commands = 10;

    FILE *handler = popen("gnuplot -persistent", "w");

    for(int i = 0; i < number_of_commands; i++)
    {
        fprintf(handler, "%s \n", setup_gnuplot[i]);
    }

    fprintf(handler,
            "set xrange['%02d:%02d:%02d':'%02d:%02d:%02d']\n",
            timeinfo->tm_hour,
            timeinfo->tm_min - 2,
            timeinfo->tm_sec,
            timeinfo->tm_hour,
            timeinfo->tm_min,
            timeinfo->tm_sec + 30);

    fprintf(handler,
            "plot \"./sup/data.dat\" using 1:2 with lines notitle linecolor rgb 'red' lw 2");

    pclose(handler);
}


/**
 * @brief Get the moving average object
 *
 * @param avg Previous moving average
 * @param input New value to compute
 * @param samples Number of samples to use
 * @return double New value of moving average
 */
double _get_moving_average(double avg, double input, uint32_t samples)
{
    avg -= avg / samples;
    avg += input / samples;
    return avg;
}


/**
 * @brief Get the last temp object
 *
 * @param last_temp Pointer to struct to store the last temp and the timestamp
 */
void get_last_temp(last_temp_t *last_temp)
{
    FILE *fp_read;
    char str[30];

    if((fp_read = fopen("./sup/data.dat", "r")) == NULL)
    {
        perror("fopen error");
        exit(EXIT_FAILURE);
    }

    while(fgets(str, sizeof(str), fp_read) != NULL)
    {
    }
    sscanf(str, "%s %f", last_temp->timestamp, &last_temp->temp);
    fclose(fp_read);
}


/**
 * @brief Process to handle the storage of new temperature values
 *
 * @param ctx Process context
 */
void child_plot_handler(ctx_t *ctx)
{
    float current_temp;
    uint32_t rv;
    uint32_t nread = 1;
    float moving_average;
    uint32_t samples = 0;

    FILE *fp_dat;

    time_t rawtime;
    struct tm *timeinfo;

    fclose(fopen("./sup/data.dat", "w"));

    while(1)
    {
        update_ctx_from_file(ctx);

        current_temp = get_current_temp(ctx);

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        /* Start writing data on plot */
        fp_dat = fopen("./sup/data.dat", "a");

        if(fp_dat == NULL)
        {
            perror("fopen error");
            exit(EXIT_FAILURE);
        }

        samples = nread < ctx->samples ? nread : ctx->samples;

        moving_average = _get_moving_average(moving_average, current_temp, samples);

        fprintf(fp_dat,
                "%02d:%02d:%02d %.2f\n",
                timeinfo->tm_hour,
                timeinfo->tm_min,
                timeinfo->tm_sec,
                moving_average);

        fclose(fp_dat);

        nread++;

        sleep(ctx->read_interval);
    }
}
