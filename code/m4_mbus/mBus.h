//-----------------------------------------------------------------------------
// MAEVARM M4 STM32F373 mBus 
// version: 1.0
// date: March 22, 2013
// author: Neel Shah (neels@seas.upenn.edu)
//-----------------------------------------------------------------------------

#ifndef __m_bus
#define __m_bus

#include "mGeneral.h"

#define mBusStruct I2C1_DevStructure

/*
    mBusGetLastError():
    
    Returns the most recent mBus error code and clears the error.
    Error code can be one of:
    
    CPAL_I2C_ERR_NONE:    No error
    CPAL_I2C_ERR_TIMEOUT: Timeout occurred during communication or configuration
    CPAL_I2C_ERR_BERR:    Bus error
    CPAL_I2C_ERR_ARLO:    Lost arbitration
    CPAL_I2C_ERR_AF:      Acknowledge failure (slave gave a NACK instead of ACK)
    CPAL_I2C_ERR_OVR:     Overrun/underrun error
*/
uint32_t mBusGetLastError (void);

void mBusInit(void);
void mBusRestart(void);
uint8_t mBusPeriphReady(uint8_t slaveAddr);

uint8_t mBusWrite(uint8_t slaveAddr, uint8_t regAddr, uint8_t data);
uint8_t mBusRead (uint8_t slaveAddr, uint8_t regAddr);

uint8_t mBusWriteBurst(uint8_t slaveAddr, uint8_t regAddr, uint16_t length, uint8_t* data);
uint8_t mBusReadBurst (uint8_t slaveAddr, uint8_t regAddr, uint16_t length, uint8_t* data);

uint8_t mBusWriteNoAdd(uint8_t slaveAddr, uint8_t data);
uint8_t mBusReadNoAdd (uint8_t slaveAddr);

uint8_t mBusWriteBurstNoAdd(uint8_t slaveAddr, uint16_t length, uint8_t* data);
uint8_t mBusReadBurstNoAdd (uint8_t slaveAddr, uint16_t length, uint8_t* data);

//Non Blocking functions, user takes care of waiting and other stuff 
uint8_t mBusWriteBurstNB(uint8_t slaveAddr, uint8_t regAddr, uint8_t length, uint8_t* data);
uint8_t mBusReadBurstStartNB(uint8_t slaveAddr, uint8_t regAddr, uint8_t length);
uint8_t mBusReadBurstDataNB (uint8_t length, uint8_t* data);

void CPAL_I2C_ERR_UserCallback(CPAL_DevTypeDef pDevInstance, uint32_t DeviceError);
uint32_t CPAL_TIMEOUT_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_TXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_RXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_TX_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_RX_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_DMATXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_DMATXHT_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_DMATXTE_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
void CPAL_I2C_DMARXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_DMARXHT_UserCallback(CPAL_InitTypeDef* pDevInitStruct);
//void CPAL_I2C_DMARXTE_UserCallback(CPAL_InitTypeDef* pDevInitStruct);

#endif
