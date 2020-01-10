
/******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
  *
  * Licensed under ST MYLIBERTY SOFTWARE LICENSE AGREEMENT (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/myliberty
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied,
  * AND SPECIFICALLY DISCLAIMING THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
******************************************************************************/


/*
 *      PROJECT:   NFCC
 *      LANGUAGE:  ISO C99
 */

/*! \file
 *
*  \author Gustavo Patricio
 *
 *  \brief UART driver 
 *
 *  This is the implementation file for the UART driver. 
 *  This driver was developed to the STM32 platform and was adapted from 
 *  the original NFCC UART driver.
 *  
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/

#include <stdint.h>
#include "st_errno.h"
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
******************************************************************************
* GLOBAL MACROS
******************************************************************************
*/

/*! Max. number of uarts are available */
#define UART_MAX_NUMBER_OF_UARTS (2)


/*! DMA Buffer size -> can read 512 bytes before DMA interrupt*/
#define UART_DMA_BUFFER_SIZE (512)


/*! UART internal buffer size, size of data in the message queue */
#define UART_BUFFER_ENTRY_SIZE (1)

/*! Uart 0 offset */
#define UART0 (0) 
/*! Uart 1 offset */
#define UART1 (1) 
/*! Uart 2 offset */
#define UART2 (2) 

/*! Clock Frequency for UART0 */
#define UART0_CLK_FREQUENCY (20972000) 
/*! Clock Frequency for UART1 and UART 2 */
#define UART_CLK_FREQUENCY  (20972000) 

/*! 2400 Baud */
#define UART_BAUD_RATE_2400   (2400)   
/*! 4800 Baud */
#define UART_BAUD_RATE_4800   (4800)   
/*! 9600 Baud */
#define UART_BAUD_RATE_9600   (9600)   
/*! 19200 Baud */
#define UART_BAUD_RATE_19200  (19200)  
/*! 38400 Baud */
#define UART_BAUD_RATE_38400  (38400)  
/*! 115200 Baud */
#define UART_BAUD_RATE_115200 (115200) 
/*! UART0 default baud rate */
#define UART0_BAUD_RATE     UART_BAUD_RATE_115200 

/*
******************************************************************************
* GLOBAL DATA TYPES
******************************************************************************
*/

/*! System callback to indicate an event that requires the system to be notified */
typedef void (* uartUpperLayerCallback )(void);

/*
******************************************************************************
* GLOBAL VARIABLE DECLARATIONS
******************************************************************************
*/

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/

/*! 
 *****************************************************************************
 *  \brief Create RX and TX buffers
 *  
 *  Create an internal receive buffer of size rxBufferSize and another
 *  internal transmit buffer with size txBufferSize. The memory for the
 *  transmit and receive buffer is never freed.
 *
 *  \param id : Identifier of the UART
 *  \param rxBufferSize : the size of the receive (RX) buffer
 *  \param txBufferSize : the size of the transmit (TX) buffer
 *
 *  \return ERR_NONE buffers created
 *  \return ERR_PARAM id is out of range
 *  \return ERR_NOMEM not enough memory to create RX and/or TX buffer
 *****************************************************************************
 */
ReturnCode uartCreate( uint8_t id, uint32_t rxBufferSize, uint32_t txBufferSize );

/*! 
 *****************************************************************************
 *  \brief Set UART handler
 *  
 *  Sets the UART handler from STM32 HAL
 * 
 *  \param id : Identifier of the UART
 *  \param huart : UART handler
 *
 *****************************************************************************
 */
void uartSetHandler( uint8_t id, UART_HandleTypeDef *husart );

/*! 
 *****************************************************************************
 *  \brief Initialize UART and set its baud rate
 *  
 *  Configures the hardware module for the given baud rate. The
 *  achieved baudrate may differ from the desired baud rate. The real baud
 *  rate will be written to realBaudRate if provided.
 *
 *  UART is configured as follows:
 *    * Data     : 8 bit
 *    * Parity   : No
 *    * Stop Bit : 1 bit
 *    * Transmit : MSB first
 *    * Receive  : MSB first
 *    * Polarity : normal
 *
 *  \param id : Identifier of the UART
 *  \param clkSourceFrequency : source frequency of the clock
 *  \param baudRate : desired baud rate
 *  \param realBaudRate : baud rate that was actually set
 *
 *  \return ERR_NONE buffers created
 *  \return ERR_PARAM id is out of range
 *****************************************************************************
 */
ReturnCode uartInitialize( uint8_t id, uint32_t clkSourceFrequency, uint32_t baudRate, uint32_t * realBaudRate);

/*! 
 *****************************************************************************
 *  \brief Resets the UART
 *  
 *  Resets the internal structures. Clears any buffers and resets the HW module
 *
 *  \param id : Identifier of the UART
 *
 *  \return ERR_NONE buffers created
 *  \return ERR_PARAM id is out of range
 *****************************************************************************
 */
ReturnCode uartReset( uint8_t id );

/*! 
 *****************************************************************************
 *  \brief Turns the UART off
 *  
 *  Turns off the UART.
 *
 *  \param id : Identifier of the UART
 *
 *  \return ERR_NONE buffers created
 *  \return ERR_PARAM id is out of range
 *****************************************************************************
 */
ReturnCode uartDeinitialize( uint8_t id );

/*! 
 *****************************************************************************
 *  \brief UART driver Interupt Handler
 *  
 *  Handles Interrupts from all three UARTS. UARTS are identified by the passed id.
 *
 *  \param id : Identifier of the UART
 *
 *  \return ERR_NONE buffers created
 *  \return ERR_PARAM id is out of range
 *****************************************************************************
 */
void uartHandleInterrupt( uint8_t id );

/*!
 *****************************************************************************
 * \brief Set System Rerun Callback
 *
 * Sets a callback for the driver to call when an event has occurred that 
 * may require the system to be notified
 *****************************************************************************
 */
void uartSetUpperLayerCallback( uartUpperLayerCallback pFunc );

/*! 
 *****************************************************************************
 *  \brief Get the number of bytes that can be transmitted
 *  
 *  Get the maximum number of bytes that the UART is able to transmit
 *
 *  \param id : Identifier of the UART
 *
 *  \return >= 0 : number of bytes that can be transmitted
 *****************************************************************************
 */
uint32_t uartMaxTxBytes( uint8_t id );

/*! 
 *****************************************************************************
 *  \brief Send N bytes
 *  
 *  Copies up to size bytes from the buffer into the transmit buffer and starts
 *  transmitting immediately.
 *
 *  In case of an error size and the return value will not be the same!
 *
 *  \param id : Identifier of the UART
 *  \param buffer : buffer holding the data
 *  \param size : size of the buffer
 *
 *  \return >= 0 : number of bytes that are transmitted
 *****************************************************************************
 */
uint32_t uartTxNBytes( uint8_t id, const uint8_t * buffer, uint32_t size );

/*! 
 *****************************************************************************
 *  \brief Get number of bytes that can be fetched from buffer
 *  
 *  Get the number of bytes that are already received and waiting in the
 *  buffer to be received.
 *
 *  \param id : Identifier of the UART
 *
 *  \return >= 0 : number of bytes that can be received
 *****************************************************************************
 */
uint32_t uartRxBytesReadyForReceive( uint8_t id );

/*! 
 *****************************************************************************
 *  \brief Get N bytes from receive buffer
 *  
 *  Receive up to maxSize bytes from the internal receive buffer. The data
 *  is saved in buffer. If buffer is a null pointer no data is copied and
 *  the function returns 0.
 *
 *  \param id : Identifier of the UART
 *  \param buffer : buffer for the data to be written into
 *  \param maxSize : size of the buffer.
 *
 *  \return >= 0 : number of bytes that are received
 *****************************************************************************
 */
uint32_t uartRxNBytes( uint8_t id, uint8_t * buffer, uint32_t maxSize );

/*! 
 *****************************************************************************
 *  \brief Discard N bytes from receive buffer
 *  
 *  Discard up to maxSize bytes from the internal receive buffer.
 *
 *  \param id   : Identifier of the UART
 *  \param size : size of the buffer.
 *
 *  \return >= 0 : number of bytes that are received
 *****************************************************************************
 */
uint32_t uartRxDiscardNBytes(uint8_t id, uint32_t size);

/*! 
 *****************************************************************************
 *  \brief Get the last error
 *  
 *  Returns the last error that occurred between the last call of either
 *  uartLastError, uartInitialize or uartReset.
 *
 *  Calling this function clears the error
 *
 *  \param id : Identifier of the UART
 *
 *  \return ERR_NONE no error occured
 *  \return ERR_PARAM id is out of range
 *  \return ERR_OVERRUN one or more received bytes were lost
 *****************************************************************************
 */
ReturnCode uartLastError( uint8_t id );

#ifdef __cplusplus
}
#endif

#endif /* UART_DRIVER_H */

