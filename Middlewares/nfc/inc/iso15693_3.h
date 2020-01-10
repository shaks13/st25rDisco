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
 *  \brief Implementation of ISO-15693-3
 *
 */

#ifndef ISO_15693_3_H
#define ISO_15693_3_H

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"
#include "rfal_iso15693_2.h"

/*
******************************************************************************
* GLOBAL DEFINES
******************************************************************************
*/
#define ISO15693_UID_LENGTH 8
#define ISO15693_MAX_MEMORY_BLOCK_SIZE 32

#define ISO15693_CMD_INVENTORY 1
#define ISO15693_CMD_STAY_QUIET 2
#define ISO15693_CMD_READ_SINGLE_BLOCK 0x20
#define ISO15693_CMD_WRITE_SINGLE_BLOCK 0x21
#define ISO15693_CMD_READ_MULTIPLE_BLOCKS 0x23
#define ISO15693_CMD_SELECT 0x25
#define ISO15693_CMD_GET_SYSTEM_INFORMATION 0x2B

#define ISO15693_M24LR_IC_MFG_CODE          0x02   /* M24LR */
#define ISO15693_CMD_FAST_READ_SINGLE_BLOCK 0xC0   /* M24LR */
#define ISO15693_CMD_FAST_READ_MULTI_BLOCK  0xC3   /* M24LR */

#define ISO15693_REQ_FLAG_INVENTORY       0x04
#define ISO15693_REQ_FLAG_PROT_EXTENSION  0x08
/* with INVENTORY flag set we have these bits : */
#define ISO15693_REQ_FLAG_SELECT          0x10
#define ISO15693_REQ_FLAG_ADDRESS         0x20
/* with INVENTORY flag not set we have these bits : */
#define ISO15693_REQ_FLAG_AFI             0x10
#define ISO15693_REQ_FLAG_1_SLOT          0x20
/*! OPTION flag is the same for both: */
#define ISO15693_REQ_FLAG_OPTION          0x40

#define ISO15693_RESP_FLAG_ERROR          0x01
#define ISO15693_RESP_FLAG_PROT_EXTENSION 0x08
/*
******************************************************************************
* GLOBAL DATATYPES
******************************************************************************
*/
/*! 
 * structure representing an ISO15693 PICC
 */
typedef struct
{
    uint8_t flags; /*!< flag byte of response */
    uint8_t dsfid; /*!< Data Storage Format Identifier */
    uint8_t uid[ISO15693_UID_LENGTH]; /*!< UID of the PICC */
    uint16_t crc; /*!< CRC of response */
}iso15693ProximityCard_t;

/*! 
 * structure holding the information returned by #iso15693GetPiccSystemInformation
 */
typedef struct
{
    uint8_t flags; /*!< flag byte of response */
    uint8_t infoFlags; /*!< info flags */
    uint8_t uid[ISO15693_UID_LENGTH]; /*!< UID of the PICC */
    uint8_t dsfid;  /*!< Data Storage Format Identifier */
    uint8_t afi; /*!< Application Family Identifier */
    uint8_t memNumBlocks; /*!< number of blocks available */
    uint8_t memBlockSize; /*!< number of bytes per block */
    uint8_t icReference; /*!< IC reference field */
}iso15693PiccSystemInformation_t;

/*! 
 * structure representing a memory block
 * of an ISO15693 PICC
 */
typedef struct
{
    uint8_t flags;
    uint8_t errorCode;
    uint8_t securityStatus; /*< security status byte */
    uint8_t blocknr; 
    uint8_t data[ISO15693_MAX_MEMORY_BLOCK_SIZE]; /*!< the content */
    uint8_t actualSize; /*!< actual size of \a data */
}iso15693PiccMemoryBlock_t;

/*! 
 * enum holding possible slot count values used by inventory command.
 */
typedef enum
{
    ISO15693_NUM_SLOTS_1, /*!< 1 slot */
    ISO15693_NUM_SLOTS_16 /*!< 16 slots */
}iso15693NumSlots_t;

/*
******************************************************************************
* GLOBAL FUNCTION PROTOTYPES
******************************************************************************
*/
/*! 
 *****************************************************************************
 *  \brief  Initialize ISO15693 mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode.
 *
 *  \return GP_ERR_IO : Error during communication.
 *  \return GP_ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693Initialize( bool useSlowTX, bool useFastRX );

/*! 
 *****************************************************************************
 *  \brief  Deinitialize ISO15693 mode.
 *  \note This function should be called every time iso 15693 is not needed
 *  any more.
 *  \param keep_on: if true the RF field will not be switched off
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693Deinitialize(uint8_t keep_on);

/*! 
 *****************************************************************************
 *  \brief  Perform an ISO15693 inventory to return all PICCs in field.
 *
 *  This function performs an ISO15693 inventory which is used to return
 *  the UIDs of all PICCs in field.
 *  If \a maskLength is 0 all PICCs are addressed otherwise only
 *  PICCs with matched \a mask are returned. Using \a slotcnt the
 *  number of slots (1 or 16) can be chosen.
 *
 *  \param[in] slotcnt : Slotcount used (16 or 1)
 *  \param[in] maskLength : length of the mask if available (0 - 63)
 *  \param[in] mask : mask to use if \a maskLength is set, otherwise NULL
 *  \param[out] cards : buffer array where found card information is stored.
 *  \param[in] maxCards : maximum number of cards to return (= size of \a cards)
 *  \param[out] cardsFound : number of cards found and returned.
 *
 *  \return ERR_COLLISION : Collision which couldn't be resolved.
 *  \return ERR_NOTFOUND : No PICC could be selected.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693Inventory(iso15693NumSlots_t slotcnt,
                    uint8_t maskLength,
                    uint8_t* mask,
                    iso15693ProximityCard_t* cards,
                    uint8_t maxCards,
                    uint8_t* cardsFound);

/*! 
 *****************************************************************************
 *  \brief  Send command 'stay quiet' to given PICC.
 *
 *  Using this function the given \a card is send into quiet state so it does
 *  not respond to any inventory requests.
 *
 *  \param[in] card : PICC to be sent into quiet state.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693SendStayQuiet(const iso15693ProximityCard_t* card);

/*! 
 *****************************************************************************
 *  \brief  Send command 'Select' to select a PICC for non-addressed mode.
 *
 *  Using this function the given \a card is selected for non-addressed mode.
 *
 *  \param[in] card : PICC to select.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC could not be selected. (Not in field)
 *  \return ERR_CRC : CRC error.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, PICC selected.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693SelectPicc(const iso15693ProximityCard_t* card);

/*! 
 *****************************************************************************
 *  \brief  Send command 'get system information' to retrieve information
 *  from a given or selected PICC.
 *
 *  Using this function the system information value from a given or
 *  selected PICC is retrieved. If \a card is NULL then the PICC
 *  needs to be selected using #iso15693SelectPicc prior to calling
 *  this function. If \a card is not NULL then this value is treated
 *  as the PICCs UID.
 *
 *  \param[in] card : PICC to retrieve the information from.
 *                    If card is NULL then this parameter
 *                    is ignored and the information is fetched from the PICC
 *                    priorly selected with #iso15693SelectPicc
 *  \param[out] sysInfo : buffer of type #iso15693PiccSystemInformation_t
 *                      where the answer will be stored.
 *  \param[out] sysInfoLen : actual length of the sysInfo field returned by the card.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, respone written to \a sysInfo.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693GetPiccSystemInformation(const iso15693ProximityCard_t* card,
                                iso15693PiccSystemInformation_t* sysInfo, uint16_t *sysInfoLen);

/*! 
 *****************************************************************************
 *  \brief  Read a single block from a given or selected PICC.
 *
 *  This function reads out a single block from a given or selected PICC.
 *  If \a card is NULL then the PICC
 *  needs to be selected using #iso15693SelectPicc prior to calling
 *  this function. If \a card is not NULL then this value is treated
 *  as the PICCs UID.
 *
 *  \param[in] card : PICC to read the block from.
 *                    If \a card is NULL then this parameter
 *                    is ignored and the information is fetched from the PICC
 *                    priorly selected with #iso15693SelectPicc
 *  \param[out] memBlock : buffer of type #iso15693PiccMemoryBlock_t
 *                      where the block and its size will be stored.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, block read out and written to \a memBlock.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693ReadSingleBlock(const iso15693ProximityCard_t* card,
                                iso15693PiccMemoryBlock_t* memBlock);

/*!
 *****************************************************************************
 *  \brief  Fast read a single block from a given or selected PICC.
 *
 *  This function uses ST's Fast mode to read out a single block
 * from a given or selected PICC.
 *  If \a card is NULL then the PICC
 *  needs to be selected using #iso15693SelectPicc prior to calling
 *  this function. If \a card is not NULL then this value is treated
 *  as the PICCs UID.
 *
 *  \param[in] card : PICC to read the block from.
 *                    If \a card is NULL then this parameter
 *                    is ignored and the information is fetched from the PICC
 *                    priorly selected with #iso15693SelectPicc
 *  \param[out] memBlock : buffer of type #iso15693PiccMemoryBlock_t
 *                      where the block and its size will be stored.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, block read out and written to \a memBlock.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693FastReadSingleBlock(const iso15693ProximityCard_t* card,
                    iso15693PiccMemoryBlock_t* memBlock);
/*!
 *****************************************************************************
 *  \brief  Fast read multiple blocks from a given or selected PICC.
 *
 *  This function uses ST's Fast mode to read out \a numBlocks blocks from
 *  a given or selected PICC
 *  starting at block \a startblock. If \a card is NULL then the PICC
 *  needs to be selected using #iso15693SelectPicc prior to calling
 *  this function. If \a card is not NULL then this value is treated
 *  as the PICCs UID.
 *
 *  \param[in] card : PICC to read the block from.
 *                    If \a card is NULL then this parameter
 *                    is ignored and the information is fetched from the PICC
 *                    priorly selected with #iso15693SelectPicc
 *  \param[in] startblock : number of the first block to read out
 *  \param[in] numBlocks : number of blocks to read out.
 *  \param[out] res_flags : the response flags sent by the VICC
 *  \param[out] data : the buffer to store the data received
 *  \param[in] dataLen : length of the buffer
 *  \param[out] actLen : the lenght of the data received by from the VICC
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, block read out and written to \a memBlock.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693FastReadMultipleBlocks(const iso15693ProximityCard_t* card, uint8_t startblock, uint8_t numBlocks,
                                        uint8_t* res_flags, uint8_t* data, uint16_t dataLen, uint16_t *actLen );

/*! 
 *****************************************************************************
 *  \brief  Read multiple blocks from a given or selected PICC.
 *
 *  This function reads out \a numBlocks blocks from a given or selected PICC
 *  starting at block \a startblock. If \a card is NULL then the PICC
 *  needs to be selected using #iso15693SelectPicc prior to calling
 *  this function. If \a card is not NULL then this value is treated
 *  as the PICCs UID.
 *
 *  \param[in] card : PICC to read the block from.
 *                    If \a card is NULL then this parameter
 *                    is ignored and the information is fetched from the PICC
 *                    priorly selected with #iso15693SelectPicc
 *  \param[in] startblock : number of the first block to read out
 *  \param[in] numBlocks : number of blocks to read out.
 *  \param[out] res_flags : the response flags sent by the VICC
 *  \param[out] data : the buffer to store the data received
 *  \param[in] dataLen : length of the buffer
 *  \param[out] actLen : the lenght of the data received by from the VICC
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, block read out and written to \a memBlock.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693ReadMultipleBlocks(const iso15693ProximityCard_t* card, uint8_t startblock, uint8_t numBlocks,
                                        uint8_t* res_flags, uint8_t* data, uint16_t dataLen, uint16_t *actLen );

/*! 
 *****************************************************************************
 *  \brief  Write a single block of a given or selected PICC.
 *
 *  This function writes a single block from a given or selected PICC.
 *  If \a card is NULL then the PICC
 *  needs to be selected using #iso15693SelectPicc prior to calling
 *  this function. If \a card is not NULL then this value is treated
 *  as the PICCs UID.
 *
 *  \param[in] card : PICC whose block should be written.
 *                    If \a card is NULL then this parameter
 *                    is ignored and the information is fetched from the PICC
 *                    priorly selected with #iso15693SelectPicc
 *  \param[in] flags : flags to be sent to the card. The bit
 *                     #ISO15693_REQ_FLAG_OPTION specifies if the response 
 *                     must be polled separately.
 *                     This is required by some cards (e.g. from TI) to work
 *                     other cards refuse write command if option flag is set.
 *  \param[out] memBlock : buffer of type #iso15693PiccMemoryBlock_t
 *                      containing the block data to write.
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, block read out and written to \a memBlock.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693WriteSingleBlock(const iso15693ProximityCard_t* card,
                                uint8_t flags,
                                iso15693PiccMemoryBlock_t* memBlock);

/*! 
 *****************************************************************************
 *  \brief  Generic command to write and read arbitrary byte arrays
 *
 *  This function writes a the given buffer and receives an answer.
 *
 *  \param[in] txBuf
 *  \param[in] txLen
 *  \param[out] rxBuf
 *  \param[in] rxLen
 *  \param[out] actlength : return the actual received length inside rxBuf
 *  \param[in] response_wait_ms : number of ms to wait for answer or before 
 *                                sending another EOF and receive the answer 
 *                                afterwards
 *
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NOTFOUND : Requested PICC not available. (Not in field)
 *  \return ERR_NOTSUPP : Request not supported by PICC.
 *  \return ERR_CRC : CRC error.
 *  \return ERR_NOMEM : Not enough memory to perform this function.
 *  \return ERR_COLLISION : Collision happened.
 *  \return ERR_NONE : No error, block read out and written to \a memBlock.
 *
 *****************************************************************************
 */
extern ReturnCode iso15693TxRxNBytes(
                uint8_t* txBuf,
                uint16_t txLen,
                uint8_t* rxBuf,
                uint16_t rxLen,
                uint16_t* actlength,
                uint16_t response_wait_ms);
#endif /* ISO_15693_3_H */

