#include "tile_sense_tof.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
uint8_t tmf8806_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, TMF8806_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void tmf8806_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, TMF8806_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_tof_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, TMF8806_I2C_ADDR<<1, TMF8806_REG_ID, 1, (uint8_t *)RX_Buffer, 1, 1000);
	if( (RX_Buffer[0] & 0b00111111) == TMF8806_REG_ID_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_tof_init(I2C_HandleTypeDef* hi2c)
{
	int i=0;
	tile_handle = hi2c;

	HAL_Delay(100);
	// wait until 0xE0 = 0x00
	i=5;
	while(i-- >= 0){
		if(tmf8806_read(TMF8806_REG_ENABLE)==0x00){
			break;
		} else {
			if(i==0){
				return 0;
			}
			HAL_Delay(100);
		}
	}
	tmf8806_write(TMF8806_REG_ENABLE, 0x01);

	// wait until 0xE0 = 0x41
	i=5;
	while(i-- >= 0){
		if(tmf8806_read(TMF8806_REG_ENABLE)==0x41){
			break;
		} else {
			if(i==0){
				return 0;
			}
			HAL_Delay(10);
		}
	}

	tmf8806_write(TMF8806_REG_APPREQID, 0xC0); // start ROM application

	// wait until 0x00 = 0xC0
	i=5;
	while(i-- >= 0){
		if(tmf8806_read(TMF8806_REG_APPID)==0xC0){
			break;
		} else {
			if(i==0){
				return 0;
			}
			HAL_Delay(10);
		}
	}

	uint8_t Calibration_Buffer[14] = {0x02, 0x00, 0x00, 0x12, 0x70, 0xFE, 0x01, 0x04, 0x07, 0x08, 0x36, 0x24, 0x00, 0x04};
	HAL_I2C_Mem_Write(tile_handle, TMF8806_I2C_ADDR<<1, 0x20, 1, (uint8_t *)Calibration_Buffer, 14, 1000);


	uint8_t TX_Buffer[11] = {0x00, 0x00, 0x11, 0x02, 0x00, 0x00, 0x06, 0x1E, 0x84, 0x03, 0x02};
	// 9 - cmd_data9 (SpreadSpectrumSpadChargePump)
	// 0x00 = disabled

	// 8 - cmd_data8 (SpreadSpectrumVcselChargePump)
	// 0x00 = disabled

	// 7 - cmd_data7 (calib data bitmask)
	// 0x11 (0001 0001)

	// 6 - algorithm selection
	// 0x02 = distanceEnabled up to 2.5m

	// 5 - GPIO control
	// 0x00 = disabled

	// 4 - delay control
	// 0x00

	// 3 - detection threshold
	// 0x06 =

	// 2 - repetition period in ms (0xFE = 1000ms, 0xFF = 2000ms)
	// 0x1E

	// 1 - number of iterations LSB
	// 0x84

	// 0 - nubmer of iterations MSB
	// 0x03

	// 10 = COMMAND
	// 0x02 = start measurement with preceding 10 byes of configuration

	HAL_I2C_Mem_Write(tile_handle, TMF8806_I2C_ADDR<<1, TMF8806_REG_CMD_DATA9, 1, (uint8_t *)TX_Buffer, 11, 1000);


	// wait until 0xE1 = 0x01, signaling that a measurement is complete
	i=5;
	while(i-- >= 0){
		if(tmf8806_read(TMF8806_REG_INT_STATUS) & 0x01){
			break;
		} else {
			if(i==0){
				return 0;
			}
			HAL_Delay(10);
		}
	}

	// clear the interrupt flag
	tmf8806_write(TMF8806_REG_INT_STATUS, 0x01);

	return 1;
}

uint16_t tile_sense_tof_get_measurement(void)
{
	uint8_t RX_Buffer[7];
	HAL_I2C_Mem_Read(tile_handle, TMF8806_I2C_ADDR<<1, TMF8806_REG_STATUS, 1, (uint8_t *)RX_Buffer, 7, 1000);
	// 0 (0x1D) STATUS
	// 0x00-0x0F = good; else = bad

	// 1 (0x1E) REGISTER_CONTENTS
	// 0x0A = calibration; 0x47 = serial; 0x55 = result; 0x80-0x93 = histogram data

	// 2 (0x1F) TID
	// unique transaction ID

	// 3 (0x20) RESULT_NUMBER
	// incremented each result

	// 4 (0x21) RESULT_INFO
	//

	// 5 (0x22) DISTANCE_PEAK_LSB
	//

	// 6 (0x23) DISTANCVE_PEAK_MSB
	//

	// clear the interrupt flag
	tmf8806_write(TMF8806_REG_INT_STATUS, 0x01);

	return ((uint16_t)RX_Buffer[5] + ((uint16_t)RX_Buffer[6]<<8));
}

