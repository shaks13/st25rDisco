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
 *  \brief Implementation of Kovio barcode tag
 *
 */
/*!
 * 
 */

#ifndef KOVIO_H
#define KOVIO_H

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
#define KOVIO_UID_LENGTH 32

/*
******************************************************************************
* GLOBAL DATATYPES
******************************************************************************
*/
/*!< 
 * struct representing an KOVIO PICC as returned by
 * #kovioRead.
 */
typedef struct
{
    uint8_t length; /*<! either 16 or 32 bytes UID */
    uint8_t uid[KOVIO_UID_LENGTH]; /*<! UID of the PICC */
}kovioProximityCard_t;

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Initialize Kovio barcode mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode kovioInitialize(void);

/*! 
 *****************************************************************************
 *  \brief  Deinitialize Kovio barcode mode.
 *  \note This function should be called every time iso 14443 a is not needed
 *  any more.
 *  \param keep_on: if true the RF field will not be switched off
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode kovioDeinitialize(uint8_t keep_on);

/*! 
 *****************************************************************************
 *  \brief  Receive the 128 bit code of a Kovio tag
 *
 *  This function is used listen if a Kovio barcode tag is out there transmitting
 *  its ID
 *
 *  \return ERR_NOTFOUND : no kovio barcode tag was found
 *  \return ERR_TIMEOUT : Timeout waiting for interrupts.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode kovioRead(kovioProximityCard_t *card);

#endif /* KOVIO_H */
