#ifndef INC_TILE_DRIVE_DA_IB_H_
#define INC_TILE_DRIVE_DA_IB_H_

#include "main.h"
#include "stdint.h"

#define DAC530A2W_I2C_ADDR					0x49

#define DAC530A2W_REG_GEN_STATUS			0x22

#define DAC530A2W_REG_GEN_STATUS_L_DEFAULT	0x18
#define DAC530A2W_REG_COMMON_CONFIG			0x1F
#define DAC530A2W_REG_DAC_2_GAIN_CONFIG		0x03
#define DAC530A2W_REG_DAC_2_DATA			0x19


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_da_ib_check(I2C_HandleTypeDef* hi2c);

uint8_t tile_drive_da_ib_config(I2C_HandleTypeDef* hi2c);

void tile_drive_da_ib_output(int16_t value);

#endif
