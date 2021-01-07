#ifndef BMP_280_H
#define BMP_280_H

#include "common_inc.h"

float get_bmp280_temp();
ssize_t set_bmp280_control_reg(uint32_t value);

typedef struct
{
    uint32_t dig_T1;
    uint32_t dig_T2;
    uint32_t dig_T3;
} bmp280_calib_t;


#define MASK_READ 0x80
#define MASK_WRITE 0x7F

#define BMP280_REG_RESULT_TEMPRERATURE 0xFA  // 0xFA(msb) , 0xFB(lsb) , 0xFC(xlsb)
#define BMP280_REG_CONTROL 0xF4

#endif