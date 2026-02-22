#include "tile_sense_i_9.h"

I2C_HandleTypeDef* sense_i_9_handle;

void icm20948_write(uint8_t reg, uint8_t value);
void ak09916_write(uint8_t reg, uint8_t value);

uint8_t tile_sense_i_9_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, ICM20948_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_i_9_init(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];

	sense_i_9_handle = hi2c;

	// CHECK WHO_AM_I
	HAL_I2C_Mem_Read(sense_i_9_handle, ICM20948_I2C_ADDR<<1, ICM20948_REG_WHOAMI, 1, (uint8_t *)RX_Buffer, 1, 1000);
	if(RX_Buffer[0] != ICM20948_REG_WHOAMI_DEFAULT){
		return 0;
	}

	icm20948_write(ICM20948_REG_PWR_MGMT_1,1<<7);		// reset
	HAL_Delay(50);

    icm20948_write(ICM20948_REG_PWR_MGMT_1, 1);			// WAKE (6=0) & AUTO CLK (2:0 = 1)
    icm20948_write(ICM20948_REG_PWR_MGMT_2, 7);         // DISABLE GYRO (--000111)
    icm20948_write(ICM20948_REG_PWR_MGMT_2, 0x00);      // ENABLE GYRO + ACCEL (--000000)
    icm20948_write(ICM20948_REG_INT_PIN_CFG, 0x02);     // BYPASS_EN for talking to the magnetometer directly

    icm20948_write(ICM20948_REG_BANK_SEL, ICM20948_BANK_0);  // switch to bank 0

    ak09916_write(AK09916_REG_CNTL2, 0x06);              // initialize the magnetometer

	return 1;
}

void tile_sense_i_9_set_accel_range(a_range_t accel_range)
{
    // SET ACCEL RANGE
    icm20948_write(ICM20948_REG_BANK_SEL, ICM20948_BANK_2);  // switch to bank 2
    icm20948_write(ICM20948_REG_ACCEL_CONFIG, accel_range);  // set range and FCHOICE=0 to disable internal filter

}

void tile_sense_i_9_set_gyro_range(g_range_t gyro_range)
{
    icm20948_write(ICM20948_REG_BANK_SEL, ICM20948_BANK_2);  // switch to bank 2
    icm20948_write(ICM20948_REG_GYRO_CONFIG, gyro_range);  // set range and FCHOICE=0 to disable internal

}

void tile_sense_i_9_get_raw_accels(int16_t* buffer){
	HAL_I2C_Mem_Read(sense_i_9_handle, ICM20948_I2C_ADDR<<1, ICM20948_REG_ACCEL_X_H, 1, (uint8_t *)buffer, 6, 1000);
	for(int i=0;i<3;i++){
		buffer[i] = (int16_t*)__builtin_bswap16(buffer[i]);
	}
}

void tile_sense_i_9_get_raw_6dof(int16_t* buffer){
	HAL_I2C_Mem_Read(sense_i_9_handle, ICM20948_I2C_ADDR<<1, ICM20948_REG_ACCEL_X_H, 1, (uint8_t *)buffer, 12, 1000);
	for(int i=0;i<6;i++){
		buffer[i] = (int16_t*)__builtin_bswap16(buffer[i]);
	}
}

void tile_sense_i_9_get_raw_mags(int16_t* buffer){
	uint8_t RX_Buffer[1];

	HAL_I2C_Mem_Read(sense_i_9_handle, AK09916_I2C_ADDR<<1, AK09916_REG_HXL, 1, (uint8_t *)buffer, 6, 1000);
	HAL_I2C_Mem_Read(sense_i_9_handle, AK09916_I2C_ADDR<<1, AK09916_REG_ST2, 1, RX_Buffer, 1, 1000);
}

// PRIVATE FUNCTIONS

void icm20948_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(sense_i_9_handle, ICM20948_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}

void ak09916_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(sense_i_9_handle, AK09916_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);

}
