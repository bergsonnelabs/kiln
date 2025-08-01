
#include "tile_drive_h.h"

#define DRIVE_H_EN_GPIO_PORT        GPIOA
#define DRIVE_H_EN_PIN              GPIO_PIN_1

I2C_HandleTypeDef* drive_h_handle;

uint8_t tile_drive_h_init(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	uint8_t TX_Buffer[1] = {0};

	drive_h_handle = hi2c;

	HAL_I2C_Mem_Read(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_STATUS, 1, (uint8_t *)RX_Buffer, 1, 1000);
	if(RX_Buffer[0] != TILE_DRIVE_H_REG_STATUS_DEFAULT){
		return 0;
	}

	HAL_GPIO_WritePin(DRIVE_H_EN_GPIO_PORT, DRIVE_H_EN_PIN, 1);

	// EXIT STANDBY
	TX_Buffer[0] = 0b00000000; // reset
	HAL_I2C_Mem_Write(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_MODE, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
	HAL_Delay(400);

	// FEEDBACK CONTROL
	// 7 	1		N_ERM_LRA
	// 6-4	011		FB_BRAKE_FACTOR
	// 3-2	01		LOOP_GAIN
	// 1-0	10		BEMF_GAIN
	TX_Buffer[0] = 0b10110110; // 10110110
	HAL_I2C_Mem_Write(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_FEEDBACK_CTRL, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);
	HAL_Delay(100);

	// CONTROL3
	// 7-6	00		NG_THRESH
	// 5	0		ERM_OPEN_LOOP
	// ...
	//	0	1		LRA_OPEN_LOOP

	TX_Buffer[0] = 0x01; // 10110110
	HAL_I2C_Mem_Write(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_CONTROL3, I2C_MEMADD_SIZE_8BIT, (uint8_t *)TX_Buffer, 1, 1000);

	TX_Buffer[0] = 6; // LRA library
	HAL_I2C_Mem_Write(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_LIBRARY_SEL, I2C_MEMADD_SIZE_8BIT, (uint8_t *)TX_Buffer, 1, 1000);

	return 1; // success
}

void tile_drive_h_play(uint8_t index, uint8_t repeat){
	uint8_t TX_Buffer[1] = {0};
	TX_Buffer[0] = index; // 10110110
	HAL_I2C_Mem_Write(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_WAVE_SEQ_0, I2C_MEMADD_SIZE_8BIT, (uint8_t *)TX_Buffer, 1, 1000);
	TX_Buffer[0] = 0x01; // 10110110
	for(int i=0; i<repeat; i++){
		HAL_I2C_Mem_Write(drive_h_handle, TILE_DRIVE_H_I2C_ADDR<<1, TILE_DRIVE_H_REG_GO, I2C_MEMADD_SIZE_8BIT, (uint8_t *)TX_Buffer, 1, 1000);
		HAL_Delay(200);
	}
}
