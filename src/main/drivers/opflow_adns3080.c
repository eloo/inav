/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

// This file is copied with modifications from project Deviation,
// see http://deviationtx.com

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <platform.h>

#if defined(USE_OPTICAL_FLOW_ADNS3080_SOFTSPI) || defined(USE_OPTICAL_FLOW_ADNS3080)

#include "system.h"
#include "gpio.h"
#include "sensor.h"
#include "opflow.h"
#include "opflow_adns3080.h"

#include "bus_spi.h"
#include "bus_spi_soft.h"

static opflow_t *opflowPtr;

// Register Map for the ADNS3080 Optical OpticalFlow Sensor
#define ADNS3080_PRODUCT_ID            0x00
#define ADNS3080_MOTION                0x02
#define ADNS3080_DELTA_X               0x03
#define ADNS3080_DELTA_Y               0x04
#define ADNS3080_SQUAL                 0x05
#define ADNS3080_CONFIGURATION_BITS    0x0A
#define ADNS3080_MOTION_CLEAR          0x12
#define ADNS3080_FRAME_CAPTURE         0x13
#define ADNS3080_MOTION_BURST          0x50

// ADNS3080 hardware config
#define ADNS3080_PIXELS_X              30
#define ADNS3080_PIXELS_Y              30

// Id returned by ADNS3080_PRODUCT_ID register
#define ADNS3080_PRODUCT_ID_VALUE      0x17

#define DISABLE_ADNS3080()      {GPIO_SetBits(ADNS3080_CSN_GPIO, ADNS3080_CSN_PIN);}
#define ENABLE_ADNS3080()       {GPIO_ResetBits(ADNS3080_CSN_GPIO, ADNS3080_CSN_PIN);}
#define DELAY_ADNS3080()        { volatile int i = 500; while (i) { i--; } }


#ifdef USE_OPTICAL_FLOW_ADNS3080_SOFTSPI
static const softSPIDevice_t softSPIDevice = {
    .sck_gpio = ADNS3080_SCK_GPIO,
    .mosi_gpio = ADNS3080_MOSI_GPIO,
    .miso_gpio = ADNS3080_MISO_GPIO,
    .sck_pin = ADNS3080_SCK_PIN,
    .mosi_pin = ADNS3080_MOSI_PIN,
    .miso_pin = ADNS3080_MISO_PIN,
};
#endif

static void ADNS3080_SpiInit(void)
{
    static bool hardwareInitialised = false;

    if (hardwareInitialised) {
        return;
    }

    if (opflowPtr->hasSoftSPI) {
#ifdef USE_OPTICAL_FLOW_ADNS3080_SOFTSPI
        softSpiInit(&softSPIDevice);
#endif
    }

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
#if defined(STM32F10X)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
#endif
#ifdef STM32F303xC
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
#endif
    // CSN as output
    RCC_AHBPeriphClockCmd(ADNS3080_CSN_GPIO_CLK_PERIPHERAL, ENABLE);
    GPIO_InitStructure.GPIO_Pin = ADNS3080_CSN_PIN;
    GPIO_Init(ADNS3080_CSN_GPIO, &GPIO_InitStructure);

    DISABLE_ADNS3080();

#ifdef ADNS3080_SPI_INSTANCE
    spiSetDivisor(ADNS3080_SPI_INSTANCE, SPI_9MHZ_CLOCK_DIVIDER);
#endif

    hardwareInitialised = true;
}

static uint8_t ADNS3080_TransferByte(uint8_t data)
{
#ifdef USE_OPTICAL_FLOW_ADNS3080_SOFTSPI
    if (opflowPtr->hasSoftSPI) {
        return softSpiTransferByte(&softSPIDevice, data);
    } else
#endif
    {
#ifdef ADNS3080_SPI_INSTANCE
        return spiTransferByte(ADNS3080_SPI_INSTANCE, data);
#else
        return 0;
#endif
    }
}

static uint8_t ADNS3080_WriteReg(uint8_t reg, uint8_t data)
{
    ENABLE_ADNS3080();
    ADNS3080_TransferByte(0x80 | reg);
    DELAY_ADNS3080();
    ADNS3080_TransferByte(data);
    DISABLE_ADNS3080();
    return true;
}

static uint8_t ADNS3080_ReadReg(uint8_t reg)
{
    ENABLE_ADNS3080();
    ADNS3080_TransferByte(reg);
    DELAY_ADNS3080();
    const uint8_t ret = ADNS3080_TransferByte(0xFF);
    DISABLE_ADNS3080();
    return ret;
}

void opflowADNS3080Init(void)
{
}

bool opflowADNS3080Read(int16_t *opflowDataPtr)
{
    opflow_data_t * opflowData = (opflow_data_t*)opflowDataPtr;

    opflowData->delta[0] = 0;
    opflowData->delta[1] = 0;
    opflowData->quality = 0;

    return false;
}

bool opflowADNS3080Detect(opflow_t *opflow)
{
#ifdef USE_OPTICAL_FLOW_ADNS3080_SOFTSPI
    if (!opflow->hasSoftSPI) {
        return false;
    }
#endif

    opflowPtr = opflow;

    ADNS3080_SpiInit();

    const uint8_t reg = ADNS3080_ReadReg(ADNS3080_PRODUCT_ID);

    if (reg == ADNS3080_PRODUCT_ID_VALUE) {
        opflow->init = opflowADNS3080Init;
        opflow->read = opflowADNS3080Read;
        return true;
    }

    return false;
}

#endif
