
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
 *      PROJECT:   NFCC firmware
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

/*
 ******************************************************************************
 * INCLUDES
 ******************************************************************************
 */
#include <stdint.h>
#include "utils.h"
#include "st_errno.h"
#include "uart_driver.h"
#include "platform.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */


/*
 ******************************************************************************
 * MACROS
 ******************************************************************************
 */

/*! Executes the system callback, if previously defined               */
#define uartExecUpperLayerCallback()    if(uartSysRerunCb != NULL) uartSysRerunCb()


/*
******************************************************************************
* GLOBAL TYPES
******************************************************************************
*/
/*! UART information */
typedef struct _uartInf
{
    UART_HandleTypeDef   *hUART;     /*!< UART handler */
    ReturnCode           lastError;  /*!< error code of the last error */
    uint32_t             rxLastRead; /*!< Saves the last location of the read */
} uartInf;

/*
 ******************************************************************************
 * VARIABLES
 ******************************************************************************
 */

/*! Receive Buffer for UART0, needs to be aligned at 32 bit boundary in order to use DMA internal ring buffer.  */
static uint8_t uart0ReceiveDMABuffer[UART_DMA_BUFFER_SIZE] __attribute__ ((aligned (UART_DMA_BUFFER_SIZE)));

/*! Transmit Buffer for UART0, needs to be aligned at 32 bit boundary in order to use DMA internal ring buffer. */
static uint8_t uart0TransmitDMABuffer[UART_DMA_BUFFER_SIZE] __attribute__ ((aligned (UART_DMA_BUFFER_SIZE)));

static uartUpperLayerCallback uartSysRerunCb;             /*!< System callback for indication of an event that may require the system to be notified */
static uartInf uartInfo[UART_MAX_NUMBER_OF_UARTS] = {NULL, ERR_NONE, 0, NULL, ERR_NONE, 0}; /*!< Information about each uart*/

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/
static uint32_t calculateAvailableBytesToReceive( uint32_t current, uint32_t lastRead );


/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/
void uartHandleInterrupt( uint8_t id )
{
    if (id >= UART_MAX_NUMBER_OF_UARTS)
    {
        /* Can't even read from an UART because it could be the wrong one */
        return;
    }

    /* Check if there was an UART overrun */
    if(  __HAL_UART_GET_FLAG( uartInfo[id].hUART, HAL_UART_ERROR_ORE) )
    {
        uartInfo[id].lastError = ERR_INSERT_UART_GRP(ERR_HW_OVERRUN);
    }
    
    
    /*
     * Due to the use of a transmit buffer obscured to the uart (the
     * TPT outbound message queue) it might be the case that supperloop
     * needs to rerun if a byte has been transmitted.
     */
    uartExecUpperLayerCallback();
}

ReturnCode uartCreate( uint8_t id, uint32_t rxBufferSize, uint32_t txBufferSize )
{
    if (id >= UART_MAX_NUMBER_OF_UARTS)
    {
        return ERR_INSERT_UART_GRP(ERR_PARAM);
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return ERR_INSERT_UART_GRP(ERR_INVALID_HANDLE);
    }
    
    uartInfo[id].lastError = ERR_NONE;
    return ERR_NONE;
}

/*******************************************************************************/
ReturnCode uartInitialize( uint8_t id, uint32_t clkSourceFrequency, uint32_t baudRate, uint32_t * realBaudRate )
{
    if (id >= UART_MAX_NUMBER_OF_UARTS)
    {
        return ERR_INSERT_UART_GRP(ERR_PARAM);
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return ERR_INSERT_UART_GRP(ERR_INVALID_HANDLE);
    }
    
    return uartInfo[id].lastError;
}

/*******************************************************************************/
ReturnCode uartReset( uint8_t id )
{
    if (id >= UART_MAX_NUMBER_OF_UARTS)
    {
        return ERR_INSERT_UART_GRP(ERR_PARAM);
    }
    
    /* Stop all ongoing DMA Tx and Rx */
    HAL_UART_DMAStop(uartInfo[id].hUART);
    HAL_UART_AbortTransmit(uartInfo[id].hUART);
    HAL_UART_AbortReceive(uartInfo[id].hUART);

    /* Clear Tx DMA cnt, as DMA may have been stopped abruptly */
    uartInfo[id].hUART->hdmatx->Instance->CNDTR = 0;
    
    /* Clear Rx Last read */
    uartInfo[id].rxLastRead = 0;
    
    /* Prepare UART for DMA reception */
    HAL_UART_Receive_DMA( uartInfo[id].hUART, uart0ReceiveDMABuffer, UART_DMA_BUFFER_SIZE );
        
    return ERR_NONE;
}

/*******************************************************************************/
ReturnCode uartDeinitialize( uint8_t id )
{
    if (id >= UART_MAX_NUMBER_OF_UARTS)
    {
        return ERR_INSERT_UART_GRP(ERR_PARAM);
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return ERR_INSERT_UART_GRP(ERR_INVALID_HANDLE);
    }
    
    HAL_UART_DMAStop( uartInfo[id].hUART );
    
    return ERR_NONE;
}

/*******************************************************************************/
void uartSetUpperLayerCallback( uartUpperLayerCallback pFunc )
{
    uartSysRerunCb = pFunc;
}

/*******************************************************************************/
uint32_t uartMaxTxBytes( uint8_t id )
{
    HAL_UART_StateTypeDef uartSt;
    
    if( id >= UART_MAX_NUMBER_OF_UARTS )
    {
        return 0;
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return 0;
    }
   
    uartSt = HAL_UART_GetState( uartInfo[id].hUART );
    if( (uartInfo[id].hUART->hdmatx->Instance->CNDTR == 0) && (uartSt != HAL_UART_STATE_BUSY_TX) && (uartSt != HAL_UART_STATE_BUSY_TX_RX) )
    { 
        return (UART_DMA_BUFFER_SIZE - 1);
    }

    return 0;
}

/*******************************************************************************/
uint32_t uartTxNBytes( uint8_t id, const uint8_t * buffer, uint32_t size )
{
    HAL_StatusTypeDef ret;
    
    if( (id >= UART_MAX_NUMBER_OF_UARTS) || (buffer == 0) || (size == 0) )
    {
        return 0;
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return 0;
    }

    if( id == UART0 )
    {
        if( uartMaxTxBytes(id) > size ) 
        {
            /* Copy data to internal DMA buffer */
            ST_MEMCPY( uart0TransmitDMABuffer, buffer,  size );
            
            /* Trigger a DMA transmission */
            ret = HAL_UART_Transmit_DMA( uartInfo[id].hUART, (uint8_t*)uart0TransmitDMABuffer, size );
            if( ret == HAL_OK )
            {
                return size;
            }
        }
    }
    
    return 0;
}

/*******************************************************************************/
uint32_t uartRxBytesReadyForReceive( uint8_t id )
{
    if( id >= UART_MAX_NUMBER_OF_UARTS )
    {
        return 0;
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return 0;
    }
    
    return calculateAvailableBytesToReceive( (UART_DMA_BUFFER_SIZE - uartInfo[id].hUART->hdmarx->Instance->CNDTR), uartInfo[id].rxLastRead);
}

/*******************************************************************************/
uint32_t uartRxNBytes( uint8_t id, uint8_t * buffer, uint32_t maxSize )
{
    uint32_t cnt;

    if( id >= UART_MAX_NUMBER_OF_UARTS )
    {
        return 0;
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return 0;
    }

    for( cnt = 0; 0 < uartRxBytesReadyForReceive(id) && (cnt < maxSize); ++cnt )
    {
        if( buffer != NULL ) /* copy data, otherwise discard */
        {
            buffer[cnt] = uart0ReceiveDMABuffer[uartInfo[id].rxLastRead];
        }
        uartInfo[id].rxLastRead++;
        uartInfo[id].rxLastRead &= (UART_DMA_BUFFER_SIZE - 1);
    }
    
    return cnt;
}

/*******************************************************************************/
uint32_t uartRxDiscardNBytes( uint8_t id, uint32_t size )
{
    return uartRxNBytes(id, NULL, size);
}

/*******************************************************************************/
ReturnCode uartLastError( uint8_t id )
{
    ReturnCode lastError;

    if( id >= UART_MAX_NUMBER_OF_UARTS )
    {
        return ERR_INSERT_UART_GRP(ERR_PARAM);
    }
    
    if( uartInfo[id].hUART == NULL )
    {
        return ERR_INSERT_UART_GRP(ERR_INVALID_HANDLE);
    }

    lastError = uartInfo[id].lastError;
    uartInfo[id].lastError = ERR_NONE;

    return lastError;
}

/*******************************************************************************/
void uartSetHandler( uint8_t id, UART_HandleTypeDef *husart )
{
    if (id >= UART_MAX_NUMBER_OF_UARTS)
    {
        return;
    }
    
    uartInfo[id].hUART = husart;
}

/*
 ******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************
 */

/* Calculates how many bytes can be received. Therefore it is crucial to know if an overflow has happened.
 * If NO overflow has happened then the available bytes are just the difference between the current position and the last read position
 * If OVERFLOW occurred the difference since the last read position to the end of the buffer has to be calculated first and then
 * added to the current position to get the values.
 */
static uint32_t calculateAvailableBytesToReceive( uint32_t current, uint32_t lastRead )
{
    uint32_t result;

    result = (current - lastRead);
    result &= (UART_DMA_BUFFER_SIZE - 1);

    return result;
}
