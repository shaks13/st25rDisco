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
 *  \brief Low level implementation of FeliCa
 *
 */
/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "felica.h"
#include "platform.h"
#include "st_errno.h"
#include "utils.h"
#include "rfal_rf.h"
#include "rfal_nfcf.h"

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/* FELICA_MASK_RECEIVE_TIME spec: ~42*64/fc = 197us, set it to half: */
#define FELICA_MASK_RECEIVE_TIME 21

/*  FeliCa Response Time A is 2.417 ms ~512*64/fc, higher than can be set in ST25R3911 -> max */
#define FELICA_POLL_WAIT_TIME 255

/*  FeliCa Response Time A is 1.208 ms ~256*64/fc */
#define FELICA_POLL_SLOT_TIME 256

/* FeliCa max FWT is almost unlimited. However this is much too large for our USB 
   communication. Limit to 10ms = 21186 * 64/fc. */
#define FELICA_FRAME_WAIT_TIME  21186

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/
ReturnCode felicaInitialize()
{
    rfalNfcfPollerInitialize( RFAL_BR_212 );
    return rfalFieldOnAndStartGT();
}

ReturnCode felicaDeinitialize(uint8_t keep_on)
{
    if (!keep_on)
    {
        return rfalFieldOff();
    }

    return ERR_NONE;
}


ReturnCode felicaTxRxNBytes(const uint8_t *txbuf, uint16_t sizeof_txbuf, uint8_t *rxbuf, uint16_t sizeof_rxbuf, uint16_t *actrxlength)
{
    return rfalTransceiveBlockingTxRx( (uint8_t*)txbuf, sizeof_txbuf, rxbuf, sizeof_rxbuf, actrxlength, RFAL_TXRX_FLAGS_DEFAULT, rfalConv64fcTo1fc(FELICA_FRAME_WAIT_TIME) );
}


ReturnCode felicaPoll(enum felicaSlots slots, uint8_t sysCode1, uint8_t sysCode2, enum felicaComParamRequest compar, 
                      struct felicaProximityCard * card, uint8_t *num_cards, uint8_t *num_cols)
{
    uint8_t crds = *num_cards;
  
    return rfalFeliCaPoll( (rfalFeliCaPollSlots) slots,
                            (sysCode1<<8) | (sysCode2),
                            compar,
                            (rfalFeliCaPollRes*) card,
                            crds,
                            num_cards,
                            num_cols );

}
