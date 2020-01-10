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
 *  \brief Common functions for 14443A and 14443B
 *
 */
/*!
 * 
 */

#ifndef ISO_14443_COMMON_H
#define ISO_14443_COMMON_H

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Transmit an ISO14443 frame and get response
 *
 *  This function is used to transfer a payload (usually an APDU according to 
 *  ISO7816-4).
 *  It handles all of chaining, waiting time extension and retransmission. It 
 *  takes the parameters from last ATQB/ATTRIB/RATS/ATS.
 *
 *  Normally, a PICC should answer. This answer is then written to \a rxbuf
 *  and should have a max. length of \a rxlen (length of \a rxbuf).
 *  In case more data is received this data is discarded then.
 *
 *  \param[in] txbuf: data to be transmitted.
 *  \param[in] txlen: Number of bytes to transmit.
 *  \param[in] rxbuf: Buffer where the result will be written to.
 *  \param[in] rxlen: Max. number of bytes to receive (= length of \a rxbuf)
 *  \param[out] actrxlength: actual receive length.
 *
 *  \return ERR_NOTFOUND : No answer from PICC
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443TransmitAndReceiveL4(const uint8_t* txbuf,
                                    uint16_t txlen,
                                    uint8_t* rxbuf,
                                    uint16_t rxlen,
                                    uint16_t* actrxlength);
/*! 
 *****************************************************************************
 *  \brief  Transmit an ISO14443 frame and get response
 *
 *  This function is used to transfer an ISO14443 compatible frame.
 *  It takes the data given by \a txbuf with \a txlen and calls
 *  #rfalStartTransceive to perform its action.
 *  Normally, a PICC should answer. This answer is then written to \a rxbuf
 *  and should have a max. length of \a rxlen (length of \a rxbuf).
 *  In case more data is received this data is discarded then.
 *
 *  \param[in] txbuf: data to be transmitted.
 *  \param[in] txlen: Number of bytes to transmit.
 *  \param[in] rxbuf: Buffer where the result will be written to.
 *  \param[in] rxlen: Max. number of bytes to receive (= length of \a rxbuf)
 *  \param[out] actrxlength: actual receive length.
 *  \param[in] fwt_64fcs: waiting time for the response in the unit of 64/fc.
 *
 *  \return ERR_NOTFOUND : No answer from PICC
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443TransmitAndReceive(const uint8_t* txbuf,
                                    uint16_t txlen,
                                    uint8_t* rxbuf,
                                    uint16_t rxlen,
                                    uint16_t* actrxlength,
                                    uint32_t fwt_64fcs);

/*! 
 *****************************************************************************
 *  \brief  Deselect a PICC
 *
 *  This function deselects a PICC which is in PROTOCOL state.
 *
 *  \return ERR_NOTFOUND : PICC not in field or in right state.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso14443Deselect( void );

#endif /* ISO_14443_COMMON_H */

