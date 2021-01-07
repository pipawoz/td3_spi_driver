/**
 * @file bmp_280.c
 * @author Pedro Wozniak Lorice (pwozniaklorice@est.frba.utn.edu.ar)
 * @brief BMP 280 interface
 * @version 0.1
 * @date 2019-11-28
 *
 * @copyright Copyright (c) 2019
 *
 */

#include "../../inc/bmp_280.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * @brief Read bytes from dev
 *
 * @param fd File descriptor
 * @param data Data to read
 * @param length Length of data
 * @return size_t Return Value
 */
static size_t _bmp280_read_bytes(int fd, unsigned char *data, char length)
{
    char *buf = malloc(length + 1);
    char bytes_read;

    buf[0] = data[0] | MASK_READ;

    for(size_t i = 0; i < length; i++)
    {
        buf[i] = buf[i - 1] + 1;
        buf[i] |= MASK_READ;
    }

    bytes_read = read(fd, buf, length + 1);
    memcpy(data, buf + 1, length);

    return bytes_read;
}


/**
 * @brief Write bytes to dev
 *
 * @param fd File descriptor
 * @param data Data to write
 * @param length Length of data
 * @return size_t Return Value
 */
static size_t _bmp280_write_bytes(const char *dev_path, unsigned char *data, char length)
{
    int fd = 0;
    char *buf = malloc(length + 1);
    char bytes_written = 0;
    
    if(fd < 0)
    {
        printf("open error %s\n", dev_path);
        return EXIT_FAILURE;
    }

    buf[0] = data[0] & MASK_WRITE;

    for(size_t i = 0; i < length; i++)
    {
        buf[i] = buf[i - 1] + 1;
        buf[i] &= MASK_WRITE;
    }

    bytes_written = write(fd, buf, length + 1);
    memcpy(data, buf + 1, length);

    close(fd);

    return bytes_written;
}


/**
 * @brief Get raw temperature from device
 *
 * @param dev_path Device path in FS
 * @return float Raw Temp
 */
static float _read_bmp280_temp(const char *dev_path)
{
    float raw_temp = 0;
    int fd = 0;
    unsigned char data[3];
    double value;

    time_t t;
    srand(( unsigned )time(&t));  // test

    
    fd = open(dev_path, O_RDWR);

    if(fd < 0)
    {
        printf("open error %s\n", dev_path);
        return EXIT_FAILURE;
    }

    data[0] = BMP280_REG_RESULT_TEMPRERATURE;

    if(_bmp280_read_bytes(fd, data, 3))  // MSB 0xFA, LSB 0xFB, XLSB 0xFC
    {
        value = ( double )(int16_t)((( unsigned int )data[1] << 8) | ( unsigned int )data[0]);
    }
    else
    {
        value = 0;
    }
    close(fd);
    
    return raw_temp;
}


/**
 * @brief Compensate raw temperature of bmp280 with calibration data from manufacturer
 *
 * @param raw_temp Uncompensated temperature
 * @return float Compensated temperature
 */
static float _compensate_temperature(uint32_t raw_temp)
{
    bmp280_calib_t calib_data = { .dig_T1 = 27641, .dig_T2 = 25684, .dig_T3 = 50 };
    int32_t var1, var2;
    int32_t t_fine;
    float T;

    int32_t adc_T = raw_temp;
    adc_T >>= 4;

    var1 =
      ((((adc_T >> 3) - (( int32_t )calib_data.dig_T1 << 1))) * (( int32_t )calib_data.dig_T2)) >>
      11;

    var2 = (((((adc_T >> 4) - (( int32_t )calib_data.dig_T1)) *
              ((adc_T >> 4) - (( int32_t )calib_data.dig_T1))) >>
             12) *
            (( int32_t )calib_data.dig_T3)) >>
           14;

    t_fine = var1 + var2;

    T = (t_fine * 5 + 128) >> 8;

    return T / 100;
}


/**
 * @brief Get the bmp280 temp in user friendly mode
 *
 * @return float Temperature
 */
float get_bmp280_temp()
{
    const char *dev_path = "/dev/spi_td3";

    float raw_temp;
    float comp_temp;

    raw_temp = _read_bmp280_temp(dev_path);
    if(raw_temp < 0)
    {
        raw_temp = 0;
    }
    comp_temp = _compensate_temperature(raw_temp);

    return comp_temp;
}


ssize_t set_bmp280_control_reg(uint32_t value)
{
    const char *dev_path = "/dev/spi_td3";
    uint32_t rv;
    unsigned char data[2];

    data[0] = BMP280_REG_CONTROL;
    data[1] = value;

    rv = _bmp280_write_bytes(dev_path, data, sizeof(data));

    return rv;
}