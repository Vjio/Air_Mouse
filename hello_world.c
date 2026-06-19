/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017, 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "fsl_swm.h"
#include "board.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
// register for Accelerometer data
#define ACCEL_XOUT_H        0x3B
// my MPU has a bias of -4 for y
#define MPU_Y_BIAS_FIX      4
#define DEADZONE_TRESHHOLD  3

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Main function
 */
int main(void)
{
    BOARD_InitHardware();
    PRINTF("\r\n--- Starting MPU6500 I2C Test ---\r\n");
    // Turn on the Switch Matrix clock to route the pins
    CLOCK_EnableClock(kCLOCK_Swm);

    // Route I2C0_SDA to PIO0_11 and I2C0_SCL to PIO0_10
    SWM_SetFixedPinSelect(SWM0, kSWM_I2C0_SDA, true);
    SWM_SetFixedPinSelect(SWM0, kSWM_I2C0_SCL, true);

    // Turn off the Switch Matrix clock
    CLOCK_DisableClock(kCLOCK_Swm);

    // Configure the I2C Master hardware settings (100kHz speed)
    CLOCK_Select(kI2C0_Clk_From_MainClk);
    i2c_master_config_t masterConfig;
    I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = 100000;
    
    // Initialize I2C0 (30MHz clock speed)
    I2C_MasterInit(I2C0, &masterConfig, 30000000);

    // Set up our I2C "envelope" to read the WHO_AM_I register
    i2c_master_transfer_t masterXfer;

    // wake up the MPU
    uint8_t wakeCmd = 0x00;
    masterXfer.slaveAddress   = 0x68;
    masterXfer.direction      = kI2C_Write; 
    masterXfer.subaddress     = 0x6B;       // PWR_MGMT_1 register
    masterXfer.subaddressSize = 1;
    masterXfer.data           = &wakeCmd;
    masterXfer.dataSize       = 1;
    masterXfer.flags          = kI2C_TransferDefaultFlag;
    
    I2C_MasterTransferBlocking(I2C0, &masterXfer);

    uint8_t rawData[14]; // Buffer for Accel X,Y,Z, Temp, and Gyro X,Y,Z
    while(1) {
        masterXfer.slaveAddress   = 0x68; // MPU6500 Address (with AD0 to GND)
        masterXfer.direction      = kI2C_Read;
        masterXfer.subaddress     = ACCEL_XOUT_H;
        masterXfer.subaddressSize = 1;
        masterXfer.data           = rawData;
        masterXfer.dataSize       = 14;
        masterXfer.flags          = kI2C_TransferDefaultFlag;

        status_t status = I2C_MasterTransferBlocking(I2C0, &masterXfer);

        if (status == kStatus_Success) {
            // I2C registers are 8 bit low but each axis is 16 bites wide
            // => the data gets split in half
            // Reassemble the bytes (Shift high byte, OR with low byte)
            int16_t accelX = (int16_t)((rawData[0] << 8) | rawData[1]);
            int16_t accelY = (int16_t)((rawData[2] << 8) | rawData[3]);
            int16_t accelZ = (int16_t)((rawData[4] << 8) | rawData[5]);
            
            int16_t gyroX  = (int16_t)((rawData[8] << 8) | rawData[9]);
            int16_t gyroY  = (int16_t)((rawData[10] << 8) | rawData[11]);
            int16_t gyroZ  = (int16_t)((rawData[12] << 8) | rawData[13]);

            // converting to physical units
            // Accel formula : Multiply by 1000 to get milli-g, then divide by the 16384 scale factor
            // Gyro formula: Divide by the 131 scale factor to get degrees per second
            int gx_dps = (int)gyroX / 131;
            int gy_dps = (int)gyroY / 131;
            int gz_dps = (int)gyroZ / 131;
            gy_dps += MPU_Y_BIAS_FIX;

            // deadsone filter
            int mouseX = 0;
            int mouseY = 0;
            
            if (gz_dps > DEADZONE_TRESHHOLD || gz_dps < -DEADZONE_TRESHHOLD) {
                mouseX = gz_dps; 
            }
            if (gy_dps > DEADZONE_TRESHHOLD || gy_dps < -DEADZONE_TRESHHOLD) {
                mouseY = gy_dps; 
            }

            // Print human readable data
            // nxp printf can't hanlde negative numbers
            if (mouseX < 0 && mouseY < 0) {
                PRINTF("-%d,-%d\r\n", -mouseX, -mouseY);
            } else if (mouseX < 0) {
                PRINTF("-%d,%d\r\n", -mouseX, mouseY);
            } else if (mouseY < 0) {
                PRINTF("%d,-%d\r\n", mouseX, -mouseY);
            } else {
                PRINTF("%d,%d\r\n", mouseX, mouseY);
            }
        } else {
            PRINTF("\nRead Failed!\n");
        }

        // Small delay so we don't flood the terminal
        for (volatile int i = 0; i < 500000; i++);
    }
}
// stty -F /dev/ttyACM0 9600 raw -echo && cat /dev/ttyACM0

