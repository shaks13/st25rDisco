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
 *  \brief Implementation of Kovio barcode
 *
 */
/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "kovio.h"
#include "kovio.h"
#include "utils.h"
#include "rfal_rf.h"
#include "rfal_nfca.h"
#include "rfal_crc.h"

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/* Kovio barcode tag can transmit at any time */
#define KOVIO_MASK_RECEIVE_TIME 0

/* Kovio barcode transmits every 5 ms = 1067 * 64/fc. */
#define KOVIO_FRAME_DELAY_TIME  1067

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/

uint8_t reverse8(uint8_t x)
{
    x = (((x & 0xaa) >> 1) | ((x & 0x55) << 1));
    x = (((x & 0xcc) >> 2) | ((x & 0x33) << 2));
    x = (((x & 0xf0) >> 4) | ((x & 0x0f) << 4));
    return x;
}

static void kovioNormalizeUID(kovioProximityCard_t *card)
{ /* First bit (always 1) is regarded as SOF, shift it now in*/
  /* iso14443 transmits lsb first, while Kovio is msb first */
    int i;
    uint8_t new_bit = 0;
    uint8_t old_bit = 1;
    uint8_t *b = card->uid;

    for ( i = 0; i < card->length; i++)
    {
        new_bit = b[i] >> 7;
        b[i] <<= 1;
        b[i] |= old_bit;
        b[i] = reverse8(b[i]);
        old_bit = new_bit;
    }
}

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/
ReturnCode kovioInitialize()
{
    rfalNfcaPollerInitialize();
    return rfalFieldOnAndStartGT();
}

ReturnCode kovioDeinitialize(uint8_t keep_on)
{
    if (!keep_on)
    {
        return rfalFieldOff();
    }

    return ERR_NONE;
}

ReturnCode kovioRead(kovioProximityCard_t *card)
{
    int i;
    ReturnCode err;
    uint16_t actrxlength = 0;
    uint16_t crc;

    card->length = 0;

    for (i = 0; i < 2; i++)
    { /* Scan two times for ID, since we might start receiving in the middle of a transmission */
    
        EXIT_ON_ERR(err, rfalTransceiveBlockingTx( NULL, 0, card->uid, KOVIO_UID_LENGTH, &actrxlength, (RFAL_TXRX_FLAGS_DEFAULT | RFAL_TXRX_FLAGS_CRC_RX_KEEP | RFAL_TXRX_FLAGS_PAR_RX_KEEP), rfalConvMsTo1fc(5) ));
        err = rfalTransceiveBlockingRx();  /* Get rcvd length in bits */

        if ( (ERR_TIMEOUT != err) && ((127 == actrxlength) || (255 == actrxlength)) )
        { /* Complete UID was received */
            card->length = rfalConvBitsToBytes(actrxlength);
            break;
        }
        /* If we started to receive a little after the start of transmission,
           we are now in the middle, with 2ms delay we should be in correct
           position inside the [1.2ms tx - 3.6ms sleep - 1.2ms tx - ...] grid. */
        platformDelay(1);
        /* FIXME timerDelayInMs(2) led to ~3ms on KL25 */
        /* FIXME tmerDelayinUs(2000) led to ~200us on KL25 (clocks probabyl not correctly initialized */
    }

    
    /* map incomplete byte error as no error (parity bits are also received) */
    if ( ERR_INCOMPLETE_BYTE == err )
    {
        err = ERR_NONE;
    }
    
    /* map parity error or receive timeout to not found error */
    if ((ERR_TIMEOUT == err) ||
            (ERR_NOMSG == err))
    {
        return ERR_NOTFOUND;
    }

    if ( (actrxlength != 0) && (32 != card->length) && (16 != card->length) )
    {
        return ERR_FRAMING;
    }

    kovioNormalizeUID(card);

    crc = rfalCrcCalculateCcitt(0x6363, card->uid, card->length - 2);

    if ((crc & 0xff) != card->uid[card->length - 1] || (crc>>8) != card->uid[card->length - 2]) 
      err = ERR_CRC;

     return err;
}
