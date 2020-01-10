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

#ifndef FELICA_H
#define FELICA_H

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
#define FELICA_MAX_ID_LENGTH 8

/*
******************************************************************************
* GLOBAL DATATYPES
******************************************************************************
*/
/*! 
  struct representing an FELICA PICC as returned by
  #felicaPoll.
  Layout is:
  <ol>
    <li> length[1]
    <li> response_code[1];
    <li> IDm[8];
    <li> PMm[8];
    <li> request_data[2];
  </ol>
 */
struct felicaProximityCard
{
    uint8_t length[1];
    uint8_t response_code[1];
    uint8_t IDm[FELICA_MAX_ID_LENGTH]; /*<! ID of the PICC */
    uint8_t PMm[8];
    uint8_t request_data[2];
};

/*! 
 * PCD command set.
 */
enum felicaCommand
{
    FELICA_CMD_POLLING                  = 0x00, /*!< acquire and identify a card. */
    FELICA_CMD_REQUEST_SERVICE          = 0x02, /*!< verify the existence of Area and Service. */
    FELICA_CMD_REQUEST_RESPONSE         = 0x04, /*!< verify the existence of a card. */
    FELICA_CMD_READ_WITHOUT_ENCRYPTION  = 0x06, /*!< read Block Data from a Service that requires no authentication. */
    FELICA_CMD_WRITE_WITHOUT_ENCRYPTION = 0x08, /*!< write Block Data to a Service that requires no authentication. */
    FELICA_CMD_REQUEST_SYSTEM_CODE      = 0x0c, /*!< acquire the System Code registered to a card. */
    FELICA_CMD_AUTHENTICATION1          = 0x10, /*!< authenticate a card. */
    FELICA_CMD_AUTHENTICATION2          = 0x12, /*!< allow a card to authenticate a Reader/Writer. */
    FELICA_CMD_READ                     = 0x14, /*!< read Block Data from a Service that requires authentication. */
    FELICA_CMD_WRITE                    = 0x16, /*!< write Block Data to a Service that requires authentication. */
};

enum felicaSlots
{
    FELICA_1_SLOT = 0,
    FELICA_2_SLOTS = 1,
    FELICA_4_SLOTS = 3,
    FELICA_8_SLOTS = 7,
    FELICA_16_SLOTS = 15,

};

enum felicaComParamRequest
{
    FELICA_REQ_NO_REQUEST = 0x00,
    FELICA_REQ_SYSTEM_CODE = 0x01,
    FELICA_REQ_COM_PERFORMANCE = 0x02,
};

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Initialize FELICA mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode felicaInitialize(void);

/*! 
 *****************************************************************************
 *  \brief  Deinitialize FELICA mode.
 *  \note This function should be called every time iso 14443 a is not needed
 *  any more.
 *  \param keep_on: if true the RF field will not be switched off
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode felicaDeinitialize(uint8_t keep_on);

/*! 
 *****************************************************************************
 *  This function sends to all PICCs in field the POLL command with the given
 *  number of slots.
 *
 *  \param[in] slots: the number of slots to be performed
 *  \param[in] sysCode1: as given in FeliCa poll command 
 *  \param[in] sysCode2: as given in FeliCa poll command 
 *  \param[in] compar: FeliCa communication paramters
 *  \param[out] card : Parameter of type #felicaProximityCard which holds
 *                the found card then.
 *  \param[in] num_cards : size of the card array
 *  \param[out] num_cards : actual number of cards found
 *  \param[out] num_cols : number of collisions encountered
 *
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode felicaPoll(enum felicaSlots slots,
                     uint8_t sysCode1,
                     uint8_t sysCode2,
                     enum felicaComParamRequest compar,
                     struct felicaProximityCard *card,
                     uint8_t *num_cards,
                     uint8_t *num_cols
                    );

/*! 
 *****************************************************************************
 *  Transfer sizeof_txbuf bytes to a FeliCa card and receive back up to 
 *  sizeof_rxbuf bytes 
 *  \param[in] txbuf: buffer to transfer
 *  \param[in] sizeof_txbuf: number of bytes to transfer
 *  \param[out] rxbuf: buffer of read data
 *  \param[out] sizeof_rxbuf: maximum number of bytes to receive
 *  \param[out] actrxlength : the number of actually received bytes
 *****************************************************************************
 */
extern ReturnCode felicaTxRxNBytes(const uint8_t *txbuf, uint16_t sizeof_txbuf, uint8_t *rxbuf, uint16_t sizeof_rxbuf, uint16_t *actrxlength);
#endif /* FELICA_H */
