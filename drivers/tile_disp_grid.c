#include "tile_disp_grid.h"

static I2C_HandleTypeDef* disp_grid_handle;

uint8_t tile_disp_grid_check(I2C_HandleTypeDef* hi2c)
{
	HAL_Delay(10);
	if(HAL_I2C_IsDeviceReady(hi2c, AS1130_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

void tile_disp_grid_init(I2C_HandleTypeDef* hi2c)
{
	uint8_t TX_Buffer[1] = {0};
	disp_grid_handle = hi2c;

	uint8_t led = 0, i = 0;

	// SELECT: CONTROL
	TX_Buffer[0] = 0xC0;
	HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_SELECTION, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

		// 6 different "Blink & PWM" datasets
		//
		// Define RAM Configuration; bit mem_conf in the AS1130 (Config Register (see Figure 45), On/Off Frames, Blink & PWM Sets, Dot Correction, if specified

		// CONFIG
		// -----------------------------------------------------------
		//	BITS					DEF		SET		NOTES
		// -----------------------------------------------------------
		// 	7		low_vdd_rst		0		0
		//	6		low_vdd_stat	0		0
		//	5		led_err_corr	0		0
		//	4		dot_corr		0		0
		//	3		common_addr		0		0		0=disabled; 1=enabled for 0x0111111
		//	2:0		mem_conf		000		001		000=default/invalid; 001 = RAM config 1 (Blink & PWM = 0; On/Off = 35..0)
		TX_Buffer[0] = 0b00000001;
		HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_CONFIG, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);


	// SELECT: FRAME 0 ON/OFF CONTROL
	TX_Buffer[0] = 0x01;
	HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_SELECTION, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

		TX_Buffer[0] = 0b11111111;
		for(i = 0x00; i<=0x16; i=i+2){
			HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, i, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
		}
		TX_Buffer[0] = 0b00000111;
		for(i = 0x01; i<=0x17; i=i+2){
			HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, i, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
		}

	// SELECT: BLINK & PWM SET 0
	TX_Buffer[0] = 0x40;
	HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_SELECTION, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

		// NO BLINK
		TX_Buffer[0] = 0x00;
		for(i=0x00; i<=0x17; i++){
			HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, i, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
		}

		// INTENSITY
		TX_Buffer[0] = 0x09;
		for(led=0x18; led<=0x9B; led++){
			HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, led, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
		}

	// SELECT: CONTROL
	TX_Buffer[0] = 0xC0;
	HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_SELECTION, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

		// CURRENT_SOURCE
		// -----------------------------------------------------------
		//	BITS					DEF		SET		NOTES
		// -----------------------------------------------------------
		//	7:0						0x00			0x00=0mA, 0xFF=30mA (30/255mA per step, 8.5µA
		TX_Buffer[0] = 17;	// 2mA * 255/30
		HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_CURRENT_SOURCE, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

		// DISPLAY_OPTION
		// -----------------------------------------------------------
		//	BITS					DEF		SET		NOTES
		// -----------------------------------------------------------
		//	7:5		loops			001		001		# of loops to play in one movie, 0 is not valid
		//	4		blink_freq		0		0		0=1.5s, 1=3s
		//	3:0		scan_limit		0000	0000	number of displayed segments in one frame (0x000 = CS0, 0x0001 = CS0-CS1,... 0x1011 = CS0-CS11)
		TX_Buffer[0] = 0b00101011;
		HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_DISPLAY_OPTION, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);


		// PICTURE
		// -----------------------------------------------------------
		//	BITS					DEF		SET		NOTES
		// -----------------------------------------------------------
		//	7		blink_pic		0		0		0 = no, 1 = all blink
		//	6		display_pic		0		1		0 = no, 1 = disply picture
		//	5:0		pic_addr		000000	000000	frame address
		TX_Buffer[0] = 0b01000000;
		HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_PICTURE, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

		// EXIT SHUTDOWN
		// -----------------------------------------------------------
		//	BITS				DEF		SET
		// -----------------------------------------------------------
		// 	1		init		1		1		0=initialize, 1=normal
		//	0		shutdown	0		1		0=shutdown, 1=normal
		TX_Buffer[0] = 0b00000011;
		HAL_I2C_Mem_Write(disp_grid_handle, AS1130_I2C_ADDR<<1, AS1130_REG_SHUTDOWN, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

}
