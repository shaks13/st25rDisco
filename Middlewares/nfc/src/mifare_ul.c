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
 *      PROJECT:   ST25R3911 firmware
 *      $Revision: $
 *      LANGUAGE:  ANSI C
 */

/*! \file
 *
 *  \author Ulrich Herrmann
 *
 *  \brief Mifare UL read and write command implementation
 *
 */

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"
#include "mifare_ul.h"
#include "iso14443_common.h"
#include "utils.h"
#include "rfal_rf.h"

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/*
******************************************************************************
* LOCAL DEFINES
******************************************************************************
*/
#define MIFARE_UL_CMD_READ 0x30
#define MIFARE_UL_CMD_WRITE 0xA2

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/
ReturnCode mifareUlReadNBytes(uint8_t startAddr, uint8_t* readbuf, uint8_t length, uint8_t* actLength)
{
    ReturnCode err;
    uint8_t txbuf[2];
    uint16_t actrxlength;
    uint8_t rxbuf[16];

    *actLength = 0;

    if (startAddr > 0xf)
    {
        return ERR_PARAM;
    }

    txbuf[0] = MIFARE_UL_CMD_READ;

    do
    {
        txbuf[1] = startAddr;
        
        err = rfalTransceiveBlockingTxRx( (uint8_t*)txbuf, sizeof(txbuf), rxbuf, sizeof(rxbuf), &actrxlength, RFAL_TXRX_FLAGS_DEFAULT, rfalConvMsTo1fc(5) );
        EXIT_ON_ERR(err, ERR_NONE);

        if (actrxlength != sizeof(rxbuf))
        {
            /* only NAK received or less than 16 bytes */
            return ERR_NOMSG;
        }
        if (actrxlength > length)
        {
            length = 0;
        }
        else
        {
            length -= actrxlength;
        }
        /* copy received bytes to output buffer */
        ST_MEMCPY(readbuf, rxbuf, actrxlength);
        readbuf += actrxlength;
        *actLength += actrxlength;

        startAddr += 4;
        /* roll back in case we go behind 0xf */
        if (startAddr > 0xf)
        {
            startAddr -= 0x10;
        }
    } while (length > 0);

    return err;
}


ReturnCode mifareUlWritePage(uint8_t pageAddr, const uint8_t* writebuf)
{
    ReturnCode err;
    uint8_t txbuf[6];
    uint16_t actrxlength;
    uint8_t rxbuf;

    txbuf[0] = MIFARE_UL_CMD_WRITE;
    txbuf[1] = pageAddr;
    ST_MEMCPY(&txbuf[2], (uint8_t*)writebuf, 4);

    err = rfalTransceiveBlockingTx( txbuf, sizeof(txbuf), &rxbuf, sizeof(rxbuf), &actrxlength, (RFAL_TXRX_FLAGS_CRC_TX_AUTO | RFAL_TXRX_FLAGS_CRC_RX_KEEP), rfalConvMsTo1fc(10) );
    err = rfalTransceiveBlockingRx();
    
    
    if( (ERR_INCOMPLETE_BYTE == err)  && (actrxlength == 4) )
    {
        if (0x0a == rxbuf)
        {
            return ERR_NONE;
        }
        else 
        {
            return ERR_WRITE;
        }
    }

    /* map receive timeout to not found error */
    if ((ERR_TIMEOUT == err) ||
            (ERR_NOMSG == err))
    {
        err = ERR_NOTFOUND;
    }

    return err;
}

