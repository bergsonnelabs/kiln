#ifndef INC_TILE_POWER_L_1_H_
#define INC_TILE_POWER_L_1_H_

#include "main.h"
#include "stdint.h"

#define BQ25150_I2C_ADDR				0x6B

#define BQ25150_REG_STAT0       		0x00
#define BQ25150_REG_STAT1       		0x01
#define BQ25150_REG_STAT2       		0x02
#define BQ25150_REG_FLAG0       		0x03
#define BQ25150_REG_FLAG1       		0x04
#define BQ25150_REG_FLAG2       		0x05
#define BQ25150_REG_FLAG3       		0x06
#define BQ25150_REG_ICHG_CTRL   		0x13
#define BQ25150_REG_PCHRGCTRL   		0x14
#define BQ25150_REG_BUVLO       		0x16
#define BQ25150_REG_ICCTRL0     		0X35
#define BQ25150_REG_ADCCTRL0    		0x40
#define BQ25150_REG_VBAT_MSB    		0x42
#define BQ25150_REG_VBAT_LSB    		0x43
#define BQ25150_REG_ADC_READ_EN 		0x58
#define BQ25150_REG_DEVICE_ID   		0x6F

#define BQ25150_REG_DEVICE_ID_DEFAULT	0x20

uint8_t tile_power_l_1_init(I2C_HandleTypeDef* hi2c);
uint16_t tile_power_l_1_get_vbat(void);
uint8_t tile_power_l_1_get_percent(void);

#endif /* INC_TILE_POWER_L_1_H_ */
