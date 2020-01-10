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
 *  \brief Implementation of ISO-14443A
 *
 */
/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"
#include "iso14443a.h"
#include "iso14443_common.h"
#include "utils.h"
#include "rfal_rf.h"
#include "rfal_nfca.h"
#include "rfal_isoDep.h"

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/*  REQA, etc. have much shorter time of 1172/fc ~= 19*64/fc */
#define ISO14443A_INVENTORY_WAITING_TIME 35

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/
ReturnCode iso14443AInitialize()
{
    rfalNfcaPollerInitialize();  
    return rfalFieldOnAndStartGT();
}

ReturnCode iso14443ADeinitialize(uint8_t keep_on)
{
    if (!keep_on)
    {
        return rfalFieldOff();
    }

    return ERR_NONE;
}

ReturnCode iso14443ASelect(iso14443ACommand_t cmd, iso14443AProximityCard_t* card, uint8_t perform_anticollision)
{
    ReturnCode err;
    uint16_t actlength;

    card->actlength = 0;

    err = rfalISO14443ATransceiveShortFrame( (rfal14443AShortFrameCmd) cmd, (uint8_t*)&card->atqa, 2*8, &actlength, ISO14443A_INVENTORY_WAITING_TIME * 64);

    if (ERR_NONE==err && 0x00 == (card->atqa[0] & 0x1f)) /* Select/anticollision not supported */
    {
        return ERR_NOTSUPP; /* Select/anticollision not supported */
    }
    if (ERR_TIMEOUT == err)
    {
        return err;
    }

    if (!perform_anticollision)
    {
        return err;
    }

    /* at least one tag responded - start anticollision loop regardless
       on if there was a collision within ATQA or not */
    err = rfalNfcaPollerSingleCollisionResolution( 1, &card->collision, (rfalNfcaSelRes*) &card->sak[0], (uint8_t*)card->uid, &card->actlength   );
    
    if(card->actlength == RFAL_NFCA_CASCADE_1_UID_LEN)
        card->cascadeLevels = 1;
    else if(card->actlength == RFAL_NFCA_CASCADE_2_UID_LEN)
        card->cascadeLevels = 2;
    else if(card->actlength == RFAL_NFCA_CASCADE_3_UID_LEN)
        card->cascadeLevels = 3;
    else
        return ERR_PROTO;
    

    if (ERR_TIMEOUT == err)
    {
        err = ERR_NOTSUPP; /* Select/anticollision not supported */
    }

    return err;
}


ReturnCode iso14443ASendHlta()
{
    return rfalNfcaPollerSleep();
}

extern rfalIsoDepApduTxRxParam iso14443L4TxRxParams; 

ReturnCode iso14443AEnterProtocolMode(uint8_t fscid, uint8_t* answer, uint16_t maxlength, uint16_t* length)
{
    ReturnCode err;
    uint8_t blen = maxlength;

    rfalIsoDepInitialize();
    
    /* send RATS command */
    *length = maxlength;
    err = rfalIsoDepRATS( (rfalIsoDepFSxI) (fscid>>4), fscid & 0xf, (rfalIsoDepAts *)answer , &blen);
    *length = blen;


    /* map timeout error to PICC not found */
    if (ERR_TIMEOUT == err)
    {
        err = ERR_NOTFOUND;
    }

    iso14443L4TxRxParams.FWT = rfalIsoDepFWI2FWT((((rfalIsoDepAts *)answer)->TB)>>4);   /*!< FWT to be used (ignored in Listen Mode)  */
    iso14443L4TxRxParams.dFWT = 50000;             /*!< Delta FWT to be used                     */
    iso14443L4TxRxParams.FSx = rfalIsoDepFSxI2FSx( (((rfalIsoDepAts *)answer)->T0) & 0xf); /*!< Other device Frame Size (FSD or FSC)     */
    iso14443L4TxRxParams.ourFSx = rfalIsoDepFSxI2FSx( (fscid>>4));/*!< Our device Frame Size (FSD or FSC)       */
    iso14443L4TxRxParams.DID = fscid & 0xf;/*!< Device ID (RFAL_ISODEP_NO_DID if no DID) */

    return err;
}

ReturnCode iso14443ASendProtocolAndParameterSelection(uint8_t cid, uint8_t pps1)
{
    ReturnCode err;
    rfalIsoDepPpsRes ppsRes;

    err = rfalIsoDepPPS( cid, (rfalBitRate)((pps1 & 0x0c)>>2) , (rfalBitRate)(pps1 & 0x03), &ppsRes );


    /* map timeout error to PICC not found */
    if ((ERR_TIMEOUT == err) || ((ISO14443A_CMD_PPSS | cid) != ppsRes.PPSS))
    {
        err = ERR_NOTFOUND;
    }

    return err;
}

