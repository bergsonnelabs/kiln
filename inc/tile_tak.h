/*
 * tile_tak.h
 *
 *  Created on: Jun 6, 2025
 *      Author: jonathanfiene
 */

#ifndef INC_TILE_TAK_H_
#define INC_TILE_TAK_H_


#include "stdint.h"
#include "i2c.h"

#define BOS1921_I2C_ADDR	0x44

#define BOS_REG_REFERENCE   0x00
#define BOS_REG_CONFIG      0x05
// BITS                 DEF     SET     NOTES
// ----------------------------------------------------------------------------------
//  15      ONCOMP      0       0       activate sensing comparator
//  14      AUTO        0       0       trigger auto play
//  13      SENSE       0       0       enable piezo sensing
//  12      GAINS       1       0       sensing resolution LSB mV (0 = 54.5; 1 = 7.6)
//
//  11      GAIND       0       0       output voltage range (0 = +/-95; 1 = +/-13.28)
//  10:9    PLAY_MODE   0       0       (0=direct; 1=FIFO; 2=RAM; 3=RAM Synth)
//          ^           0       0       ^
//  8       RET         0       0       retention during sleep (0=enabled; 1=disabled)
//
//  7       SYNC        0       0       multi-chip sync (0=disable; 1=enable)
//  6       RST         0       0       (0=normal; 1=software reset)
//  5       POL_SENSE   0       0       sensing polarity to VDD (0=OUT-; 1=OUT+)
//  4       OE          0       0       output enable (0=disable; 1=enable)
//
//  3       DS          0       0       mode when not playback (0=idle; 1=sleep)
//  2:0     PLAY_SRATE  0       0       sample rate in ksps (0=1024; ... 7=8)
//          ^           0       0       ^
//          ^           0       0       ^
#define BOS_REG_PARCAP      0x06
#define BOS_REG_SUP_RISE	0x07
#define BOS_REG_COMM        0x0B

#define BOS_REG_IC_STATUS   0x10
// BITS                 DEF     SET     NOTES
// ----------------------------------------------------------------------------------
//  9:8     STATE       0 = idle; 1 = cal; 2 = run; 3 = error
//  -       ^           ^

//  7       OVV         1 = overvoltage
//  6       OCT         1 = overtemp
//  5       MXPWR       1 = max power warning (max current)
//  4       IDAC        1 = problem with current detection (must reset)

//  3       UVLO        1 = VDD under-voltage fault
//  2       SC          1 = piezo short circuit fault
//  1       FULL        1 = FIFO is full
//  0       PLAYST      0/1 = mode dependent

#define BOS_REG_SENSE_VAL   0x18
#define BOS_REG_CHIP_ID     0x1E

#define TILE_TAK_MODE_IDLE           0
#define TILE_TAK_MODE_SENSE_FINE     1
#define TILE_TAK_MODE_SENSE_COARSE   2
#define TILE_TAK_MODE_PLAY_DIRECT    3
#define TILE_TAK_MODE_PLAY_FIFO      4
#define TILE_TAK_MODE_DEBUG			 5

uint8_t tile_tak_find(I2C_HandleTypeDef* hi2c, uint8_t index);
void tile_tak_reset(uint8_t lsb);
uint8_t tile_tak_init(uint8_t index);

void tile_tak_set_mode(uint8_t lsb, uint8_t mode);
uint16_t tile_tak_read(uint8_t lsb);
void tile_tak_write(uint8_t lsb, uint8_t reg, uint16_t value);


#endif /* INC_TILE_TAK_H_ */
