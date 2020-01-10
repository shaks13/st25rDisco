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
/*!
 * 
 */

#ifndef ISO_14443_A_H
#define ISO_14443_A_H

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
#define ISO14443A_MAX_UID_LENGTH 10
#define ISO14443A_MAX_CASCADE_LEVELS 3
#define ISO14443A_CASCADE_LENGTH 7
#define ISO14443A_RESPONSE_CT  0x88

/*
******************************************************************************
* GLOBAL DATATYPES
******************************************************************************
*/
/*!< 
 * struct representing an ISO14443A PICC as returned by
 * #iso14443ASelect.
 */
typedef struct
{
    uint8_t uid[ISO14443A_MAX_UID_LENGTH]; /*<! UID of the PICC */
    uint8_t actlength; /*!< actual UID length */
    uint8_t atqa[2]; /*!< content of answer to request byte */
    uint8_t sak[ISO14443A_MAX_CASCADE_LEVELS]; /*!< SAK bytes */
    uint8_t cascadeLevels; /*!< number of cascading levels */
    bool    collision; /*!< true, if there was a collision which has been resolved,
                        otherwise no collision occured */
}iso14443AProximityCard_t;

/*! 
 * PCD command set.
 */
typedef enum
{
    ISO14443A_CMD_REQA = 0x26,  /*!< command REQA */
    ISO14443A_CMD_WUPA = 0x52, /*!< command WUPA */
    ISO14443A_CMD_SELECT_CL1 = 0x93, /*!< command SELECT cascade level 1 */
    ISO14443A_CMD_SELECT_CL2 = 0x95, /*!< command SELECT cascade level 2 */
    ISO14443A_CMD_SELECT_CL3 = 0x97, /*!< command SELECT cascade level 3 */
    ISO14443A_CMD_HLTA = 0x50, /*!< command HLTA */
    ISO14443A_CMD_PPSS = 0xd0, /*!< command PPSS */
    ISO14443A_CMD_RATS = 0xe0, /*!< command RATS */
}iso14443ACommand_t;

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Initialize ISO14443-A mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443AInitialize(void);

/*! 
 *****************************************************************************
 *  \brief  Deinitialize ISO14443-A mode.
 *  \note This function should be called every time iso 14443 a is not needed
 *  any more.
 *  \param keep_on: if true the RF field will not be switched off
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443ADeinitialize(uint8_t keep_on);

/*! 
 *****************************************************************************
 *  \brief  Select a PICC and put it into ACTIVE state
 *
 *  This function sends to all PICCs in field either the REQA or the
 *  WUPA (depending on \a cmd parameter). This command puts the
 *  PICCs into READY state. After that anticollision loop is performed to
 *  select a unique PICC. In the end this PICC should be in ACTIVE
 *  state and its UID is returned.
 *
 *  \param[in] cmd : Used command to put the PICCs in READY state. This
 *                   could either be #ISO14443A_CMD_REQA or #ISO14443A_CMD_WUPA
 *  \param[out] card : Parameter of type #iso14443AProximityCard_t which holds
 *                the found card then.
 *  \param[in] perform_anticollision : if false then stop after ATQA.
 *
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts or no reply from cards.
 *  \return ERR_PARAM : Parameter \a cmd not available/unvalid.
 *  \return ERR_NOTFOUND : No PICC could be selected..
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443ASelect(iso14443ACommand_t cmd, iso14443AProximityCard_t* card, uint8_t perform_anticollision);

/*! 
 *****************************************************************************
 *  \brief  Send the HLTA command
 *
 *  This function is used to send the HLTA command to put a PICCs to
 *  state HALT so that they do not reply to REQA commands (only to WUPA).
 *
 *  \return ERR_NOMSG : PICC send not acknowledge
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443ASendHlta(void);

/*! 
 *****************************************************************************
 *  \brief  Enter protocol mode
 *
 *  This function is used to send the RATS command which puts the PICC
 *  into protocol mode. The PICC must be in ACTIVE mode using #iso14443ASelect
 *  command. If the PICC receives the RATS command it backscatters the ATS
 *  which will be stored to \a answer.
 *
 *  \param[in] fscid : the parameter byte send with RATS containing 
 *           frame size and card id. Refer to ISO14443-4 for exact coding.
 *  \param[out] answer : buffer where ATS will be stored
 *  \param[in] maxlength : max. length of the answer (= size of \a answer)
 *  \param[out] length : actual length of ATS
 *
 *  \return ERR_NOTFOUND : PICC not in field or in right state.
 *  \return ERR_TIMEOUT : Timeout during transmit.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error, ATS received and written to \a answer.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443AEnterProtocolMode(uint8_t fscid, uint8_t* answer, uint16_t maxlength, uint16_t* length);

/*! 
 *****************************************************************************
 *  \brief  Send protocol and parameter selection request
 *
 *  After a PICC has been put to protocol mode using #iso14443AEnterProtocolMode
 *  some parameter can be set. This function is used to set these parameters.
 *  For more information on codeing of the \a pps1 byte refer to ISO14443-4.
 *  PPS0 byte is set to 0x11 implicitely.
 *  In case of success the PICC sends back the PPS start byte.
 *
 *  \param[in] cid : logical number of the addressed PICC (see ISO14443-4)
 *  \param[in] pps1 : pps1 byte to be written (see ISO14443-4)
 *
 *  \return ERR_NOTFOUND : PICC not in field or in right state.
 *  \return ERR_TIMEOUT : Timeout during transmit.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443ASendProtocolAndParameterSelection(uint8_t cid, uint8_t pps1);

#endif /* ISO_14443_A_H */

