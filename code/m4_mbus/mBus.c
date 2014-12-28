//-----------------------------------------------------------------------------
// MAEVARM M4 STM32F373 mBus
// version: 1.0
// date: March 22, 2013
// author: Neel Shah (neels@seas.upenn.edu)
//-----------------------------------------------------------------------------

#include "mBus.h"



/* Initialize TX Transfer structure */
CPAL_TransferTypeDef  mBusRx = { 
                        pNULL, //buffer pointer data 
                        0,     //length of data
                        0,     //slaveAddr
                        0};    //regAddr
/* Initialize RX Transfer structure */
CPAL_TransferTypeDef  mBusTx = {
                        pNULL,
                        0,
                        0,
                        0};

uint8_t bufferWr[2],bufferRd[2];
uint8_t bufferBurstWr[256],bufferBurstRd[256];
extern CPAL_InitTypeDef mBusStruct;
__IO uint8_t mBusReadBurstReady=0, mBusReadBurstStartFlag=0;

void mBusInit(void)
{
	RCC_I2CCLKConfig(RCC_I2C1CLK_SYSCLK);
	RCC_I2CCLKConfig(RCC_I2C2CLK_SYSCLK);
	CPAL_I2C_StructInit(&mBusStruct);
	mBusStruct.CPAL_Dev = CPAL_I2C1;
	mBusStruct.CPAL_Direction = CPAL_DIRECTION_TXRX;
	mBusStruct.CPAL_Mode = CPAL_MODE_MASTER;
	mBusStruct.CPAL_ProgModel = CPAL_PROGMODEL_INTERRUPT;
	mBusStruct.pCPAL_TransferTx = &mBusTx;
	mBusStruct.pCPAL_TransferRx = &mBusRx;
	mBusStruct.wCPAL_Options = CPAL_OPT_I2C_AUTOMATIC_END;
	mBusStruct.pCPAL_I2C_Struct->I2C_Timing = I2C_Timing_FastMode;
	mBusStruct.pCPAL_I2C_Struct->I2C_AnalogFilter = I2C_AnalogFilter_Enable;
	mBusStruct.pCPAL_I2C_Struct->I2C_DigitalFilter = 0x00;
	mBusStruct.pCPAL_I2C_Struct->I2C_Mode = I2C_Mode_I2C;
	mBusStruct.pCPAL_I2C_Struct->I2C_OwnAddress1 = 0x00;
	mBusStruct.pCPAL_I2C_Struct->I2C_Ack = I2C_Ack_Enable;
	mBusStruct.pCPAL_I2C_Struct->I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	CPAL_I2C_Init(&mBusStruct);
}
void mBusRestart(void)
{
	mBusInit();
}
uint8_t mBusPeriphReady(uint8_t slaveAddr)
{
	//if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
	mBusStruct.pCPAL_TransferTx = &mBusTx;     
  mBusStruct.pCPAL_TransferTx->wAddr1 = (uint32_t)slaveAddr;
  	if(CPAL_I2C_IsDeviceReady(&mBusStruct) == CPAL_FAIL)
  	{
  		mBusStruct.CPAL_State = CPAL_STATE_READY;
  		return ERROR;
    }
  	else 
  	{
  		mBusStruct.CPAL_State = CPAL_STATE_READY;
  		return SUCCESS;
  	}
}
uint8_t mBusWrite(uint8_t slaveAddr, uint8_t regAddr, uint8_t data)
{
	if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
	bufferWr[0] = data;
	mBusStruct.pCPAL_TransferTx = &mBusTx; 
  mBusStruct.pCPAL_TransferTx->wNumData = 1;
 	mBusStruct.pCPAL_TransferTx->pbBuffer = bufferWr;
  mBusStruct.pCPAL_TransferTx->wAddr1   = (uint32_t)slaveAddr;
  mBusStruct.pCPAL_TransferTx->wAddr2   = (uint32_t)regAddr;

  if(CPAL_I2C_Write(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if (mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      return ERROR;
    }
    return SUCCESS;
  }
  else 
  {
    return ERROR;
  }
}
uint8_t mBusRead(uint8_t slaveAddr, uint8_t regAddr)
{
	if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
	bufferRd[0] = 0;
	mBusStruct.pCPAL_TransferRx = &mBusRx; 
  mBusStruct.pCPAL_TransferRx->wNumData = 1;
 	mBusStruct.pCPAL_TransferRx->pbBuffer = bufferRd ;
  mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)slaveAddr;
  mBusStruct.pCPAL_TransferRx->wAddr2   = (uint32_t)regAddr;

  if(CPAL_I2C_Read(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
	  if(mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      return ERROR;
    }
    return bufferRd[0];
  }
  else
  {
    return ERROR;
  }	
}
uint8_t mBusWriteBurst(uint8_t slaveAddr, uint8_t regAddr, uint16_t length, uint8_t *data)
{
	if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
	
	if (length > 256)
	    return ERROR;
	
	uint8_t i;
	for(i=0;i<length;i++)
	{
		bufferBurstWr[i] = data[i];	
	}
	mBusStruct.pCPAL_TransferTx = &mBusTx; 
  mBusStruct.pCPAL_TransferTx->wNumData = length;
 	mBusStruct.pCPAL_TransferTx->pbBuffer = bufferBurstWr ;
  mBusStruct.pCPAL_TransferTx->wAddr1   = (uint32_t)slaveAddr;
  mBusStruct.pCPAL_TransferTx->wAddr2   = (uint32_t)regAddr;

  if(CPAL_I2C_Write(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if (mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      return ERROR;
    }
    return SUCCESS;
  }
  else 
  {
    return ERROR;
  }
}
uint8_t mBusReadBurst(uint8_t slaveAddr, uint8_t regAddr, uint16_t length, uint8_t *data)
{
  if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
  
  if (length > 256)
    return ERROR;
  
  mBusStruct.pCPAL_TransferRx = &mBusRx; 
  mBusStruct.pCPAL_TransferRx->wNumData = length;
  mBusStruct.pCPAL_TransferRx->pbBuffer = bufferBurstRd ;
  mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)slaveAddr;
  mBusStruct.pCPAL_TransferRx->wAddr2   = (uint32_t)regAddr;

  if(CPAL_I2C_Read(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if(mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      return ERROR;
    }
    for(uint8_t i=0;i<length;i++)
    {
      data[i] = bufferBurstRd[i]; 
    }
    return SUCCESS;
  }
  else
  {
    return ERROR;
  }
}



uint8_t mBusWriteNoAdd(uint8_t slaveAddr, uint8_t data)
{
  if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
  bufferWr[0] = data;
  mBusStruct.wCPAL_Options |= CPAL_OPT_NO_MEM_ADDR;
  mBusStruct.pCPAL_TransferTx = &mBusTx; 
  mBusStruct.pCPAL_TransferTx->wNumData = 1;
  mBusStruct.pCPAL_TransferTx->pbBuffer = bufferWr;
  mBusStruct.pCPAL_TransferTx->wAddr1   = (uint32_t)slaveAddr;

  if(CPAL_I2C_Write(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if (mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
      return ERROR;
    }
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return SUCCESS;
  }
  else 
  {
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return ERROR;
  }
}
uint8_t mBusReadNoAdd(uint8_t slaveAddr)
{
  if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
  bufferRd[0] = 0;
  mBusStruct.wCPAL_Options |= CPAL_OPT_NO_MEM_ADDR;
  mBusStruct.pCPAL_TransferRx = &mBusRx; 
  mBusStruct.pCPAL_TransferRx->wNumData = 1;
  mBusStruct.pCPAL_TransferRx->pbBuffer = bufferRd ;
  mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)slaveAddr;

  if(CPAL_I2C_Read(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if(mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
      return ERROR;
    }
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return bufferRd[0];
  }
  else
  {
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return ERROR;
  } 
}
uint8_t mBusWriteBurstNoAdd(uint8_t slaveAddr, uint16_t length, uint8_t *data)
{
  if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
  
  if (length > 256)
    return ERROR;
  
  uint16_t i;
  for(i=0;i<length;i++)
  {
    bufferBurstWr[i] = data[i]; 
  }
  mBusStruct.wCPAL_Options |= CPAL_OPT_NO_MEM_ADDR;
  mBusStruct.pCPAL_TransferTx = &mBusTx; 
  mBusStruct.pCPAL_TransferTx->wNumData = length;
  mBusStruct.pCPAL_TransferTx->pbBuffer = bufferBurstWr ;
  mBusStruct.pCPAL_TransferTx->wAddr1   = (uint32_t)slaveAddr;

  if(CPAL_I2C_Write(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if (mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
      return ERROR;
    }
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return SUCCESS;
  }
  else 
  {
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return ERROR;
  }
}
uint8_t mBusReadBurstNoAdd(uint8_t slaveAddr, uint16_t length, uint8_t *data)
{
  if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
  
  if (length > 256)
    return ERROR;
  
  mBusStruct.wCPAL_Options |= CPAL_OPT_NO_MEM_ADDR;
  mBusStruct.pCPAL_TransferRx = &mBusRx; 
  mBusStruct.pCPAL_TransferRx->wNumData = length;
  mBusStruct.pCPAL_TransferRx->pbBuffer = bufferBurstRd ;
  mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)slaveAddr;

  if(CPAL_I2C_Read(&mBusStruct) == CPAL_PASS)
  {
    while((mBusStruct.CPAL_State != CPAL_STATE_READY) && (mBusStruct.CPAL_State != CPAL_STATE_ERROR)){}
    if(mBusStruct.CPAL_State == CPAL_STATE_ERROR)
    {
      mBusStruct.CPAL_State = CPAL_STATE_READY;
      mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
      return ERROR;
    }
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    for(uint8_t i=0;i<length;i++)
    {
      data[i] = bufferBurstRd[i]; 
    }
    return SUCCESS;
  }
  else
  {
    mBusStruct.wCPAL_Options &= ~CPAL_OPT_NO_MEM_ADDR;
    return ERROR;
  }
}

uint8_t mBusWriteBurstNB(uint8_t slaveAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
  if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
  uint8_t i;
  for(i=0;i<length;i++)
  {
    bufferBurstWr[i] = data[i]; 
  }
  mBusStruct.pCPAL_TransferTx = &mBusTx; 
  mBusStruct.pCPAL_TransferTx->wNumData = length;
  mBusStruct.pCPAL_TransferTx->pbBuffer = bufferBurstWr ;
  mBusStruct.pCPAL_TransferTx->wAddr1   = (uint32_t)slaveAddr;
  mBusStruct.pCPAL_TransferTx->wAddr2   = (uint32_t)regAddr;

  if(CPAL_I2C_Write(&mBusStruct) == CPAL_PASS)
  {
    return SUCCESS;
  }
  else 
  {
    return ERROR;
  }
}
uint8_t mBusReadBurstStartNB(uint8_t slaveAddr, uint8_t regAddr, uint8_t length)
{
	if(mBusStruct.CPAL_State != CPAL_STATE_READY){return ERROR;}
	mBusStruct.pCPAL_TransferRx = &mBusRx; 
  mBusStruct.pCPAL_TransferRx->wNumData = length;
 	mBusStruct.pCPAL_TransferRx->pbBuffer = bufferBurstRd ;
  mBusStruct.pCPAL_TransferRx->wAddr1   = (uint32_t)slaveAddr;
  mBusStruct.pCPAL_TransferRx->wAddr2   = (uint32_t)regAddr;

  	if(CPAL_I2C_Read(&mBusStruct) == CPAL_PASS)
  	{
      mBusReadBurstStartFlag=1;
  		return SUCCESS;
  	}
  	else 
  	{
  		return ERROR;
  	}
}
uint8_t mBusReadBurstDataNB(uint8_t length, uint8_t* data)
{
	if(mBusReadBurstReady)
	{
		for(uint8_t i=0;i<length;i++)
    {
      data[i] = bufferBurstRd[i]; 
    }
		mBusReadBurstReady = 0;
    mBusReadBurstStartFlag=0;
		return SUCCESS;
	}
	else
	{
		return ERROR;
	}
}

uint32_t CPAL_TIMEOUT_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
{
/*
  uint32_t i;
	for(i=0; i<10; i++)
    {
    	mGreenON; mRedON; mYellowON; mBlueON; mWhiteON;
    	mWait(800000);
    	mGreenOFF; mRedOFF; mYellowOFF; mBlueOFF; mWhiteOFF;
    	mWait(800000);  	
    }*/
	mBusRestart();
  	return CPAL_PASS;
}

/*=========== Transfer UserCallback ===========*/

//void CPAL_I2C_TXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
//void CPAL_I2C_RXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
//void CPAL_I2C_TX_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ } //
//void CPAL_I2C_RX_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
//void CPAL_I2C_DMATXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
//void CPAL_I2C_DMATXHT_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
//void CPAL_I2C_DMATXTE_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
void CPAL_I2C_DMARXTC_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
{
  if(mBusReadBurstStartFlag = 1)
		{mBusReadBurstReady = 1;}
}
//void CPAL_I2C_DMARXHT_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }
//void CPAL_I2C_DMARXTE_UserCallback(CPAL_InitTypeDef* pDevInitStruct)
//{ }

volatile uint32_t mBusErrorCode = CPAL_I2C_ERR_NONE;

void CPAL_I2C_ERR_UserCallback(CPAL_DevTypeDef pDevInstance, uint32_t DeviceError)
{
    mBusErrorCode = DeviceError;
	mBusRestart();
}

// return and clear the most recent error code
uint32_t mBusGetLastError (void)
{
    uint32_t retval = mBusErrorCode;
    mBusErrorCode = CPAL_I2C_ERR_NONE;
    return retval;
}

