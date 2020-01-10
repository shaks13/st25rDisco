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

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "platform.h"
#include "utils.h"
#include "rfal_iso15693_2.h"
#include "iso15693_3.h"
#include "logger.h"
#include "st_errno.h"
#include "utils.h"

#include "rfal_rf.h"

/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/*
******************************************************************************
* LOCAL DEFINES
******************************************************************************
*/
#define ISO15693_NUM_UID_BITS 64 /*!< number of UID bits */

#define ISO15693_BUFFER_SIZE (64 + 8) /*!< length of iso15693 general purpose buffer */

/*
******************************************************************************
* LOCAL VARIABLES
******************************************************************************
*/
static uint8_t iso15693Buffer[ISO15693_BUFFER_SIZE+2]; /*!< general
                                        purpose buffer. 2 extra bytes are needed for crc*/
static uint8_t iso15693DirMarker[ISO15693_NUM_UID_BITS]; /*!< direction marker
                                used during inventory for binary tree search.
                                values: 0, 1, 2. 0 means no collision at current
                                bit position. 1 means that left path is tried and
                                2 means right path is tried */
static uint8_t iso15693DefaultSendFlags; /*!< default flags used for iso15693SendRequest */

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/
static ReturnCode iso15693SendRequest(uint8_t cmd,
                uint8_t flags,
                const iso15693ProximityCard_t* card,
                uint8_t* rcvbuffer,
                uint16_t rcvbuffer_len,
                uint16_t* actlength,
                uint8_t* addSendData,
                uint8_t addSendDataLength,
                uint32_t no_response_time_64fcs);
/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/
ReturnCode iso15693Initialize( bool useSlowTX, bool useFastRX )
{
    ReturnCode err;

    err = rfalSetMode( RFAL_MODE_POLL_NFCV, useSlowTX?(RFAL_BR_1p66):(RFAL_BR_26p48), useFastRX?(RFAL_BR_52p97):(RFAL_BR_26p48) );
    rfalSetErrorHandling( RFAL_ERRORHANDLING_NFC );
    
    rfalSetGT( RFAL_GT_NFCV_ADJUSTED );
    rfalSetFDTListen( rfalConv64fcTo1fc(ISO15693_MASK_FDT_LISTEN) );
    rfalSetFDTPoll( RFAL_FDT_POLL_NFCV_POLLER );
  
    rfalFieldOnAndStartGT();

    return err;
}

ReturnCode iso15693Deinitialize(uint8_t keep_on)
{
    if (!keep_on)
    {
        return rfalFieldOff();
    }
    return ERR_NONE;
}

ReturnCode iso15693Inventory(iso15693NumSlots_t slotcnt,
                    uint8_t maskLength,
                    uint8_t* mask,
                    iso15693ProximityCard_t* cards,
                    uint8_t maxCards,
                    uint8_t* cardsFound)
{
    ReturnCode err; /* error variable */
    uint16_t i; /* count variable */
    uint8_t maskLengthBytes; /* length of the mask in full bytes */
    uint8_t bitmask;  /* temporary variable for holding bitmasks */
    uint16_t actlength; /* actual number of bytes received */
    uint8_t currColPos; /* bitposition of the currently processed collision within 64bit UID */
    uint8_t bitsBeforeCol; /* number of bits received within last byte before collision */
    int8_t slot; /* number of the slot currently being processed */
    uint8_t slotNumPos; /* gives the position of the slot number within the mask */
    int8_t currColSlot; /* index of the slot whose collision is currently processed */
    uint16_t colSlots; /* 16 bit long marker holding all slots with collisions (indicated by set bit */
    uint64_t collisions; /* 64 bit long marker holding all unresolved collisions within 64bit UID */
    iso15693ProximityCard_t* crdptr = cards; /* pointer to the card currently used */
    uint8_t crdidx = 0; /* index of the card currently used */

    if (maxCards == 0)
    {
        err = ERR_NOMEM;
        goto out;
    }

    ST_MEMSET(iso15693DirMarker, 0, ISO15693_NUM_UID_BITS);

    /* set inventory flag */
    iso15693Buffer[0] = ISO15693_REQ_FLAG_INVENTORY;

    /* if number of slot is 1 then set the appropriate flag */
    if (ISO15693_NUM_SLOTS_1 == slotcnt)
    {
        iso15693Buffer[0] |= ISO15693_REQ_FLAG_1_SLOT;
    }
    iso15693Buffer[1] = ISO15693_CMD_INVENTORY;
    iso15693Buffer[2] = maskLength;

    /* convert maskLength from number of bits to bytes */
    maskLengthBytes = (maskLength >> 3) + (((maskLength & 7) > 0) ? 1 : 0);
    if ((maskLengthBytes + 3) > ISO15693_BUFFER_SIZE)
    {
        err = ERR_NOMEM;
        goto out;
    }

    /* mask out unneeded bits */
    if (maskLength & 7)
    {
        bitmask = (1 << (maskLength & 7)) - 1;
        mask[maskLengthBytes-1] &= bitmask;
    }
    ST_MEMCPY(&iso15693Buffer[3], mask, maskLengthBytes);

    slotNumPos = maskLength & 7;
    currColPos = 0;
    collisions = 0;
    colSlots = 0;
    currColSlot = -1;
    slot = (slotcnt == ISO15693_NUM_SLOTS_1) ? -1 : 15;
    do
    {
        /* this outer loop iterates as long as there are unresolved
           collisions (including slots with unresolved collisions */
        do
        {
            /* inner loop iterates once over all slots (in case of slot count 16)
               or only one iteration in case of slot count 1.
               After first 16 slot iterations (slot count 16) slot count is
               changed to 1 */

            if ((slotcnt == ISO15693_NUM_SLOTS_1) || (slot == 15))
            {
                /* send the request. Note: CRC is appended by physical layer.
                   Add 3 to mask for flag field, command field and mask length */
                err = rfalISO15693TransceiveAnticollisionFrame(iso15693Buffer, (3 + maskLengthBytes),
                            (uint8_t*)crdptr, sizeof(iso15693ProximityCard_t), &actlength);
            }
            else
            {
                HAL_Delay(5); /* this is a worst case delay for sending EOF. t3min depends on modulation depth and data rate */
                /* in case if slot count 16 slot is incremented by just sending EOF */
                err = rfalISO15693TransceiveEOFAnticollision(
                            (uint8_t*)crdptr, sizeof(iso15693ProximityCard_t), &actlength);
            }

            bitsBeforeCol = actlength%8;
            actlength /= 8;

            if (ERR_RF_COLLISION == err) do
            {
                if (actlength < 2)
                {
                    /* collision before actually receiving UID!!! This should
                    not happen since we ignored collisions in these bytes. */
                    err = ERR_RF_COLLISION;
                    goto out;
                }
                if ((actlength - 2 + (bitsBeforeCol ? 1 : 0)) < maskLengthBytes)
                {
                    /* we provided a mask but received fewer bytes which 
                     * should not happen. Treat this as timeout and continue. */
                    err = ERR_TIMEOUT;
                    break;
                }

                if (ISO15693_NUM_SLOTS_1 == slotcnt)
                {
                    /* in case slot count is 1 collision needs to be resolved */
                    /* find position of collision within received UID and
                       update mask and mask length appropriately */
                    iso15693Buffer[2] = ((actlength - 2) << 3) + bitsBeforeCol + 1;
                    if (iso15693Buffer[2] > ISO15693_NUM_UID_BITS)
                    { /* The collision is inside the CRC: This should not happen, 
                         treat this as a timeout and continue */
                        err = ERR_TIMEOUT;
                        break;
                    }
                    currColPos = iso15693Buffer[2] - 1;
                    collisions |= ((uint64_t)1 << (uint64_t)currColPos);
                    maskLengthBytes = actlength - 1;

                    /* copy received UID to mask */
                    ST_MEMCPY(&iso15693Buffer[3], crdptr->uid, maskLengthBytes);
                    bitmask = (1 << bitsBeforeCol) - 1;

                    /* clear bit where collision happened which means try
                       left branch of the tree first */
                    iso15693Buffer[2+maskLengthBytes] &= bitmask;

                    if (1 == iso15693DirMarker[currColPos])
                    {
                        /* if left branch has been tried out before (dirMarker set to 1)
                           the set the bit where collision happened to 1, i.e.
                           try right branch */
                        iso15693Buffer[2+maskLengthBytes] |= (1 << (currColPos & 7));
                    }
                    /* in any case increment dirMarker to indicate the way we chose */
                    iso15693DirMarker[currColPos]++;
                }
                else
                {
                    /* in case of slot count 16 just mark that there is a collision
                    within this slot. Resolve it later when switching back to slot count 1 */
                    colSlots |= 1 << (15 - slot);
                }
            } while(0);
            if (ERR_RF_COLLISION != err)
            {
                if (ERR_NONE == err)
                {
                    /* received all bytes without collision - store UID */
                    crdidx++;
                    if (crdidx >= maxCards)
                    {
                        goto out_max_cards;
                    }
                    crdptr++;
                }

                if (ISO15693_NUM_SLOTS_1 == slotcnt)
                {
                    i = ISO15693_NUM_UID_BITS;
                    /* a collisions has been resolved. Go back in the tree to find
                     next collision */
                    while (i--)
                    {
                        if (collisions & ((uint64_t)1 << (uint64_t)i))
                        {
                            if (iso15693DirMarker[i] > 1)
                            {
                                /* dirMarker 2 means that both paths (left 'n right)
                                   have been tried (=resolved). Remove this collision */
                                collisions &= ~((uint64_t)((uint64_t)1 << (uint64_t)i));
                                iso15693DirMarker[i] = 0;
                                if (currColSlot >= 0)
                                {
                                    /* if this collision was within a slot unmark
                                       also this slot */
                                    colSlots &= ~(1 << currColSlot);
                                    currColSlot = -1;
                                }
                            }
                            else
                            {
                                /* update collision position. dirMarker 1
                                also means that left branch was tried before.
                                Switch to right branch now */
                                currColPos = i;
                                iso15693Buffer[2] = currColPos + 1;
                                maskLengthBytes = (currColPos >> 3) + 1;
                                iso15693Buffer[2+maskLengthBytes] |= (1 << (currColPos & 7));
                                iso15693DirMarker[currColPos]++;
                                break;
                            }
                        }
                    }

                    if ((currColSlot >= 0) && (iso15693DirMarker[currColPos] == 0))
                    {
                        /* a slot where a collision was found before has been processed
                           with no collision. So unmark this slot */
                        colSlots &= ~(1 << currColSlot);
                        currColSlot = -1;
                    }
                }
            }

            if (slot >= 0)
            {
                slot--;
            }
        } while (slot >= 0);

        /* after 16 iterations switch back to slot count 0 which means
         a normal binary tree search */
        if (ISO15693_NUM_SLOTS_16 == slotcnt)
        {
            slotcnt = ISO15693_NUM_SLOTS_1;
            iso15693Buffer[0] |= ISO15693_REQ_FLAG_1_SLOT;
        }
        if (!collisions && (ISO15693_NUM_SLOTS_1 == slotcnt))
        {
            /* if all collisions are resolved check the slots for open collisions */
            for (i = 0; i < 16; i++)
            {
                if ((1 << i) & colSlots)
                {
                    /* found a slot with unresolved collision.
                       Reset mask length to original value and append slot number to mask */
                    maskLengthBytes = (maskLength >> 3) + (((maskLength & 7) > 0) ? 1 : 0);
                    if (slotNumPos == 0)
                    {
                        /* add an additional byte in case slot number starts at LSB */
                        maskLengthBytes++;
                    }
                    if (slotNumPos > 4)
                    {
                        /* also if slot number would overlap add an additional byte */
                        maskLengthBytes++;
                        /* add slot number to mask */
                        iso15693Buffer[2+maskLengthBytes] &= ~((1 << (8 - slotNumPos)) - 1);
                        iso15693Buffer[2+maskLengthBytes] |= i >> (8 - slotNumPos);
                        iso15693Buffer[1+maskLengthBytes] &= (1 << slotNumPos) - 1;
                        iso15693Buffer[1+maskLengthBytes] |= (i << slotNumPos);
                    }
                    else
                    {
                        /* add slot number to mask */
                        iso15693Buffer[2+maskLengthBytes] &= (1 << slotNumPos) - 1;
                        iso15693Buffer[2+maskLengthBytes] |= (i << slotNumPos);
                    }
                    /* in any case number of mask bits needs to be incremented by 4 */
                    iso15693Buffer[2] = maskLength + 4;
                    currColSlot = i;
                    break;
                }
            }

        }

        /* do not stop before all collisions in all slots are resolved */
    } while (collisions || colSlots);

out_max_cards:
    err = ERR_NONE;
out:
    *cardsFound = crdidx;

    if (*cardsFound == 0)
    {
        err = ERR_NOTFOUND;
    }

    return err;
}

ReturnCode iso15693SendStayQuiet(const iso15693ProximityCard_t* card)
{
    uint16_t actlength;
    uint8_t data;


    /* just send the command - no reply sent by the PICC */
    return iso15693SendRequest(ISO15693_CMD_STAY_QUIET,
            iso15693DefaultSendFlags,
            card,
            &data,
            1,
            &actlength,
            NULL,
            0,
            ISO15693_FWT);
}

ReturnCode iso15693SelectPicc(const iso15693ProximityCard_t* card)
{
    uint16_t actlength;
    ReturnCode err;

    err = iso15693SendRequest(ISO15693_CMD_SELECT,
            iso15693DefaultSendFlags,
            card,
            iso15693Buffer,
            4,
            &actlength,
            NULL,
            0,
            ISO15693_FWT);
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (iso15693Buffer[0] != 0)
    {
        err = ERR_NOMSG;
    }

out:
    return err;
}

ReturnCode iso15693GetPiccSystemInformation(const iso15693ProximityCard_t* card,
                                iso15693PiccSystemInformation_t* sysInfo, uint16_t *sysInfoLen )
{
    ReturnCode err;
    uint8_t offset = 0;

    if( sysInfoLen == NULL )
    {
        return ERR_PARAM;
    }
    
    err = iso15693SendRequest(ISO15693_CMD_GET_SYSTEM_INFORMATION,
            iso15693DefaultSendFlags,
            card,
            iso15693Buffer,
            17,
            sysInfoLen,
            NULL,
            0,
            ISO15693_FWT);
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    sysInfo->dsfid = 0;
    sysInfo->afi = 0;
    sysInfo->memNumBlocks = 0;
    sysInfo->memBlockSize = 0;
    sysInfo->icReference = 0;
    /* process data */
    if (*sysInfoLen > 4)
    {
        /* copy first 10 bytes which are fixed */
        ST_MEMCPY((uint8_t*)sysInfo, iso15693Buffer, 10);
        /* evaluate infoFlags field */
        if (sysInfo->infoFlags & 0x1)
        {
            /* dsfid field present */
            sysInfo->dsfid = iso15693Buffer[10];
            offset++;
        }
        if (sysInfo->infoFlags & 0x2)
        {
            /* afi field present */
            sysInfo->afi = iso15693Buffer[10+offset];
            offset++;
        }
        if (sysInfo->infoFlags & 0x4)
        {
            /* memory size field present */
            sysInfo->memNumBlocks = iso15693Buffer[10+offset];
            sysInfo->memBlockSize = iso15693Buffer[11+offset];
            offset += 2;
        }
        if (sysInfo->infoFlags & 0x8)
        {
            /* ic reference field present */
            sysInfo->icReference = iso15693Buffer[10+offset];
        }
    }
    else
    {
        /* error field set */
        err = ERR_NOTSUPP;
    }

out:
    return err;
}

ReturnCode iso15693ReadSingleBlock(const iso15693ProximityCard_t* card,
                    iso15693PiccMemoryBlock_t* memBlock)
{
    ReturnCode err;
    uint16_t actlength;

    err = iso15693SendRequest(ISO15693_CMD_READ_SINGLE_BLOCK,
            iso15693DefaultSendFlags,
            card,
            iso15693Buffer,
            ISO15693_BUFFER_SIZE,
            &actlength,
            &memBlock->blocknr,
            1,
            ISO15693_FWT);
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (actlength >= 2)
    {
        memBlock->flags = iso15693Buffer[0];
        if (memBlock->flags & ISO15693_RESP_FLAG_ERROR)
        {
            memBlock->actualSize = actlength - 2;
            memBlock->errorCode = iso15693Buffer[1];
        }
        else
        {
            memBlock->actualSize = actlength - 1;
            memBlock->errorCode = 0;
            ST_MEMCPY(memBlock->data, &iso15693Buffer[1], memBlock->actualSize);
        }
    }

out:
    return err;
}



ReturnCode iso15693ReadMultipleBlocks(const iso15693ProximityCard_t* card, uint8_t startblock, uint8_t numBlocks,
                                        uint8_t* res_flags, uint8_t* data, uint16_t dataLen, uint16_t *actLen )
{
    ReturnCode err;
    uint16_t actlength;
    uint8_t addData[2];

    addData[0] = startblock;
    addData[1] = numBlocks - 1;

    err = iso15693SendRequest(ISO15693_CMD_READ_MULTIPLE_BLOCKS,
            iso15693DefaultSendFlags,
            card,
            iso15693Buffer,
            ISO15693_BUFFER_SIZE,
            &actlength,
            addData,
            2,
            ISO15693_FWT);
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (actlength >= 2)
    {
        *res_flags = iso15693Buffer[0];
        if (*res_flags & ISO15693_RESP_FLAG_ERROR)
        {
            *actLen = 0;
            err = iso15693Buffer[1];
        }
        else
        {
            *actLen = ((( actlength > dataLen ) ? dataLen : actlength) - 1);
            ST_MEMCPY( data, &iso15693Buffer[1], *actLen );
        }
    }

out:
    return err;

}

ReturnCode iso15693WriteSingleBlock(const iso15693ProximityCard_t* card,
                                uint8_t flags,
                                iso15693PiccMemoryBlock_t* memBlock)
{
    ReturnCode err;
    uint16_t actlength;
    uint8_t* buf;


    /* just send the request and wait separately for the answer */
    err = iso15693SendRequest(ISO15693_CMD_WRITE_SINGLE_BLOCK,
            flags,
            card,
            NULL,
            0,
            &actlength,
            (uint8_t*)&memBlock->blocknr,
            memBlock->actualSize+1,
            (flags & ISO15693_REQ_FLAG_OPTION)?ISO15693_FWT:4238); /* ~ 20ms */

    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    
    if (flags & ISO15693_REQ_FLAG_OPTION)
    {
        /* according to the standard wait 20ms before sending EOF */
        platformDelay(20);

        buf = iso15693Buffer; /* valid buffer with 0 length: EOF */
        
        EVAL_ERR_NE_GOTO(ERR_NONE, err, out);
    }
    else
    {
        buf = NULL;
    }
    
    err = rfalTransceiveBlockingTxRx( buf, 0, iso15693Buffer, (2+2), &actlength, 
                                     (RFAL_TXRX_FLAGS_CRC_TX_MANUAL | RFAL_TXRX_FLAGS_CRC_RX_REMV | RFAL_TXRX_FLAGS_NFCIP1_OFF | RFAL_TXRX_FLAGS_AGC_ON | RFAL_TXRX_FLAGS_PAR_RX_REMV), 
                                      rfalConv64fcTo1fc( ((flags & ISO15693_REQ_FLAG_OPTION)?ISO15693_FWT:4238) ) );
    
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (actlength > 3)
    {
        err = ERR_NOTSUPP;
    }

    if (actlength == 2 && (ISO15693_RESP_FLAG_ERROR & iso15693Buffer[0]))
        memBlock->errorCode = iso15693Buffer[1];

out:
    return err;
}

ReturnCode iso15693TxRxNBytes(
                uint8_t* txBuf,
                uint16_t txLen,
                uint8_t* rxBuf,
                uint16_t rxLen,
                uint16_t* actlength,
                uint16_t response_wait_ms)
{
    ReturnCode err;
    uint8_t  flags = *txBuf;
    uint8_t* buf;
    uint16_t bufLen;
    uint32_t fwt;

    if (flags & ISO15693_REQ_FLAG_OPTION)
    {
        buf = NULL;
        bufLen = 0;
        fwt = rfalConv64fcTo1fc(ISO15693_FWT);
    }
    else
    {
        buf = rxBuf;
        bufLen = rxLen;
        fwt = rfalConvMsTo1fc(response_wait_ms);
    }
    
    err = rfalTransceiveBlockingTxRx( txBuf, txLen, buf, bufLen, actlength, 
                                     (RFAL_TXRX_FLAGS_CRC_TX_AUTO | RFAL_TXRX_FLAGS_CRC_RX_KEEP | RFAL_TXRX_FLAGS_NFCIP1_OFF | RFAL_TXRX_FLAGS_AGC_ON | RFAL_TXRX_FLAGS_PAR_RX_REMV), 
                                      fwt );    

    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (rxLen > 0 && response_wait_ms && (flags & ISO15693_REQ_FLAG_OPTION))
    {
        /* according to the standard wait 20ms before sending EOF */
        platformDelay(response_wait_ms);
        err = rfalISO15693TransceiveEOFAnticollision( rxBuf, bufLen, actlength );
    }

out:
    return err;
}

/*
******************************************************************************
* LOCAL FUNCTIONS
******************************************************************************
*/
static ReturnCode iso15693SendRequest(uint8_t cmd,
                uint8_t flags,
                const iso15693ProximityCard_t* card,
                uint8_t* rcvbuffer,
                uint16_t rcvbuffer_len,
                uint16_t* actlength,
                uint8_t* addSendData,
                uint8_t addSendDataLength,
                uint32_t no_response_time_64fcs)
{
    uint8_t length;
 

    /* sanity checks first */
    if ((ISO15693_UID_LENGTH + addSendDataLength + 2) > ISO15693_BUFFER_SIZE)
    {
        return ERR_NOMEM;
    }

    /* FLAG_SELECT and FLAG_ADDRESS will be added as required below */
    flags &= (~(ISO15693_REQ_FLAG_SELECT|ISO15693_REQ_FLAG_ADDRESS));
    if (card == NULL)
    {
        /* uid is NULL which means that selected PICC (using #iso15693SelectPicc)
           is used */
        /* set select flag */
        iso15693Buffer[0] = flags | ISO15693_REQ_FLAG_SELECT;
        length = 2;
    }
    else
    {
        /* set address flag */
        iso15693Buffer[0] = flags | ISO15693_REQ_FLAG_ADDRESS;
        /* copy UID */
        ST_MEMCPY((void*)&iso15693Buffer[2], (void*)card->uid, ISO15693_UID_LENGTH);
        length = 2 + ISO15693_UID_LENGTH;
    }

    iso15693Buffer[1] = cmd;

    /* append additional data to be sent */
    ST_MEMCPY(&iso15693Buffer[length], addSendData, addSendDataLength);
    length += addSendDataLength;

    return rfalTransceiveBlockingTxRx( iso15693Buffer, length, rcvbuffer, rcvbuffer_len, actlength, 
                                     (RFAL_TXRX_FLAGS_CRC_TX_AUTO | RFAL_TXRX_FLAGS_CRC_RX_REMV | RFAL_TXRX_FLAGS_NFCIP1_OFF | RFAL_TXRX_FLAGS_AGC_ON | RFAL_TXRX_FLAGS_PAR_RX_REMV), 
                                      rfalConv64fcTo1fc( no_response_time_64fcs * 4 ) );

}


ReturnCode iso15693FastReadSingleBlock(const iso15693ProximityCard_t* card,
                    iso15693PiccMemoryBlock_t* memBlock)
{
    ReturnCode err;
    uint16_t actlength;
    uint8_t txBuf[4];

    txBuf[0] = 0x02;  /* Req fags */
    txBuf[1] = ISO15693_CMD_FAST_READ_SINGLE_BLOCK;  /* CMD      */
    txBuf[2] = ISO15693_M24LR_IC_MFG_CODE;  /* Mfg code */
    txBuf[3] = memBlock->blocknr;  /* block number */


    err = iso15693TxRxNBytes( txBuf, sizeof(txBuf), iso15693Buffer, ISO15693_BUFFER_SIZE, &actlength, 50 );
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (actlength >= 2)
    {
        memBlock->flags = iso15693Buffer[0];
        if (memBlock->flags & ISO15693_RESP_FLAG_ERROR)
        {
            memBlock->actualSize = actlength - 2;
            memBlock->errorCode = iso15693Buffer[1];
        }
        else
        {
            memBlock->actualSize = actlength - 1;
            memBlock->errorCode = 0;
            ST_MEMCPY(memBlock->data, &iso15693Buffer[1], memBlock->actualSize);
        }
    }

out:
    return err;
}


ReturnCode iso15693FastReadMultipleBlocks(const iso15693ProximityCard_t* card, uint8_t startblock, uint8_t numBlocks,
                                        uint8_t* res_flags, uint8_t* data, uint16_t dataLen, uint16_t *actLen )
{
    ReturnCode err;
    uint16_t actlength;
    uint8_t txBuf[5];

    /* Using the non Addressed mode */
    txBuf[0] = 0x02;  /* Req fags */
    txBuf[1] = ISO15693_CMD_FAST_READ_MULTI_BLOCK;  /* CMD              */
    txBuf[2] = ISO15693_M24LR_IC_MFG_CODE;          /* Mfg code         */
    txBuf[3] = startblock;                          /* block number     */
    txBuf[4] = (numBlocks - 1);                     /* number of blocks */

    err = iso15693TxRxNBytes( txBuf, sizeof(txBuf), iso15693Buffer, ISO15693_BUFFER_SIZE, &actlength, 50 );
    EVAL_ERR_NE_GOTO(ERR_NONE, err, out);

    if (actlength >= 2)
    {
        *res_flags = iso15693Buffer[0];
        if (*res_flags & ISO15693_RESP_FLAG_ERROR)
        {
            *actLen = 0;
            err = iso15693Buffer[1];
        }
        else
        {
            *actLen = ((( actlength > dataLen ) ? dataLen : actlength) - 1);
            ST_MEMCPY( data, &iso15693Buffer[1], *actLen );
        }
    }

out:
    return err;
}
