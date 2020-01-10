/**
  ******************************************************************************
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
 *  \brief I2C communication header file
 *
 */
 
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __i2c_H
#define __i2c_H

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

 /*! 
 *****************************************************************************
 * \brief  Initalize I2C
 * 
 * \param[in]  hi2c : pointer to initalized I2C block
 *
 *****************************************************************************
 */
void i2cInit(I2C_HandleTypeDef *hi2c);


 /*! 
 *****************************************************************************
 * \brief  I2C Read
 *  
 * Reads multiple data on the BUS.
 * 
 * \param[in]  Addr    : I2C Address
 * \param[in]  Reg     : Reg Address 
 * \param[in]  pBuffer : pointer to read data buffer
 * \param[in]  Length  : length of the data
 *
 * \return HAL Error code 
 *****************************************************************************
 */
HAL_StatusTypeDef i2cRead(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length);

 /*! 
 *****************************************************************************
 * \brief  I2C Write
 *  
 * Writes a value in a register of the device through BUS.
 * 
 * \param[in]  Addr    : device address 
 * \param[in]  Reg     : The target register address to write
 * \param[out] pBuffer : The target register value to be written 
 * \param[in]  Length  : buffer size to be written
 *
 * \return HAL Error code 
 *****************************************************************************
 */
HAL_StatusTypeDef i2cWrite(uint8_t Addr, uint8_t Reg,  uint8_t *pBuffer, uint16_t Length);


 /*! 
 *****************************************************************************
 * \brief  I2C Sequential Transmit
 *  
 * This method transmits the given txBuf over I2C with support for sequencial
 * transmits, and repeat start condition
 * 
 * \param[in]  address : device address 
 * \param[in]  txBuf   : buffer to be transmitted
 * \param[in]  txLen   : size of txBuffer
 * \param[in]  last    : true if last data to be transmitted
 * \param[in]  txOnly  : true if no reception is to be performed after
 *
 * \return HAL Error code 
 *****************************************************************************
 */
HAL_StatusTypeDef i2cSequentialTx( uint8_t address, const uint8_t *txBuf, uint16_t txLen, bool last, bool txOnly );


/*! 
 *****************************************************************************
 * \brief  I2C Sequential Receive
 *  
 * This method receives data over I2C. To be used after i2cSequentialTx()
 * 
 * \param[in]  address : device address 
 * \param[in]  rxBuf   : buffer to be place received data
 * \param[in]  txLen   : size of rxBuffer
 *
 * \return HAL Error code 
 *****************************************************************************
 */
HAL_StatusTypeDef i2cSequentialRx( uint8_t address, uint8_t *rxBuf, uint16_t rxLen );


/*! 
 *****************************************************************************
 * \brief  I2C Transmit
 *  
 * This method receives data over I2C. To be used after i2cSequentialTx()
 * 
 * \param[in]  address : device address 
 * \param[in]  txBuf   : buffer to be transmitted
 * \param[in]  txLen   : size of txBuffer
 * \param[in]  rxBuf   : buffer to be place received data
 * \param[in]  txLen   : size of rxBuffer: if 0 no reception is performed
 *
 * \return HAL Error code 
 *****************************************************************************
 */
HAL_StatusTypeDef i2cSequentialTxRx( uint8_t address, const uint8_t *txBuf, uint16_t txLen, uint8_t *rxBuf, uint16_t rxLen );

   
#endif /*__i2c_H */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
