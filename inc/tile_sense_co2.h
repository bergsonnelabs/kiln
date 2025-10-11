#ifndef INC_TILE_SENSE_CO2_H_
#define INC_TILE_SENSE_CO2_H_

#include "main.h"
#include "stdint.h"

#define STC31_I2C_ADDR					0x29

#define STC31_CMD_ID1A					0x36
#define STC31_CMD_ID1B					0x7C
#define STC31_CMD_ID2A					0xE1
#define STC31_CMD_ID2B					0x02

//#define TMD3725_REG_ID_DEFAULT			0xE4


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_co2_find(I2C_HandleTypeDef* hi2c);

#endif
