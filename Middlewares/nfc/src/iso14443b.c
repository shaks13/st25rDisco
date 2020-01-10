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
 *  \brief Implementation of ISO-14443B
 *
 */
/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"
#include "iso14443b.h"
#include "iso14443_common.h"
#include "utils.h"
#include "rfal_rf.h"
#include "rfal_nfcb.h"
#include "rfal_isoDep.h"

/*
******************************************************************************
* LOCAL DEFINES
******************************************************************************
*/

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/* iso14443 max FDT is ~5secs. However this is much too large for our USB 
   communication. Limit to 100ms = 21186 * 64/fc. */
#define ISO14443B_FRAME_DELAY_TIME  21186

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
ReturnCode iso14443BInitialize()
{
    rfalNfcbPollerInitialize();  
    return rfalFieldOnAndStartGT();
}

ReturnCode iso14443BDeinitialize(uint8_t keep_on)
{
    if (!keep_on)
    {
        return rfalFieldOff();
    }

    return ERR_NONE;
}

extern rfalIsoDepApduTxRxParam iso14443L4TxRxParams; 

ReturnCode iso14443BSelect(iso14443BCommand_t cmd,
                iso14443BProximityCard_t* card,
                uint8_t afi,
                iso14443BSlotCount_t slotCount)
{
    ReturnCode err = ERR_NONE;
    rfalNfcbListenDevice nfcbDev;
    uint8_t              devCnt;
    uint8_t              rcvdLen;
    
    card->collision = false;

    /* Configure AFI on the NFCB module */
    rfalNfcbPollerInitializeWithParams( afi, 0x00 ); 
    
    /* Send REQB or WUPB */
    err = rfalNfcbPollerCheckPresence( ((cmd == ISO14443B_CMD_WUPB) ? RFAL_NFCB_SENS_CMD_ALLB_REQ : RFAL_NFCB_SENS_CMD_SENSB_REQ),
                                       RFAL_NFCB_SLOT_NUM_1, (rfalNfcbSensbRes*)card, &rcvdLen );
    
    if( (err == ERR_NONE) && (rcvdLen == 0) )   /* Devices detected with collision/transmission error */
    {
        card->collision = true;
        
        /* Use Compliance to NFC Forum 1.1 so that WUPB is always performed before */
        err = rfalNfcbPollerCollisionResolution( RFAL_COMPLIANCE_MODE_NFC, 1, &nfcbDev, &devCnt );
        
        if( err == ERR_NONE )
        {
            if(devCnt > 0)
            {
                ST_MEMCPY( card, (uint8_t*)&nfcbDev.sensbRes, nfcbDev.sensbResLen );
                err = ERR_NONE;
            }
            else
            {
                err = ERR_NOTFOUND;
            }
        }
    }
    
    
    if(ERR_TIMEOUT == err) 
    {
        /* Remap Timeout to Not Found */
        err = ERR_NOTFOUND; 
    }
    else if(ERR_NONE == err)
    {
        iso14443L4TxRxParams.FWT = rfalIsoDepFWI2FWT(card->protocolInfo[2]>>4);   /* FWT to be used (ignored in Listen Mode)  */
        iso14443L4TxRxParams.dFWT = 50000;                                        /* Delta FWT to be used                     */
        iso14443L4TxRxParams.FSx = rfalIsoDepFSxI2FSx(card->protocolInfo[1]>>4);  /* Other device Frame Size (FSD or FSC)     */
    }

    return err;
}

ReturnCode iso14443BSendHltb(iso14443BProximityCard_t* card)
{   
    return rfalNfcbPollerSleep( card->pupi );
}

ReturnCode iso14443BEnterProtocolMode(iso14443BProximityCard_t* card,
                                iso14443BAttribParameter_t* param,
                                iso14443BAttribAnswer_t* answer)
{
    ReturnCode err;
    uint8_t actlength;
    rfalIsoDepAttribRes attribRes;

    rfalIsoDepInitialize();
    
    iso14443L4TxRxParams.DID = param->param4 & 0xf;
    iso14443L4TxRxParams.ourFSx = rfalIsoDepFSxI2FSx(param->param2 & 0xf);

    err = rfalIsoDepATTRIB( (uint8_t*)&card->pupi,                     // PUPI
                              param->param1,                           // PARAM1
                              (rfalBitRate)((param->param2>>6)&0x03),  // DSI
                              (rfalBitRate)((param->param2>>4)&0x03),  // DRI
                              (rfalIsoDepFSxI)(param->param2&0x0F),    // FSDI
                              param->param3,                           // PARAM3
                              param->param4,                           // PARAM4
                              NULL,                                    // HLInf
                              0,                                       // HLInf length
                              rfalConv64fcTo1fc(ISO14443B_FRAME_DELAY_TIME), 
                              &attribRes,
                              &actlength );
    
    if(err == ERR_NONE)
    {
        answer->mbli = (attribRes.mbliDid >> 4) & 0xf;
        answer->cid = attribRes.mbliDid & 0xf;
    }

    return err;
}

