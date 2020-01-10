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

#ifndef ISO_14443_B_H
#define ISO_14443_B_H

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"

/*
******************************************************************************
* GLOBAL DEFINES
******************************************************************************
*/
#define ISO14443B_PUPI_LENGTH 4
#define ISO14443B_APPDATA_LENGTH 4
#define ISO14443B_PROTINFO_LENGTH 3
/*
******************************************************************************
* GLOBAL DATATYPES
******************************************************************************
*/
/*! 
 * PCD command set.
 */
typedef enum
{
    ISO14443B_CMD_REQB = 0x00,  /*!< command REQB */
    ISO14443B_CMD_WUPB = 0x08, /*!< command WUPB */
    ISO14443B_CMD_HLTB = 0x50, /*!< command HLTB */
    ISO14443B_CMD_ATTRIB = 0x1D, /*!< command ATTRIB */
}iso14443BCommand_t;

/*! 
 * slot count (N parameter) used for iso14443b anti collision
 */
typedef enum
{
    ISO14443B_SLOT_COUNT_1 = 0,
    ISO14443B_SLOT_COUNT_2 = 1,
    ISO14443B_SLOT_COUNT_4 = 2,
    ISO14443B_SLOT_COUNT_8 = 3,
    ISO14443B_SLOT_COUNT_16 = 4,
}iso14443BSlotCount_t;

/*!  
 * struct representing the content of ATQB
 * See ISO14443b spec. for more information.
 */
typedef struct
{
    uint8_t atqb; /*<! content of answer to request byte */
    uint8_t pupi[ISO14443B_PUPI_LENGTH]; /*!< UID of the PICC */
    uint8_t applicationData[ISO14443B_APPDATA_LENGTH]; /*!< application specific data */
    uint8_t protocolInfo[ISO14443B_PROTINFO_LENGTH]; /*!< protocol info */
    bool    collision; /*!< true, if there was a collision which has been resolved,
                        otherwise no collision occured */
}iso14443BProximityCard_t;

/*! 
 * struct holding parameter needed for ATTRIB command
 * The parameters are called param1, param2, param3 and
 * param4 and are described in ISO14443-3 spec.
 */
typedef struct
{
    uint8_t param1;
    uint8_t param2;
    uint8_t param3;
    uint8_t param4;
}iso14443BAttribParameter_t;

/*! 
 * struct holding the answer to ATTRIB command
 */
typedef struct
{
    uint8_t mbli; /*!< maximum buffer length */
    uint8_t cid;  /*!< logical card identifier */
}iso14443BAttribAnswer_t;

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Initialize ISO14443-B mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode to initialize ISO14443-B configuration.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443BInitialize(void);

/*! 
 *****************************************************************************
 *  \brief  Deinitialize ISO14443-B mode.
 *  \note This function should be called every time iso 14443b is not needed
 *  any more.
 *  \param keep_on: if true the RF field will not be switched off
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443BDeinitialize(uint8_t keep_on);

/*! 
 *****************************************************************************
 *  \brief  Select a PICC and put it into READY_DECLARED state
 *
 *  This function sends to all PICCs in field either the REQB or the
 *  WUPB (depending on \a cmd parameter). Depending on \a slotCount
 *  one or more PICC with matched \a afi send back their PUPI and
 *  change their state to READY_DECLARED. All other PICCs change to READY_REQUESTED.
 *  If more PICCs send their PUPI a collision occurs and the anti-collision
 *  algorithm is started until only one PICC is in READY_DECLARED state.
 *  The PUPI of this PICC is then stored in \a card.
 *
 *  \param[in] cmd : Used command to put the PICCs in READY state. This
 *                   could either be #ISO14443B_CMD_REQB or #ISO14443B_CMD_WUPB
 *  \param[out] card : Parameter of type #iso14443BProximityCard_t which holds
 *                the found card then.
 *  \param[in] afi : application family identifier (Refer to ISO14443-3)
 *  \param[in] slotCount : Parameter used for REQB/WUPB. Refer to ISO14443-3
 *                         for more information.
 *
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts or no reply from cards.
 *  \return ERR_NOTFOUND : No PICC could be selected..
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443BSelect(iso14443BCommand_t cmd,
                        iso14443BProximityCard_t* card,
                        uint8_t afi,
                        iso14443BSlotCount_t slotCount);

/*! 
 *****************************************************************************
 *  \brief  Send the HLTB command
 *
 *  This function is used to send the HLTB command to put one or more PICCs to
 *  state HALT so that they do not reply to further REQB commands (only to WUPB).
 *  \note PICC must be in state READY_DECLARED state using #iso14443BSelect
 *
 *  \param[in] card : Card to which the HLTB command shall be sent to
 *  \return ERR_NOMSG : PICC send not acknowledge
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443BSendHltb(iso14443BProximityCard_t* card);

/*! 
 *****************************************************************************
 *  \brief  Enter protocol mode
 *
 *  This function is used to send the ATTRIB command which puts the PICC
 *  into protocol mode. The PICC must be in READY_DECLARED state using #iso14443BSelect
 *  command.
 *
 *  \param[in] card : parameter of type #iso14443BProximityCard_t representing
 *          the PICC to be put into protocol state. (\a pupi member)
 *  \param[in] param : parameter sent with attrib command (see ISO14443-3 spec)
 *  \param[out] answer : answer to attrib command
 *
 *  \return ERR_NOTFOUND : PICC not in field or in right state.
 *  \return ERR_TIMEOUT : Timeout during transmit.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error, answer received and written to \a answer, PICC
 *                     now in protocol state.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443BEnterProtocolMode(iso14443BProximityCard_t* card,
                                iso14443BAttribParameter_t* param,
                                iso14443BAttribAnswer_t* answer);

#endif /* ISO_14443_B_H */

