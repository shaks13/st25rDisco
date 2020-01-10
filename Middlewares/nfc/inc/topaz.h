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
 *  \brief Implementation of Topaz aka NFC type 1 tag
 *
 */
/*!
 * 
 */

#ifndef TOPAZ_H
#define TOPAZ_H

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
#define TOPAZ_UID_LENGTH 4
#define TOPAZ_HR_LENGTH 2

/*
******************************************************************************
* GLOBAL DATATYPES
******************************************************************************
*/
/*!< 
 * struct representing an TOPAZ PICC as returned by
 * #topazReqaWupa().
 */
typedef struct
{
    uint8_t  hr[TOPAZ_HR_LENGTH]; /*<! UID of the PICC */
    uint8_t  uid[TOPAZ_UID_LENGTH]; /*<! UID of the PICC */
    uint16_t actlength; /*!< actual UID length */
    uint8_t  atqa[2]; /*!< content of answer to request byte */
    bool     collision; /*!< true, if there was a collision which has been resolved,
                        otherwise no collision occured */
}topazProximityCard_t;

/*! 
 * PCD command set.
 */
typedef enum
{
    TOPAZ_CMD_REQA    = 0x26, /*!< command REQA */
    TOPAZ_CMD_WUPA    = 0x52, /*!< command WUPA */
    TOPAZ_CMD_RID     = 0x78, /*!< command Read UID */
    TOPAZ_CMD_RALL    = 0x00, /*!< command Read All */
    TOPAZ_CMD_READ    = 0x01, /*!< command Read */
    TOPAZ_CMD_WRITE_E = 0x53, /*!< command Write with erase */
    TOPAZ_CMD_WRITE_NE= 0x1a, /*!< command Write, no erase */
}topazCommand_t;

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Initialize Topaz mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode topazInitialize(void);

/*! 
 *****************************************************************************
 *  \brief  Deinitialize Topaz mode.
 *  \note This function should be called every time Topaz is not needed
 *  any more.
 *  \param keep_on: if true the RF field will not be switched off
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode topazDeinitialize(uint8_t keep_on);

/*! 
 *****************************************************************************
 *  \brief  Select a PICC and put it into READY state
 *
 *  This function sends to all PICCs in field either the REQA or the
 *  WUPA (depending on \a cmd parameter). This command puts the
 *  PICCs into READY state.
 *
 *  \param[in] cmd : Used command to put the PICCs in READY state. This
 *                   could either be #TOPAZ_CMD_REQA or #TOPAZ_CMD_WUPA
 *  \param[out] card : Parameter of type #topazProximityCard_t which holds
 *                the found card then.
 *
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts or no reply from cards.
 *  \return ERR_PARAM : Parameter \a cmd not available/unvalid.
 *  \return ERR_NOTFOUND : No PICC could be selected..
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode topazReqaWupa(topazCommand_t cmd, topazProximityCard_t* card);

/*! 
 *****************************************************************************
 *  \brief  Read the UID from a tag in READY state
 *
 *
 *  \param[out] card : Parameter of type #topazProximityCard_t which will hold
 *                the UID of the read card.
 *
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts or no reply from cards.
 *  \return ERR_NOTFOUND : No PICC could be selected..
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode topazReadUID(topazProximityCard_t* card);

/*! 
 *****************************************************************************
 *  \brief  Read the memory from a tag in READY state
 *
 *
 *  \param[in] card : Parameter of type #topazProximityCard_t which holds
 *                the UID of the card to read.
 *  \param[out] buf : buffer where the complete memory should be put
 *  \param[in] buf_size : Size of the given buffer
 *  \param[out] act_size : Size actually received
 *
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts or no reply from cards.
 *  \return ERR_NOTFOUND : No PICC could be selected..
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode topazReadAll(const topazProximityCard_t* card, uint8_t *buf, uint16_t buf_size, uint16_t* act_size);

/*! 
 *****************************************************************************
 *  \brief  Write one byte of memory to a tag in READY state
 *
 *
 *  \param[in] card : Parameter of type #topazProximityCard_t which holds
 *                the UID of the card to read.
 *  \param[in] addr : Address of the byte to be written.
 *  \param[in] data : Byte to be written to the given address
 *
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts or no reply from cards.
 *  \return ERR_NOTFOUND : No PICC could be selected..
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode topazWriteByte(topazProximityCard_t* card, uint8_t addr, uint8_t data);

#endif /* TOPAZ_H */

