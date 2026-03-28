#ifndef INC_TILE_SENSE_T_C_H_
#define INC_TILE_SENSE_T_C_H_

#include "main.h"
#include "stdint.h"

#define IQS323_I2C_ADDR						0x44

#define IQS323_REG_SYSTEM_STATUS			0x10

#define IQS323_REG_CH0_SENSOR_SETUP			0x30	// 0x0101
#define IQS323_REG_CH0_CONV_FREQ_SETUP		0x31	// 0x057F
#define IQS323_REG_CH0_PROX_CONTROL			0x32	// 0x1290
#define IQS323_REG_CH0_PROX_INPUT_CTRL		0x33	// 0x01CF
#define IQS323_REG_CH0_PATTERN_DEFINITIONS	0x34	// 0x030A
#define IQS323_REG_CH0_PATTERN_SEL_EBC		0x35	// 0x0000
#define IQS323_REG_CH0_ATI_SETUP			0x36	// 0x040C
#define IQS323_REG_CH0_ATI_BASE				0x37	// 0x0064
#define IQS323_REG_CH0_ATI_MULT_DIV			0x38	// -
#define IQS323_REG_CH0_COMPENSATION			0x39	// -

#define IQS323_REG_CH0_SETUP				0x60	// 0x0000
#define IQS323_REG_CH0_PROX_SETTINGS		0x61	// 0x0000
#define IQS323_REG_CH0_TOUCH_SETTINGS		0x62	// 0x0000
#define IQS323_REG_CH0_FOLLOWER_WEIGHT		0x63	// 0x0000
#define IQS323_REG_CH0_MOVEMENT_UI_SETTINGS	0x64

#define IQS323_REG_SYSTEM_CONTROL			0xC0	// 0x0000

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_t_c_find(I2C_HandleTypeDef* hi2c);


/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_t_c_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	GET DATA

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	data		the 12-bit right-shifted data

**********************************************************/
uint16_t tile_sense_t_c_get_data(void);

#endif
