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
 *  \brief Implementation of Topaz aka NFF type 1 tag
 *
 */
/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"
#include "topaz.h"
#include "utils.h"
#include "rfal_rf.h"
#include "rfal_t1t.h"

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/* TOPAZ_MASK_RECEIVE_TIME Spec: FDT = (n * 128 + 84) / fc  with n_min = 9 
   set it lower: 2*5=10 */
#define TOPAZ_MASK_RECEIVE_TIME 10 

/*  REQA, etc. have much shorter time of 1172/fc ~= 19*64/fc */
#define TOPAZ_INVENTORY_WAITING_TIME 35

/* DRD for WRITE_E is n=554 => 1109*64/fc */
#define TOPAZ_WRITE_E_WAITING_TIME 1200
/* DRD for WRITE_E is n=281 => 563*64/fc */
#define TOPAZ_WRITE_NE_WAITING_TIME 600

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
ReturnCode topazInitialize()
{
    rfalT1TPollerInitialize();
    return rfalFieldOnAndStartGT();
}

ReturnCode topazDeinitialize(uint8_t keep_on)
{
    if (!keep_on)
    {
        return rfalFieldOff();
    }

    return ERR_NONE;
}

ReturnCode topazReqaWupa(topazCommand_t cmd, topazProximityCard_t* card)
{
    uint16_t actlength;

    return rfalISO14443ATransceiveShortFrame( (rfal14443AShortFrameCmd) cmd, (uint8_t*)&card->atqa, 2*8, &actlength, TOPAZ_INVENTORY_WAITING_TIME * 64);
}


ReturnCode topazReadUID(topazProximityCard_t* card)
{
    ReturnCode    ret;
  
    card->actlength = 0;
    ret = rfalT1TPollerRid( (rfalT1TRidRes*)&card->hr );
  
    if( ret == ERR_NONE )
    {
      card->actlength = (RFAL_T1T_HR_LENGTH + RFAL_T1T_UID_LEN);
    }
    return ret;
}

ReturnCode topazReadAll(const topazProximityCard_t* card, uint8_t *buf, uint16_t buf_size, uint16_t* act_size)
{
    return rfalT1TPollerRall( (uint8_t*)card->uid, buf, buf_size, act_size );
}

ReturnCode topazWriteByte(topazProximityCard_t* card, uint8_t addr, uint8_t data)
{
    return rfalT1TPollerWrite( (uint8_t*)card->uid, addr, data );
}

