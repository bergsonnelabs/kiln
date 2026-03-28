#ifndef INC_TILE_SENSE_TOF_H_
#define INC_TILE_SENSE_TOF_H_

#include "main.h"
#include "stdint.h"

#define TMF8806_I2C_ADDR				0x41

#define TMF8806_REG_APPID				0x00
#define TMF8806_REG_APPREQID			0x02
#define	TMF8806_REG_CMD_DATA9			0x06
#define TMF8806_REG_STATUS				0x1D
#define TMF8806_REG_DISTANCE_PEAK_LSB	0x22
#define TMF8806_REG_DISTANCE_PEAK_MSB	0x23
#define TMF8806_REG_ENABLE				0xE0
#define TMF8806_REG_INT_STATUS			0xE1
#define TMF8806_REG_ID					0xE3

#define TMF8806_REG_ID_DEFAULT			0x09 // datasheet says to "not rely on bits 6 and 7"

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_tof_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_tof_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	GET PEAK DISTANCE MEASUREMENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	result		16-bit unsigned int

**********************************************************/
uint16_t tile_sense_tof_get_measurement(void);

#endif
