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
 *  \brief Mifare UL read and write command implementation
 *
 */
/*!
 * 
 */

#ifndef MIFARE_UL_H
#define MIFARE_UL_H

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
 *  \brief  Read out a given number of bytes from a MIFARE UL PICC.
 *
 *  This function reads out a given number of bytes from a MIFARE UL
 *  compatible PICC in field.
 *  \note Due to the nature of the MIFARE UL read-command normally 16bytes are
 *  read out. This function however only returns the number of bytes given.
 *  \note Mifare UL only has 16 pages. If a roll over occurs function
 *  continues at address 0x0 as MIFARE UL specifies.
 *  \note PICC must be in ACTIVE state using #iso14443ASelect
 *
 *  \param[in] startAddr: Address of the first page to read out.
 *  \param[out] readbuf: Buffer with size \a length where the result is stored.
 *  \param[in] length: Number of bytes to read out (size of \a readbuf)
 *  \param[in] actLength: Number of bytes actually read.
 *
 *  \return ERR_NOTFOUND : No answer from PICC.
 *  \return ERR_NOMSG : NAK received.
 *  \return ERR_PARAM : \a startAddr behind 0xf.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode mifareUlReadNBytes(uint8_t startAddr, uint8_t* readbuf, uint8_t length, uint8_t* actLength);

/*! 
 *****************************************************************************
 *  \brief  Write a page of a MIFARE UL PICC.
 *
 *  This function writes one page data to a MIFARE UL compatible PICC in field.
 *  \note PICC must be in ACTIVE state using #iso14443ASelect
 *
 *  \param[in] pageAddr: Address of page write.
 *  \param[in] writebuf: one page (4 bytes!) longe buffer to write to the
 *                      given page.
 *
 *  \return ERR_NOTFOUND : No answer from PICC.
 *  \return ERR_NOMSG : NAK received.
 *  \return ERR_PARAM : \a startAddr behind 0xf.
 *  \return ERR_WRITE : Write failed.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode mifareUlWritePage(uint8_t pageAddr, const uint8_t* writebuf);

#endif /* MIFARE_UL_H */

