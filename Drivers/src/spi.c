/**
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  * 1. Redistributions of source code must retain the above copyright notice,
  * this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  * this list of conditions and the following disclaimer in the documentation
  * and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of its contributors
  * may be used to endorse or promote products derived from this software
  * without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
*/

/*! \file
 *
 *  \author 
 *
 *  \brief SPI communication handling implementation.
 *
 */
 
/* Includes ------------------------------------------------------------------*/

#include "spi.h"
#include "st_errno.h"
#include "string.h"

#define SPI_TIMEOUT   1000
#define SPI_BUF_LEN   512


static uint8_t   txBuf[SPI_BUF_LEN];
static uint8_t   rxBuf[SPI_BUF_LEN];

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef *pSpi = 0;



/* SPI1 init function */
void MX_SPI1_Init(void)
{

	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
	{
//		_Error_Handler(__FILE__, __LINE__);
	}
}

void spiInit(SPI_HandleTypeDef *hspi)
{
	MX_SPI1_Init();
    pSpi = hspi;
    /* enabling SPI block will put SCLK to output, guaranteeing proper state when spiSelect() gets called */
    __HAL_SPI_ENABLE(hspi);
}

void spiSelect(GPIO_TypeDef *ssPort, uint16_t ssPin)
{
  HAL_GPIO_WritePin(ssPort, ssPin, GPIO_PIN_RESET);
}     

void spiDeselect(GPIO_TypeDef *ssPort, uint16_t ssPin)
{
   HAL_GPIO_WritePin(ssPort, ssPin, GPIO_PIN_SET);
}

HAL_StatusTypeDef spiTxRx(const uint8_t *txData, uint8_t *rxData, uint16_t length)
{  
  if(pSpi == 0)
    return HAL_ERROR;
  
  if(length > SPI_BUF_LEN)
  {
    return HAL_ERROR;
  }
  
  /* Initialize Tx data*/
  if(txData != NULL)
  {
    memcpy(txBuf, txData, length );
  }
  else
  {
    memset(txBuf, 0x00, length );
  }

  return HAL_SPI_TransmitReceive(pSpi, txBuf, (rxData != NULL) ? rxData : rxBuf, length, SPI_TIMEOUT);
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
