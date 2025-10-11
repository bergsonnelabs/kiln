#include "tile_drive_dc_i.h"
#include "main.h"


static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_drive_dc_i_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, DRV8214_I2C_ADDR<<1, DRV8214_REG_CONFIG0, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] == DRV8214_REG_CONFIG0_DEFAULT){
		tile_handle = hi2c;
		return 1;
	} else {
		return 0;
	}
}

void tile_drive_dc_i_config(uint8_t mode)
{
	uint8_t TX_Buffer[1] = {0};

	if(mode == MODE_I2C){
		// ----------------------------------------------------------------------
		// REGISTER: CONFIG4
		// ----------------------------------------------------------------------
		// 	B		NAME			DEFAULT		SET
		// ----------------------------------------------------------------------
		// 	7-6		RC_REP			00
		// 	5		STALL_REP		1
		// 	4		CBC_REP			1
		// 	3		PMODE			1			0 (PH/EN mode)
		//	2		I2C_BC			0			1 (enable I2C control)
		//	1		I2C_EN_IN1		0
		//	0		I2C_PH_IN2		0
		TX_Buffer[0] = 0b00110100;
		HAL_I2C_Mem_Write(tile_handle, DRV8214_I2C_ADDR<<1, DRV8214_REG_CONFIG4, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
	}

	// ----------------------------------------------------------------------
	// REGISTER: CONFIG0
	// ----------------------------------------------------------------------
	// 	B		NAME			DEFAULT		SET
	// ----------------------------------------------------------------------
	// 	7		EN_OUT			0			1 (turn on output stage)
	// 	6		EN_OVP			1			1
	// 	5		EN_STALL		1			0 (disable stall detection)
	// 	4		VSNS_SEL		0			0
	// 	3		VM_GAIN_SEL		0			1 (3.92V full scale voltage mode)
	//	2		CLR_CNT			0			0
	//	1		CLR_FLT			0			0
	//	0		DUTY_CTRL		0			0
	TX_Buffer[0] = 0b11100000;
	HAL_I2C_Mem_Write(tile_handle, DRV8214_I2C_ADDR<<1, DRV8214_REG_CONFIG0, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	// ----------------------------------------------------------------------
	// REGISTER: CTRL0
	// ----------------------------------------------------------------------
	// 	B		NAME			DEFAULT		SET
	// ----------------------------------------------------------------------
	// 	7:6		-				00			00
	//	5		EN_SS			0			1		enable soft start/stop
	//	4:3		REG_CTRL		00			11		voltage regulated
	//	2		PWM_FREQ		1			1		25kHz
	//	1:0		W_SCALE			11			11		128
	TX_Buffer[0] = 0b00111111;
	HAL_I2C_Mem_Write(tile_handle, DRV8214_I2C_ADDR<<1, DRV8214_REG_CTRL0, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	// set TINRUSH
	TX_Buffer[0] = 0x1F;
	HAL_I2C_Mem_Write(tile_handle, DRV8214_I2C_ADDR<<1, DRV8214_REG_CONFIG1, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	// set target motor voltage to 3.92 * 180/255 = 2.8V
	TX_Buffer[0] = 180;
	HAL_I2C_Mem_Write(tile_handle, DRV8214_I2C_ADDR<<1, DRV8214_REG_CTRL1, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
}


void tile_drive_dc_i_output(uint8_t en, uint8_t dir)
{
	uint8_t TX_Buffer[1] = {0};
	// ----------------------------------------------------------------------
	// REGISTER: CONFIG4
	// ----------------------------------------------------------------------
	// 	B		NAME			DEFAULT		SET
	// ----------------------------------------------------------------------
	// 	7-6		RC_REP			00
	// 	5		STALL_REP		1
	// 	4		CBC_REP			1
	// 	3		PMODE			1
	//	2		I2C_BC			0			1 (enable I2C control)
	//	1		I2C_EN_IN1		0			<en>
	//	0		I2C_PH_IN2		0			<dir>
	TX_Buffer[0] = 0b00111110 + (en<1) + dir;
	  printf("%d \r\n",tile_handle);

	HAL_I2C_Mem_Write(tile_handle, DRV8214_I2C_ADDR<<1, DRV8214_REG_CONFIG4, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

}
