#include "tile_disp_rgbw.h"

static I2C_HandleTypeDef* disp_rgbw_handle;

uint8_t lp5811_read(uint8_t reg);
void lp5811_write(uint8_t reg, uint8_t value);

uint8_t tile_disp_rgbw_init(I2C_HandleTypeDef* hi2c, uint8_t vled)
{
	disp_rgbw_handle = hi2c;

	lp5811_write(LP5811_REG_CHIPEN,1);
	if(lp5811_read(LP5811_REG_CONFIG_2) != LP5811_REG_CONFIG_2_DEFAULT){
		return 0;
	}

    // 5:1 - set boost output voltage in 0.1V increments (default 0x00 > 3V; 0x1F > 5.5V)
    // 0 - max current (0=25.5mA, 1=51mA)
    //TODO: actually use the vled value
    lp5811_write(LP5811_REG_CONFIG_0,0x1F); // 4.5V & 51mA

    // LOD (open detect) and LSD (short detect)
    // 3        1       1       LOD action (0 = none; 1 = disable current sink)
    // 2        0       0       LSD action (0 = none; 1 = all off)
    // 1:0      00      11      LSD threshold (0.35 + 0.1 * value)
    lp5811_write(LP5811_REG_CONFIG_12,0x0B); // 0000 1011

    // complete command updates
    lp5811_write(LP5811_REG_CMD_UPDATE,0x55);

    // enable LEDs
    // 3..0     0       1       LED3...0 enable (0=disabled; 1=enabled)
    lp5811_write(LP5811_REG_LED_EN_1,0x0F);

    // set individual current limits (percent of max)
    lp5811_write(LP5811_MANUAL_DC_0,0x80);
    lp5811_write(LP5811_MANUAL_DC_1,0x80);
    lp5811_write(LP5811_MANUAL_DC_2,0x80);
    lp5811_write(LP5811_MANUAL_DC_3,0x80);

	return 1;
}

void tile_disp_rgbw_output(uint8_t r, uint8_t g, uint8_t b, uint8_t w){
    lp5811_write(LP5811_MANUAL_PWM_0,r);
    lp5811_write(LP5811_MANUAL_PWM_1,b);
    lp5811_write(LP5811_MANUAL_PWM_2,g);
    lp5811_write(LP5811_MANUAL_PWM_3,w);
}

uint8_t lp5811_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(disp_rgbw_handle, LP5811_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void lp5811_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(disp_rgbw_handle, LP5811_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}



