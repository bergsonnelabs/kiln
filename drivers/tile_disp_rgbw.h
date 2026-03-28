#ifndef INC_TILE_DISP_RGBW_H_
#define INC_TILE_DISP_RGBW_H_

#include "main.h"
#include "stdint.h"

#define LP5811_I2C_ADDR				0x50

#define LP5811_REG_CHIPEN           0x00
#define LP5811_REG_CONFIG_0         0x01
#define LP5811_REG_CONFIG_2         0x03
#define LP5811_REG_CONFIG_12        0x0D
#define LP5811_REG_CMD_UPDATE       0x10
#define LP5811_REG_LED_EN_1         0x20
#define LP5811_RESET                0x23
#define LP5811_MANUAL_DC_0          0x30
#define LP5811_MANUAL_DC_1          0x31
#define LP5811_MANUAL_DC_2          0x32
#define LP5811_MANUAL_DC_3          0x33
#define LP5811_MANUAL_PWM_0         0x40
#define LP5811_MANUAL_PWM_1         0x41
#define LP5811_MANUAL_PWM_2         0x42
#define LP5811_MANUAL_PWM_3         0x43

#define LP5811_REG_CONFIG_2_DEFAULT 0xE4

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port
	vled		10x drive voltage for the LED outputs (30-55 = 3.0-5.5V) - THIS IS NOT YET IMPLEMENTED!

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_disp_rgbw_init(I2C_HandleTypeDef* hi2c, uint8_t vled);

/**********************************************************

	SET RGBW OUTPUTS

	parameters
	-------------------------------------------------------
	r,g,b,w		intensity between 0-100 for each color

	returns
	-------------------------------------------------------
	n/a

**********************************************************/
void tile_disp_rgbw_output(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

#endif
