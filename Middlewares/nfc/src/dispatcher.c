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
 *  \brief Application dispatcher
 *
 *  This file is responsible for taking commands from USB HID stream layer,
 *  executing the proper ST25R3911 commands and returning the results over USB.
 *  As entry point start reading documentation of processCmd() function.
 *
 */
/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include <stdint.h>
#include "dispatcher.h"
#include "st_stream.h"
#include "st25R3911.h"
#include "st25r3911_com.h"
#include "st25r3911_interrupt.h"
#include "iso14443a.h"
#include "iso15693_3.h"
#include "iso14443_common.h"
#include "iso14443b.h"
#include "iso14443b_st25tb.h"
#include "rfal_isoDep.h"
#include "led.h"
#include "logger.h"
#include "st_errno.h"
#include "nfc.h"
#include "mifare_ul.h"
#include "felica.h"
#include "topaz.h"
#include "kovio.h"
#include "utils.h"

#include "rfal_rf.h"
#include "rfal_nfca.h"
#include "rfal_nfcb.h"
#include "rfal_chip.h"
#include "rfal_nfcDep.h"
#include "rfal_analogConfig.h"
#include "rfal_iso15693_2.h"

/*
******************************************************************************
* DEFINES
******************************************************************************
*/
#define READ_VAL08_LE(result, type, ptr)      (result = (type) ((ptr)[0]),   (ptr)  += sizeof( uint8_t ))
#define READ_VAL16_LE(result, type, ptr)      (result = (type) ((ptr)[0] + ((ptr)[1] << 8U)),     (ptr)  += sizeof( uint16_t ) )
#define READ_VAL32_LE(result, type, ptr)      (result = (type) ((ptr)[0] + ((ptr)[1] << 8U) + ((ptr)[2] << 16U) + ((ptr)[3] << 24U)),     (ptr)  += sizeof( uint32_t ) )
#define READ_BUF(result, size, ptr)  (ST_MEMCPY(result, (ptr), size),    (ptr)  += size)

#define WRITE_VAL08_LE(value, type, ptr)     ( * ((ptr) ++) = ( uint8_t ) (value & 0xFF) )
#define WRITE_VAL16_LE(value, type, ptr)     ( * ((ptr) ++) = ( uint8_t ) (value & 0xFF),\
                                               * ((ptr) ++) = ( uint8_t ) ((value >>  8)& 0xFF))
#define WRITE_VAL32_LE(value, type, ptr)     ( * ((ptr) ++) = ( uint8_t ) (value & 0xFF)         ,\
                                               * ((ptr) ++) = ( uint8_t ) ((value >>  8) & 0xFF) ,\
                                               * ((ptr) ++) = ( uint8_t ) ((value >> 16) & 0xFF) ,\
                                               * ((ptr) ++) = ( uint8_t ) ((value >> 24) & 0xFF))
#define WRITE_BUF(src, size, dest)  (ST_MEMCPY(dest, src, size),    dest  += size)


#define  ISODEP_DEVICE_INFO_STRUCT_SIZE  31U
#define  ISODEP_POLL_A_STRUCT_SIZE  (20U + ISODEP_DEVICE_INFO_STRUCT_SIZE)
#define  ISODEP_POLL_B_STRUCT_SIZE  (34U + ISODEP_DEVICE_INFO_STRUCT_SIZE)

/*! Command codes for NFC protocol. */
enum nfcCommand
{
    NFC_CMD_INITIALIZE                    = 0xC0,
    NFC_CMD_TX_NBYTES                     = 0xC1,
    NFC_CMD_RX_NBYTES                     = 0xC2,
    NFC_CMD_RFU1                          = 0xC3,
    NFC_CMD_SET_TXBITRATE                 = 0xC4,
    NFC_CMD_SET_RXBITRATE                 = 0xC5,
    NFC_CMD_NFCDEP_TX                     = 0xC6,
    NFC_CMD_NFCDEP_RX                     = 0xC7,
    NFC_CMD_NFCDEP_INIT_HANDLEACTIVATION  = 0xC8,
    NFC_CMD_NFCDEP_DESELECT               = 0xC9,
    NFC_CMD_NFCDEP_TARG_SETPARAMS         = 0xCA,
    NFC_CMD_NFCDEP_ATR                    = 0xCB,
    NFC_CMD_RFU2                          = 0xCC,
    NFC_CMD_NFCDEP_PSL                    = 0xCD,
    NFC_CMD_NFCDEP_TARG_HANDLEACTIVATION  = 0xCE,
    NFC_CMD_DEINITIALIZE                  = 0xCF
};

/*! Command codes for RFAL access. */
enum rfalCommand
{
    RFAL_CMD_INITIALIZE                        = 0x40,
    RFAL_CMD_CALIBRATE                         = 0x41,
    RFAL_CMD_DEINITIALIZE                      = 0x42,
    RFAL_CMD_SET_MODE                          = 0x43,
    RFAL_CMD_GET_MODE                          = 0x44,
    RFAL_CMD_SET_BITRATE                       = 0x45,
    RFAL_CMD_GET_BITRATE                       = 0x46,
    RFAL_CMD_SET_FDTPOLL                       = 0x47,
    RFAL_CMD_GET_FDTPOLL                       = 0x48,
    RFAL_CMD_SET_FDTLISTEN                     = 0x49,
    RFAL_CMD_GET_FDTLISTEN                     = 0x4A,
    RFAL_CMD_SET_GT                            = 0x4B,
    RFAL_CMD_GET_GT                            = 0x4C,
    RFAL_CMD_FIELDON_STARTGT                   = 0x4D,
    RFAL_CMD_FIELDOFF                          = 0x4E,
    RFAL_CMD_START_TXRX                        = 0x4F,
    RFAL_CMD_GET_TXRX_STATUS                   = 0x50,
    RFAL_CMD_ISO14443_TXRX_SHORTFRAME          = 0x51,
    RFAL_CMD_ISO14443_TXRX_ANTICOLLISION       = 0x52,
    RFAL_CMD_FELICA_POLL                       = 0x53,
    RFAL_CMD_ISO15693_TXRX_ANTICOLLISION       = 0x54,
    RFAL_CMD_ISO15693_TXRX_ANTICOLLISION_EOF   = 0x55,
    RFAL_CMD_ISO15693_TXRX_EOF                 = 0x56,
    RFAL_CMD_BLOCKING_TX                       = 0x57,
    RFAL_CMD_BLOCKING_RX                       = 0x58,
    RFAL_CMD_BLOCKING_TXRX                     = 0x59,
};

enum IsodepCommand
{
	ISODEP_CMD_EXTENSION1                     = 0X100,
	ISODEP_CMD_INITIALIZE                     = ISODEP_CMD_EXTENSION1 + 0x00,
	ISODEP_CMD_INITIALIZEWITHPARAMS           = ISODEP_CMD_EXTENSION1 + 0x01,
	ISODEP_CMD_DESELECT                       = ISODEP_CMD_EXTENSION1 + 0x02,
	ISODEP_CMD_ATTRIB                         = ISODEP_CMD_EXTENSION1 + 0x03,
	ISODEP_CMD_FSXI2FSX                       = ISODEP_CMD_EXTENSION1 + 0x04,
	ISODEP_CMD_FWI2FWT                        = ISODEP_CMD_EXTENSION1 + 0x05,
	ISODEP_CMD_GETAPDUTRANSCEIVESTATUS        = ISODEP_CMD_EXTENSION1 + 0x06,
	ISODEP_CMD_GETMAXINFLEN                   = ISODEP_CMD_EXTENSION1 + 0x07,
	ISODEP_CMD_GETTRANSCEIVESTATUS            = ISODEP_CMD_EXTENSION1 + 0x08,
	ISODEP_CMD_ISATTRIB                       = ISODEP_CMD_EXTENSION1 + 0x09,
	ISODEP_CMD_ISRATS                         = ISODEP_CMD_EXTENSION1 + 0x0A,
	ISODEP_CMD_LISTENGETACTIVATIONSTATUS      = ISODEP_CMD_EXTENSION1 + 0x0B,
	ISODEP_CMD_LISTENSTARTACTIVATION          = ISODEP_CMD_EXTENSION1 + 0x0C,
	ISODEP_CMD_PPS                            = ISODEP_CMD_EXTENSION1 + 0x0D,
	ISODEP_CMD_POLLAHANDLEACTIVATION          = ISODEP_CMD_EXTENSION1 + 0x0E,
	ISODEP_CMD_POLLBHANDLEACTIVATION          = ISODEP_CMD_EXTENSION1 + 0x0F,
	ISODEP_CMD_POLLHANDLESPARAMETERS          = ISODEP_CMD_EXTENSION1 + 0x10,
	ISODEP_CMD_RATS                           = ISODEP_CMD_EXTENSION1 + 0x11,
	ISODEP_CMD_STARTAPDUTRANSCEIVE            = ISODEP_CMD_EXTENSION1 + 0x12,
	ISODEP_CMD_STARTTRANSCEIVE                = ISODEP_CMD_EXTENSION1 + 0x13,
	ISODEP_CMD_COMMANDS                       = 0X7F,
};

/*
******************************************************************************
* LOCAL VARIABLES
******************************************************************************
*/
static uint32_t counter = 0;
static const uint8_t dispatcherInterruptResultRegs[24]=
{
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 1st byte */
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 2nd byte */
    ST25R3911_REG_CAPACITANCE_MEASURE_RESULT,
    ST25R3911_REG_PHASE_MEASURE_RESULT,
    ST25R3911_REG_AMPLITUDE_MEASURE_RESULT,0xff,0xff,0xff,0xff,0xff, /* 3rd byte */
};

static uint16_t dispatcherInterruptResults[24];

static uint8_t nfc_is_active;
static uint8_t nfc_is_initiator;
static uint8_t nfc_bitrate;
static uint8_t nfc_lmMask;
static uint8_t first_command_received;


static uint8_t                 gRxBuf[1024];      /* rx buffer used only for rfal non blocking TxRx */
static uint16_t                gRcvdLen;          /* rx length used only for rfal non blocking TxRx */
static rfalIsoDepApduBufFormat gIsoDepApduBuffer; /* Buffer dedicated to apdu */
static bool                    gIsoDepTransceiveOngoing; /* On going flag for APDU tranceive */
static ReturnCode              gIsoDepTransceiveError;   /* status of APDU transceive        */

/*
******************************************************************************
* GLOBAL CONSTANTS
******************************************************************************
*/

/*
******************************************************************************
* LOCAL FUNCTION PROTOTYPES
******************************************************************************
*/
static ReturnCode processDirectCommand(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processInterruptResult(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processKovio(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processTopaz(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processIso14443a(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processIso14443b(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processNfc(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processIso15693(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processFeliCa(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);
static ReturnCode processIsoDep(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize);

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/

/*!
  Process the different protocols provided via \ref usb_hid_stream_driver.h. This function
  also cares for implicit deinitialisation/initialisation if necessary.
  Actuals commands are implemented in
  - #processKovio()
  - #processTopaz()
  - #processIso14443a()
  - #processIso14443b()
  - #processIso15693()
  - #processNfc()
  - #processFeliCa()
  - #processMifare()
 \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()
  */
uint8_t processProtocols ( const uint8_t * rxData, uint16_t rxSize, uint8_t * txData, uint16_t *txSize)
{
    static uint8_t protocol = 0;
    uint8_t        cmd      = *rxData;
    ReturnCode     err      = ERR_REQUEST;
    uint8_t subcmd = cmd & 0x0f;
    uint8_t newprot = cmd & 0xf0;

    if (newprot != protocol)
        {
        if ((protocol & 0xf0) == 0x80)
        { /* kovio barcode commands */
            err = kovioDeinitialize(0);
        }
        else if ((protocol & 0xf0) == 0x90)
        { /* topaz commands */
            err = topazDeinitialize(0);
        }
        else if ((protocol & 0xf0) == 0xa0)
        { /* iso14443a+mifare UL commands */
            err = iso14443ADeinitialize(0);
        }
        else if ((protocol & 0xf0) == 0xb0)
        { /* iso14443a+mifare UL commands */
            err = iso14443BDeinitialize(0);
        }
        else if ((protocol & 0xf0) == 0xc0)
        { /* nfc commands */
            err = nfcDeinitialize();
        }
        else if ((protocol & 0xf0) == 0xd0)
        { /* iso15693 commands */
            err = iso15693Deinitialize(0);
        }
        else if ((protocol & 0xf0) == 0xf0)
        { /* mifare commands */
            err = felicaDeinitialize(0);
        }
        platformDelay(10);

        if (subcmd != 0x0)
        { /* if not an initialize reuse last config */
            if ((cmd & 0xf0) == 0x80)
            { /* kovio commands */
                err = kovioInitialize();
            }
            else if ((cmd & 0xf0) == 0x90)
            { /* topaz commands */
                err = topazInitialize();
            }
            else if ((cmd & 0xf0) == 0xa0)
            { /* iso14443a+mifare UL commands */
                err = iso14443AInitialize();
            }
            else if ((cmd & 0xf0) == 0xb0)
            { /* iso14443a+mifare UL commands */
                err = iso14443BInitialize();
            }
            else if ((cmd & 0xf0) == 0xc0)
            { /* nfc commands */
                err = nfcInitialize(nfc_is_active, nfc_is_initiator, nfc_bitrate, nfc_lmMask);
            }
            else if ((cmd & 0xf0) == 0xd0)
            { /* iso15693 commands */
                err = iso15693Initialize( false, false );
            }
            else if ((cmd & 0xf0) == 0xf0)
            { /* FeliCa commands */
                err = felicaInitialize();
        }
    }
    }

    /* call more specific dispatcher */
    if ((cmd & 0xf0) == 0x80)
    { /* kovio commands */
        err = processKovio(rxData, rxSize, txData, txSize);
    }
    else if ((cmd & 0xf0) == 0x90)
    { /* topaz commands */
        err = processTopaz(rxData, rxSize, txData, txSize);
    }
    else if ((cmd & 0xf0) == 0xa0)
    { /* iso14443a+mifare UL commands */
        err = processIso14443a(rxData, rxSize, txData, txSize);
    }
    else if ((cmd & 0xf0) == 0xb0)
    { /* iso14443a+mifare UL commands */
        err = processIso14443b(rxData, rxSize, txData, txSize);
    }
    else if ((cmd & 0xf0) == 0xc0)
    { /* nfc commands */
        err = processNfc(rxData, rxSize, txData, txSize);
    }
    else if ((cmd & 0xf0) == 0xd0)
    { /* iso15693 commands */
       err = processIso15693(rxData, rxSize, txData, txSize);
    }
    else if ((cmd & 0xf0) == 0xf0)
    { /* FeliCa commands */
        err = processFeliCa(rxData, rxSize, txData, txSize);
    }

    protocol = newprot;

    if (subcmd == 0xf)
        protocol = 0;

    return err;
}

/*!
  Process various generic commands. Forward RFID protocol commands to
  processProtocols().
  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Exact description of chaining of HID reports and stuff are given in file st_stream.h
  Over USB each payload will have a header. With the above rxSize and
  assuming just one command transmitted in one stream which expects an answer the OUT report will look like this:
  <table>
    <tr><th>   Byte</th><th>  0</th><th>       1</th><th>       2<th>       3</th><th>      4     </th><th>      5     </th><th>      6     </th><th>      7     </th><th> 8..8+rxSize-1</th></tr>
    <tr><th>Content</th><td>TID</td><td>length  </td><td>reserved<td>protocol</td><td>tx-prot MSB </td><td>tx-prot LSB </td><td>rx-prot MSB </td><td>rx-prot LSB </td><td>data</td></tr>
    <tr><th>Content</th><td>TID</td><td>rxSize+5</td><td>    0x00<td>    0x44</td><td>rxSize   MSB</td><td>rxSize   LSB</td><td>txSize   MSB</td><td>txSize   LSB</td><td>rxData[0..rxSize-1]</td></tr>
  </table>
  where
  <ul>
    <li> \e TID : arbitrary unique transaction ID, can be a counter
    <li> \e rxSize: the expected size of data this command will/is allowed to return.
  </ul>
  The response to such an OUT report will be an IN report looking like this:
  <table>
    <tr><th>   Byte</th><th>  0</th><th>   1    </th><th>         2</th><th>       3</th><th>       4</th><th>      5</th><th>      6    </th><th>      7    </th><th> 8..8+txSize-1     </th></tr>
    <tr><th>Content</th><td>TID</td><td>length  </td><td>HID status</td><td>protocol</td><td>reserved</td><td>status </td><td>tx-prot MSB</td><td>tx-prot LSB</td><td>data               </td></tr>
    <tr><th>Content</th><td>TID</td><td>txSize+5</td><td>   ret_val</td><td>0x44    </td><td>       0</td><td>ret_val</td><td>txSize MSB </td><td>txSize LSB </td><td>txData[0..txSize-1]</td></tr>
  </table>
  where

  Example for an antenna calibration command:

  OUT:
  <table>
    <tr><th>   Byte</th><th>  0  </th><th>   1  </th><th>       2</th><th>      3</th><th>      4</th><th>      5</th><th>      6</th><th> 7       </th><th>        7</th></tr>
    <tr><th>Content</th><td>TID  </td><td>length</td><td>protocol</td><td>tx-prot</td><td>tx-prot</td><td>rx-prot</td><td>rx-prot</td><td>rxData[0]</td><td>rxData[1]</td></tr>
    <tr><td>Content</td><td>0x1F </td><td>0x07  </td><td>0x44    </td><td>0x00   </td><td>0x02   </td><td>0x00   </td><td>0x01   </td><td>0x15     </td><td>0xD8     </td></tr>
  </table>
  where:<ul>
  <li> 0x15: cmd id for #processDirectCommand()
  <li> 0xd8: the sub command #ST25R3911_CMD_CALIBRATE_ANTENNA
  </ul>
  The reader answer with this report:
  <table>
    <tr><th>   Byte</th><th>  0 </th><th>   1  </th><th>     2    </th><th>       3</th><th>     4  </th><th>     5</th><th>    6  </th><th>  7    </th><th> 8       </th></tr>
    <tr><th>Content</th><td>TID </td><td>length</td><td>HID status</td><td>protocol</td><td>reserved</td><td>status</td><td>tx-prot</td><td>tx-prot</td><td>txData[0]</td></tr>
    <tr><td>Content</td><td>0xFD</td><td>0x06  </td><td>    0x00  </td><td>0x44    </td><td>0x00    </td><td>0x00  </td><td>0x00   </td><td>  1    </td><td>0x60     </td></tr>
  </table>
  <ul>
  <li> 0x60: the data this command produced (content of ST25R3911_REG_ANT_CAL_RESULT)
  <li> 0x00: status 0 : ERR_NONE
  </ul>

  Implemented commands:

  -  #processProtocols()
  -  #processInterruptResult()
  -  #processDirectCommand() see details in the function
  -  Enable automatic Testmode on pins CSI and CSO
      <table>
      <tr><th>   Byte</th><th>       0</th><th>                      1</th><th>       2 </th></tr>
      <tr><th>Content</th><td>0x17(ID)</td><td>enable(true=1, false=0)</td><td>test mode</td></tr>
    </table>
    test mode: Value for Test Register 0x01 (Analog Test and Observation Register)
  - #iso14443TransmitAndReceive()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>1..rxSize</th></tr>
      <tr><th>Content</th><td>0x18(ID)</td><td>data to be transmitted</td></tr>
    </table>
    #iso14443TransmitAndReceive() is called to sent rxSize-1 bytes and receive at most *txSize-4 bytes.
    *txSize is adjusted to the actual received data length + 4.
    <table>
      <tr><th>   Byte</th><th>0..3</th><th>4..*txSize</th></tr>
      <tr><th>Content</th><td>passed microseconds for #iso14443TransmitAndReceive()</td><td>received data buffer</td></tr>
    </table>
  - #st25r3911SetBitrate()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>                  1</th><th>                  2</th></tr>
      <tr><th>Content</th><td>0x19(ID)</td><td>rx bitrate r:2^r*106kBit</td><td>tx bitrate t:2^t*106kBit</td></tr>
    </table>
    no response except status.
  - Measure antenna amplitude and phase.
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x20(ID)</td></tr>
    </table>
    #st25r3911MeasureRF() and st25r3911MeasureAntennaResonance() get called.
    *txSize must be >= 2, response is:
    <table>
      <tr><th>   Byte</th><th>0</th><th>1</th></tr>
      <tr><th>Content</th><td>result of #st25r3911MeasureRF()</td><td>result of #st25r3911MeasureAntennaResonance()</td></tr>
    </table>
  - Enable/Disable RF field
    <table>
      <tr><th>   Byte</th><th>       0</th><th>       1</th></tr>
      <tr><th>Content</th><td>0x22(ID)</td><td>on</td></tr>
    </table>
    where \e on is a boolean value denoting if the field should be turned on(1) or off(0).
    returns status ERR_RF_COLLISION if field could not be turned on.
  -  Read automatic Testmode
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x23(ID)</td></tr>
    </table>
     txSize must be >=2, response is:
      <table>
      <tr><th>   Byte</th><th>    1  </th><th>       2 </th></tr>
      <tr><th>Content</th><td>enabled</td><td>test mode</td></tr>
    </table>
    enabled: true = 1,  false=0; test mode: Value of Test Register 0x01 (Analog Test and Observation Register)

  -  RFAL Initialize
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x40(ID)</td></tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Calibrate
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x41(ID)</td></tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Deinitialize
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x42(ID)</td></tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Set Mode
    <table>
      <tr><th>   Byte</th><th>       0</th><th>       1</th><th>       2</th><th>       3</th></tr>
      <tr><th>Content</th><td>0x43(ID)</td> <td>Mode</td> <td>Tx bit rate</td> <td>Rx bit rate</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Set Bitrate
    <table>
      <tr><th>   Byte</th><th>       0</th><th>       1</th><th>       2</th></tr>
      <tr><th>Content</th><td>0x45(ID)</td> <td>Tx bit rate</td> <td>Rx bit rate</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Set FDT Poll
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1..4</th> </tr>
      <tr><th>Content</th><td>0x47(ID)</td> <td>FDT Poll</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Set FDT Listen
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1..4</th> </tr>
      <tr><th>Content</th><td>0x49(ID)</td> <td>FDT Listen</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Set GT
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1..4</th> </tr>
      <tr><th>Content</th><td>0x4B(ID)</td> <td>GT</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Field On and Start GT
    <table>
      <tr><th>   Byte</th> <th>0</th> </tr>
      <tr><th>Content</th><td>0x4D(ID)</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Field Off
    <table>
      <tr><th>   Byte</th> <th>0</th> </tr>
      <tr><th>Content</th><td>0x4E(ID)</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified.

  -  RFAL Start Transceive
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1..2</th> <th>3..3+txLen</th> <th>3+txLen+1 .. 3+txLen+4</th> <th>3+txLen+5 .. 3+txLen+8</th> </tr>
      <tr><th>Content</th><td>0x4F(ID)</td> <td>txLen</td> <td>txData</td> <td>flags</td> <td>FWT</td>  </tr>
    </table>
     returns status ERR_NONE if no error was identified.

  -  RFAL Get Transceive Status
    <table>
      <tr><th>   Byte</th> <th>0</th> </tr>
      <tr><th>Content</th><td>0x50(ID)</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th><th>    0..1  </th><th> 2..2+rxLen </th></tr>
      <tr><th>Content</th><td>rxLen</td><td>rxData</td></tr>
    </table>

  -  RFAL Transceive ISO14443A Short Frame
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th> <th>2..5</th> </tr>
      <tr><th>Content</th><td>0x51(ID)</td> <td>command(REQA/WUPA)</td> <td>FWT</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th> <th>0..1 </th> <th> 2..2+rxLen </th></tr>
      <tr><th>Content</th> <td>rxLen</td> <td>rxData      </td> </tr>
    </table>

  -  RFAL Transceive ISO14443A Anticollision Frame
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th> <th>2</th> <th>3..6</th> <th>7..txLen</th> </tr>
      <tr><th>Content</th><td>0x52(ID)</td> <td>bytesToSend</td> <td>bitsToSend</td> <td>FWT</td> <td>txData</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th> <th>2..3</th> <th>4..4+rxLength</th> </tr>
      <tr><th>Content</th> <td>bytesReceived</td> <td>bitsReceived</td> <td>rxLength</td> <td>rxData      </td> </tr>
    </table>

  -  RFAL FeliCa Poll
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th> <th>2..3</th> <th>4</th> <th>5</th> </tr>
      <tr><th>Content</th><td>0x53(ID)</td> <td>numSlots</td> <td>SystemCode</td> <td>RequestCode</td> <td>PollResListSize</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th> <th>2..3</th> <th>4..4+rxLength</th> </tr>
      <tr><th>Content</th> <td>devicesDetected</td> <td>CollisionsDetected</td> <td>PollResList</td> <td>rxData      </td> </tr>
    </table>

  -  RFAL ISO15693 Transceive Anticollision Frame
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th> <th>2..2+txLen</th> </tr>
      <tr><th>Content</th><td>0x54(ID)</td> <td>txLength</td> <td>txData</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th><th>0..1 </th> <th> 2..2+rxLen </th></tr>
      <tr><th>Content</th><td>rxLen</td> <td>rxData</td> </tr>
    </table>

  -  RFAL ISO15693 Transceive Anticollision EOF
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th>  </tr>
      <tr><th>Content</th> <td>0x55(ID)</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th><th>0..1 </th> <th> 2..2+rxLen </th></tr>
      <tr><th>Content</th><td>rxLen</td> <td>rxData</td> </tr>
    </table>

  -  RFAL ISO15693 Transceive EOF
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1</th>  </tr>
      <tr><th>Content</th> <td>0x56(ID)</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th><th>0..1 </th> <th> 2..2+rxLen </th></tr>
      <tr><th>Content</th><td>rxLen</td> <td>rxData</td> </tr>
    </table>

  -  RFAL Transceive Blocking Tx
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1..2</th> <th>3..3+txLen</th> <th>3+txLen+1 .. 3+txLen+4</th> <th>3+txLen+5 .. 3+txLen+8</th> </tr>
      <tr><th>Content</th><td>0x57(ID)</td> <td>txLen</td> <td>txData</td> <td>flags</td> <td>FWT</td>  </tr>
    </table>
     returns status ERR_NONE if no error was identified.

  -  RFAL Transceive Blocking Tx
    <table>
      <tr><th>   Byte</th> <th>0</th> </tr>
      <tr><th>Content</th><td>0x58(ID)</td> </tr>
    </table>
     txSize must be >=2, returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th><th>    0..1  </th><th> 2..2+rxLen </th></tr>
      <tr><th>Content</th><td>rxLen</td><td>rxData</td></tr>
    </table>

  -  RFAL Transceive Blocking TxRx
    <table>
      <tr><th>   Byte</th> <th>0</th> <th>1..2</th> <th>3..3+txLen</th> <th>3+txLen+1 .. 3+txLen+4</th> <th>3+txLen+5 .. 3+txLen+8</th> <th>3+txLen+9 .. 3+txLen+10</th> </tr>
      <tr><th>Content</th><td>0x59(ID)</td> <td>txLen</td> <td>txData</td> <td>flags</td> <td>FWT</td> <td>rxBufSize</td> </tr>
    </table>
     returns status ERR_NONE if no error was identified and response is:
    <table>
      <tr><th>   Byte</th><th>    0..1  </th><th> 2..2+rxLen </th></tr>
      <tr><th>Content</th><td>rxLen</td><td>rxData</td></tr>
    </table>

  */
static uint8_t processCmd ( const uint8_t * rxData, uint16_t rxSize, uint8_t * txData, uint16_t *txSize)
{
    uint8_t cmd = *rxData;
    const uint8_t *buf = rxData + 1;
    uint16_t bufSize = rxSize - 1;
    uint8_t err = (uint8_t)ERR_REQUEST;

    if (41 != first_command_received)
    {
      platformLedOff(PLATFORM_LED_FIELD_PORT, PLATFORM_LED_FIELD_PIN);
      ledOff(LED_A);
      ledOff(LED_B);
      ledOff(LED_F);
      ledOff(LED_V);
      ledOff(LED_AP2P);
      first_command_received = 41;
    }

    if (cmd == 0x15)
    {
       err = processDirectCommand(buf, bufSize, txData, txSize);
    }
    if (cmd == 0x17)
    {
        rfalSetObsvMode( buf[0], buf[1] );
        if (*txSize) *txSize = 0;
        err = ERR_NONE;
    }
    if (cmd ==  0x18)
    {
        uint32_t us;
        uint16_t actlength = 0;
        if (*txSize < 4) return (uint8_t)ERR_PARAM;
        timerStopwatchStart();
        err = iso14443TransmitAndReceive(buf,
                bufSize,
                txData + 4,
                *txSize - 4,
                &actlength,
                21186);
        us = timerStopwatchMeasure();
        txData[0] = (us >>  0) & 0xff;
        txData[1] = (us >>  8) & 0xff;
        txData[2] = (us >> 16) & 0xff;
        txData[3] = (us >> 24) & 0xff;
        if (*txSize) *txSize = actlength + 4;
    }
    if (cmd ==  0x19)
    {
        err = rfalSetBitRate( (rfalBitRate)buf[0], (rfalBitRate)buf[1]);
        if (*txSize) *txSize = 0;
    }
    if (cmd == 0x20)
    {
        /* Antenna measurement. */
        rfalChipMeasureAmplitude(txData);
        rfalChipMeasurePhase(txData + 1);
        if (*txSize) *txSize = 2;
        err = ERR_NONE;
    }
    if (cmd == 0x21)
    { /* Get interrupt and result */
        err = processInterruptResult( buf, bufSize, txData, txSize);
    }
    if (cmd == 0x22)
    { /* Turn on/off antenna */
        if (bufSize < 1) return (uint8_t)ERR_PARAM;
        if( buf[0] != 0x00 )
        {
          rfalSetGT( RFAL_TIMING_NONE );
          err = rfalFieldOnAndStartGT();
        }
        else
        {
          rfalFieldOff();
          err = ERR_NONE;
        }
        if (*txSize) *txSize = 0;
    }
    if (cmd == 0x23)
    {
        rfalGetObsvMode( &txData[0], &txData[1] );
        if (*txSize) *txSize = 2;
        err = ERR_NONE;
    }
    if (cmd == 0x24)
    {

      /* Direction: >> */
      for(uint8_t i = 0; i < 6; i++)
      {
          if(i == 0) platformLedOn(PLATFORM_LED_FIELD_PORT, PLATFORM_LED_FIELD_PIN); else platformLedOff(PLATFORM_LED_FIELD_PORT, PLATFORM_LED_FIELD_PIN);
          (i == 1) ? ledOn(LED_A) : ledOff(LED_A);
          (i == 2) ? ledOn(LED_B) : ledOff(LED_B);
          (i == 3) ? ledOn(LED_F) : ledOff(LED_F);
          (i == 4) ? ledOn(LED_V) : ledOff(LED_V);
          (i == 5) ? ledOn(LED_AP2P) : ledOff(LED_AP2P);
          HAL_Delay(70);
      }
      /* Direction: << */
      for(uint8_t i = 0; i < 6; i++)
      {
          if(i == 5) platformLedOn(PLATFORM_LED_FIELD_PORT, PLATFORM_LED_FIELD_PIN); else platformLedOff(PLATFORM_LED_FIELD_PORT, PLATFORM_LED_FIELD_PIN);
          (i == 4) ? ledOn(LED_A) : ledOff(LED_A);
          (i == 3) ? ledOn(LED_B) : ledOff(LED_B);
          (i == 2) ? ledOn(LED_F) : ledOff(LED_F);
          (i == 1) ? ledOn(LED_V) : ledOff(LED_V);
          (i == 0) ? ledOn(LED_AP2P) : ledOff(LED_AP2P);
          HAL_Delay(70);
      }
      platformLedOff(PLATFORM_LED_FIELD_PORT, PLATFORM_LED_FIELD_PIN);

      if (*txSize) *txSize = 0;
      err = ERR_NONE;
    }
    if (cmd == 0x25)
    {
        int i;
        if (*txSize > 12) *txSize = 12;
        for ( i = 0; i<*txSize; i++)
        {
            txData[i] = ((uint8_t*)UID_BASE)[i];
        }
        err = ERR_NONE;
    }
    if (cmd == 0x26)
    {
        err = rfalAnalogConfigListReadRaw(txData, *txSize, txSize);
    }
    if (cmd == 0x27)
    {
        rfalDeinitialize();
        err = rfalAnalogConfigListWriteRaw(buf, bufSize);
        rfalInitialize();
        rfalCalibrate(); /* rfalInitialize() issues SetDefault - clears previous calibration, recalibrate in case analog config needs it */
        rfalSetAnalogConfig( RFAL_ANALOG_CONFIG_TECH_CHIP ); /* rfalCalibrate will employ the found values, analog config might have decided differently */
    }
    if (cmd == RFAL_CMD_INITIALIZE)
    {
        err = rfalInitialize();
        rfalCalibrate(); /* rfalInitialize() issues SetDefault - clears previous calibration, recalibrate in case analog config needs it */
        rfalSetAnalogConfig( RFAL_ANALOG_CONFIG_TECH_CHIP ); /* rfalCalibrate will employ the found values, analog config might have decided differently */
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_CALIBRATE)
    {
        err = rfalCalibrate();
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_DEINITIALIZE)
    {
        err = rfalDeinitialize();
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_SET_MODE)
    {
        err = rfalSetMode( (rfalMode)buf[0], (rfalBitRate)buf[1], (rfalBitRate)buf[2] );
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_SET_BITRATE)
    {
        err = rfalSetBitRate( (rfalBitRate)buf[0], (rfalBitRate)buf[1] );
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_SET_FDTPOLL)
    {
        rfalSetFDTPoll( (uint32_t) ( (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3] ) );
        err = ERR_NONE;
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_SET_FDTLISTEN)
    {
        rfalSetFDTListen( (uint32_t) ( (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3] ) );
        err = ERR_NONE;
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_SET_GT)
    {
        rfalSetGT( (uint32_t) ( (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3] ) );
        err = ERR_NONE;
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_FIELDON_STARTGT)
    {
        err = rfalFieldOnAndStartGT();
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_FIELDOFF)
    {
        err = rfalFieldOff();
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_BLOCKING_TXRX)
    {
        uint16_t txLen;
        uint32_t flags;
        uint32_t fwt;
        uint16_t rcvdLen;

        txLen = ((buf[0]<<8) | buf[1]);
        flags = ((buf[2+txLen+0]<<24) | (buf[2+txLen+1]<<16) | (buf[2+txLen+2]<<8) | (buf[2+txLen+3]) );
        fwt   = ((buf[2+txLen+4+0]<<24) | (buf[2+txLen+4+1]<<16) | (buf[2+txLen+4+2]<<8) | (buf[2+txLen+4+3]) );

        err = rfalTransceiveBlockingTxRx( (uint8_t*)&buf[2], txLen, &txData[2], (*txSize - 2), &rcvdLen, flags, fwt );

        txData[0] = ((rcvdLen>>8)&0xFF);
        txData[1] = ((rcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rcvdLen, (*txSize - 2)) );
    }
    if (cmd == RFAL_CMD_BLOCKING_TX)
    {
        uint16_t txLen;
        uint32_t flags;
        uint32_t fwt;

        gRcvdLen = 0;
        txLen    = ((buf[0]<<8) | buf[1]);
        flags    = ((buf[2+txLen+0]<<24) | (buf[2+txLen+1]<<16) | (buf[2+txLen+2]<<8) | (buf[2+txLen+3]) );
        fwt      = ((buf[2+txLen+4+0]<<24) | (buf[2+txLen+4+1]<<16) | (buf[2+txLen+4+2]<<8) | (buf[2+txLen+4+3]) );

        err = rfalTransceiveBlockingTx( (uint8_t*)&buf[2], txLen, gRxBuf, sizeof(gRxBuf), &gRcvdLen, flags, fwt );
        if (*txSize) *txSize = 0;
    }
    if (cmd == RFAL_CMD_BLOCKING_RX)
    {
        err = rfalTransceiveBlockingRx();

        if( err != ERR_BUSY )
        {
            ST_MEMCPY( (uint8_t*)&txData[2], gRxBuf, rfalConvBitsToBytes(gRcvdLen) );
        }
        txData[0] = ((gRcvdLen>>8)&0xFF);
        txData[1] = ((gRcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rfalConvBitsToBytes(gRcvdLen), sizeof(gRxBuf)) );
    }
    if (cmd == RFAL_CMD_START_TXRX)
    {
        rfalTransceiveContext ctx;
        uint16_t              txLen;

        gRcvdLen = 0;
        txLen    = ((buf[0]<<8) | buf[1]);

        ctx.txBuf     = (uint8_t*)&buf[2];
        ctx.txBufLen  = txLen;
        ctx.rxBuf     = gRxBuf;
        ctx.rxBufLen  = rfalConvBytesToBits( sizeof(gRxBuf) );
        ctx.rxRcvdLen = &gRcvdLen;
        txLen         = rfalConvBitsToBytes(txLen);
        ctx.flags     = ((buf[2+txLen+0]<<24) | (buf[2+ txLen+1]<<16) | (buf[2+txLen+2]<<8) | (buf[2+txLen+3]) );
        ctx.fwt       = ((buf[2+txLen+4+0]<<24) | (buf[2+txLen+4+1]<<16) | (buf[2+txLen+4+2]<<8) | (buf[2+txLen+4+3]) );

        err = rfalStartTransceive( &ctx );

        if (*txSize) *txSize = 0;
    }

    if (cmd == RFAL_CMD_GET_TXRX_STATUS)
    {
        err = rfalGetTransceiveStatus();

        if( err != ERR_BUSY )
        {
            ST_MEMCPY( (uint8_t*)&txData[2], gRxBuf, rfalConvBitsToBytes(gRcvdLen) );
        }
        txData[0] = ((gRcvdLen>>8)&0xFF);
        txData[1] = ((gRcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rfalConvBitsToBytes(gRcvdLen), sizeof(gRxBuf)) );
    }
    if (cmd == RFAL_CMD_ISO14443_TXRX_SHORTFRAME)
    {
        uint32_t fwt;
        uint16_t rcvdLen;

        fwt = ((buf[1+0]<<24) | (buf[1+1]<<16) | (buf[1+2]<<8) | (buf[1+3]) );
        err = rfalISO14443ATransceiveShortFrame( (rfal14443AShortFrameCmd)buf[0], (uint8_t*)&txData[2], (*txSize - 2), &rcvdLen, fwt );

        txData[0] = ((rcvdLen>>8)&0xFF);
        txData[1] = ((rcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rfalConvBitsToBytes(rcvdLen), (*txSize - 2)) );
    }
    if (cmd == RFAL_CMD_ISO14443_TXRX_ANTICOLLISION)
    {
        uint32_t fwt;
        uint16_t rcvdLen;

        *txSize = 4 + buf[0];
        fwt     = ((buf[2+0]<<24) | (buf[2+1]<<16) | (buf[2+2]<<8) | (buf[2+3]) );

        err = rfalISO14443ATransceiveAnticollisionFrame( (uint8_t*)&buf[6], (uint8_t*)&buf[0], (uint8_t*)&buf[1], &rcvdLen, fwt );

        txData[0] = buf[0];
        txData[1] = buf[1];
        txData[2] = ((rcvdLen>>8)&0xFF);
        txData[3] = ((rcvdLen>>0)&0xFF);

        *txSize += rfalConvBitsToBytes( rcvdLen );
        ST_MEMCPY( (uint8_t*)&txData[4], (uint8_t*)&buf[6], buf[0] + rfalConvBitsToBytes( rcvdLen ) );
    }
    if (cmd == RFAL_CMD_FELICA_POLL)
    {
        err = rfalFeliCaPoll( (rfalFeliCaPollSlots)buf[0], (uint16_t)((buf[1]<<8) | (buf[2])), buf[3], (rfalFeliCaPollRes*)&txData[2], buf[4], &txData[0], &txData[1] );

        if (*txSize) *txSize = 2 + ( txData[0] * RFAL_FELICA_POLL_RES_LEN );
    }
    if (cmd == RFAL_CMD_ISO15693_TXRX_ANTICOLLISION)
    {
        uint16_t rcvdLen;

        err = rfalISO15693TransceiveAnticollisionFrame( (uint8_t*)&buf[1], buf[0], &txData[2], (uint8_t)*txSize, &rcvdLen );

        txData[0] = ((rcvdLen>>8)&0xFF);
        txData[1] = ((rcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rfalConvBitsToBytes(rcvdLen), (*txSize - 2)) );
    }
    if (cmd == RFAL_CMD_ISO15693_TXRX_ANTICOLLISION_EOF)
    {
        uint16_t rcvdLen;

        err = rfalISO15693TransceiveEOFAnticollision( &txData[2], (uint8_t)*txSize, &rcvdLen );

        txData[0] = ((rcvdLen>>8)&0xFF);
        txData[1] = ((rcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rfalConvBitsToBytes(rcvdLen), (*txSize - 2)) );
    }
    if (cmd == RFAL_CMD_ISO15693_TXRX_EOF)
    {
        uint16_t rcvdLen;

        err = rfalISO15693TransceiveEOF( &txData[2], (uint8_t)*txSize, &rcvdLen );

        txData[0] = ((rcvdLen>>8)&0xFF);
        txData[1] = ((rcvdLen>>0)&0xFF);

        if (*txSize) *txSize = (2 + MIN( rcvdLen, (*txSize - 2)) );
    }
    if (cmd == ISODEP_CMD_COMMANDS)
    {
        return(processIsoDep(rxData, rxSize, txData, txSize) );
    }
    if ((cmd>>4) >= 0x8)
        err = processProtocols(rxData, rxSize, txData, txSize);

    return err;
}

uint8_t applProcessCmd( uint8_t protocol, uint16_t rxSize, const uint8_t * rxData, uint16_t * txSize, uint8_t * txData )
{ /* forward to different function to have place for doxygen documentation
     because applProcessCmd is already documented in usb_hid_stream_driver.h*/
    return processCmd( rxData, rxSize, txData, txSize);
}


/*!
  This function processes certain interrupts and stores results retrieved for
  later transmission over USB */
void dispatcherInterruptHandler()
{
uint8_t i;
uint8_t val;
uint16_t isrs;

    for (i=0; i<24; i++)
    {
        if(dispatcherInterruptResultRegs[i] >= 0x40) continue;
        if(!st25r3911GetInterrupt(1UL<<i)) continue;

        isrs = dispatcherInterruptResults[i] >> 8;
        if (isrs < 255) isrs++;

        st25r3911ReadRegister(dispatcherInterruptResultRegs[i], &val);

        dispatcherInterruptResults[i] = (isrs<<8) | val;
    }
}

/*!
 * @brief  Handle background task needed by the dispatcher
 * This comprises functions for keeping rfal active and also functions for low
 * level interrupt access.
 */
void dispatcherWorker()
{
    dispatcherInterruptHandler();
    rfalWorker();
    if (gIsoDepTransceiveOngoing == true)
    {
        gIsoDepTransceiveError = rfalIsoDepGetApduTransceiveStatus();
        if (ERR_BUSY != gIsoDepTransceiveError)
        { /* if not busy anymore the transceive is supposed to be finished */
            gIsoDepTransceiveOngoing = false;
        }    
    }
}

/*
******************************************************************************
* LOCAL FUNCTIONS
******************************************************************************
*/

/*!
  Return the interrupt results stored from dispatcherInterruptHandler() via USB to GUI.
  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - Get interrupt result:
  <table>
    <tr><th>   Byte</th><th>       0</th><th>       1</th></tr>
    <tr><th>Content</th><td>0x21(ID)</td><td>0..23:irq in question</td></tr>
  </table>
  txSize needs to accomodate 2 bytes of response:
  <table>
    <tr><th>   Byte</th><th>       0</th><th>           1</th></tr>
    <tr><th>Content</th><td>num of isrs occurred</td><td>last result</td></tr>
  </table>
    Currently supported interrups and corresponding results are:
    - Wakeup Capacitive IRQ(16): ST25R3911_REG_CAPACITANCE_MEASURE_RESULT
    - Wakeup Phase IRQ(17)     : ST25R3911_REG_PHASE_MEASURE_RESULT
    - Wakeup Amplituted IRQ(18): ST25R3911_REG_AMPLITUDE_MEASURE_RESULT
  */
static ReturnCode processInterruptResult(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    if(rxSize < 1) return ERR_REQUEST;
    if(rxData[0] > 23) return ERR_PARAM;
    if(dispatcherInterruptResultRegs[rxData[0]] > 0x3f) return ERR_PARAM;
    if(*txSize < 2) return ERR_PARAM;

    *txSize = 2;
    txData[0] =  dispatcherInterruptResults[rxData[0]] >> 8;
    txData[1] =  dispatcherInterruptResults[rxData[0]] & 0xff;

    dispatcherInterruptResults[rxData[0]] = 0; /* clear value */

    return ERR_NONE;
}

/*!
  Process Topaz type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #topazInitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x90(ID)</td></tr>
    </table>
    no return value only status
  - #topazDeinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>         1</th></tr>
      <tr><th>Content</th><td>0x9f(ID)</td><td>keep_rf_on</td></tr>
    </table>
    no return value only status
  - #topazReqaWupa() + topazReadUID()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>1</th></tr>
      <tr><th>Content</th><td>0x91(ID)</td><td>0x26(REQA) or 0x52(WUPA)</td></tr>
    </table>
    *txsize must be set to allow at least 17 return values.
    <table>
      <tr><th>   Byte</th><th> 1..2</th><th>3..6</th></tr>
      <tr><th>Content</th><td>atqa </td><td> uid</td></tr>
    </table>
*/
static ReturnCode processTopaz(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    ReturnCode err = ERR_REQUEST;
    uint16_t bufSize = rxSize - 1;
    topazProximityCard_t card;
    const uint8_t *buf = rxData + 1;
    uint8_t cmd = rxData[0];
    ST_MEMSET(txData, 0xff, *txSize);

    switch (cmd)
    {
        case 0x90:
            err = topazInitialize();
            *txSize = 0;
            break;
        case 0x9f:
            {
                uint8_t keep_on = 0;
                if (bufSize > 0) keep_on = buf[0];
                err = topazDeinitialize(keep_on);
                *txSize = 0;
            }
            break;
        case 0x91:
            if (*txSize < 8) return ERR_PARAM;
            err = topazReqaWupa((topazCommand_t)buf[0], &card);
            if (ERR_NONE == err)
                err = topazReadUID(&card);

            txData[0] = card.atqa[0];
            txData[1] = card.atqa[1];
            txData[2] = card.hr[0];
            txData[3] = card.hr[1];
            txData[4] = card.uid[0];
            txData[5] = card.uid[1];
            txData[6] = card.uid[2];
            txData[7] = card.uid[3];

            if(*txSize > 0) *txSize = 8;
            if(ERR_NOTSUPP == err)
                *txSize = 2;
            break;
        case 0x92:
            if (*txSize < 6) return ERR_PARAM;
            err = topazReadUID(&card);

            txData[0] = card.hr[0];
            txData[1] = card.hr[1];
            txData[2] = card.uid[0];
            txData[3] = card.uid[1];
            txData[4] = card.uid[2];
            txData[5] = card.uid[3];

            if(*txSize > 0) *txSize = 6;
            break;
        case 0x93:
            if (bufSize < 4) { *txSize = 0; return ERR_PARAM;}
            ST_MEMCPY(card.uid, buf, 4);
            err = topazReadAll(&card, txData, *txSize, txSize);
            break;
        case 0x94: /* FIXME: should be Read Byte */
            break;
        case 0x95:
            if (bufSize < 6) return ERR_PARAM;
            ST_MEMCPY(card.uid, buf, 4);
            {
                uint8_t addr = buf[4], data = buf[5];
                err = topazWriteByte(&card, addr, data);
            }
            break;
        default:
            err = ERR_PARAM;
            *txSize = 0;
    }
    return err;
}

/*!
  Process Kovio barcode type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #kovioInitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x80(ID)</td></tr>
    </table>
    no return value only status
  - #kovioDeinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>         1</th></tr>
      <tr><th>Content</th><td>0x8f(ID)</td><td>keep_rf_on</td></tr>
    </table>
    no return value only status
  - #kovioRead()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0x81(ID)</td></tr>
    </table>
    *txsize must be set to allow at least 16 return values.
    <table>
      <tr><th>   Byte</th><th> 0..15</th></tr>
      <tr><th>Content</th><td>  uid </td></tr>
    </table>
*/
static ReturnCode processKovio(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    ReturnCode err = ERR_REQUEST;
    uint16_t bufSize = rxSize - 1;
    kovioProximityCard_t card;
    const uint8_t *buf = rxData + 1;
    uint8_t cmd = rxData[0];
    ST_MEMSET(txData, 0xff, *txSize);

    switch (cmd)
    {
        case 0x80:
            err = kovioInitialize();
            *txSize = 0;
            break;
        case 0x8f:
            {
                uint8_t keep_on = 0;
                if (bufSize > 0) keep_on = buf[0];
                err = kovioDeinitialize(keep_on);
                *txSize = 0;
            }
            break;
        case 0x81:
            if (*txSize < 32) return ERR_PARAM;
            err = kovioRead(&card);
            if(err) *txSize = 0;
            else
            {
                ST_MEMCPY(txData, card.uid, card.length);
                *txSize = card.length;
            }
            break;
        default:
            err = ERR_PARAM;
            *txSize = 0;
    }
    return err;
}


/*!
  Process ISO14443A type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #iso14443AInitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xa0(ID)</td></tr>
    </table>
    no return value only status
  - #iso14443ADeinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>         1</th></tr>
      <tr><th>Content</th><td>0xaf(ID)</td><td>keep_rf_on</td></tr>
    </table>
    no return value only status
  - #iso14443ASelect()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>1</th></tr>
      <tr><th>Content</th><td>0xa1(ID)</td><td>0x26(REQA) or 0x52(WUPA)</td></tr>
    </table>
    *txsize must be set to allow at least 17 return values.
    <table>
      <tr><th>   Byte</th><th>  0</th><th> 1..2</th><th>                 3</th><th>4..6</th><th>7..17</th></tr>
      <tr><th>Content</th><td>col</td><td>atqa</td><td>cascadelevels(1..3)</td><td>sak</td><td> uid</td></tr>
    </table>
      There are three possible values of \e cascadelevels:
      - 1: sak is one byte long, \e uid is 4 bytes long
      - 2: sak is two bytes long, \e uid is 7 bytes long
      - 3: sak is three bytes long, \e uid is 10 bytes long
  - #iso14443ASendHlta()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xa3(ID)</td></tr>
    </table>
    not response data available only status
  - #iso14443AEnterProtocolMode() sends ISO14443A RATS command
    <table>
      <tr><th>   Byte</th><th>       0</th><th>   1</th><th>  2</th></tr>
      <tr><th>Content</th><td>0xa4(ID)</td><td>fsdi</td><td>cid</td></tr>
    </table>
    where:
      - \e fsdi: 0-0xf Frame size for Device Index
        <table>
          <tr><th> 0</th><th> 1</th><th> 2</th><th> 3</th><th> 4</th><th> 5</th><th> 6</th><th>  7</th><th>  8</th></tr>
          <tr><td>16</td><td>24</td><td>32</td><td>40</td><td>48</td><td>64</td><td>96</td><td>128</td><td>256</td></tr>
        </table>
      - \e cid: Card Identifier to assign to the card.
      .
    *txsize must be set to allow at least 7 return values for ATS. But card response may be even longer.
    <table>
      <tr><th>   Byte</th><th> 0</th><th> 1</th><th>    2</th><th>    3</th><th>    4</th><th> 5</th><th> 6</th></tr>
      <tr><th>Content</th><td>TL</td><td>T0</td><td>TA(1)</td><td>TB(1)</td><td>TC(1)</td><td>T1</td><td>Tk</td></tr>
    </table>
  - #iso14443ASendProtocolAndParameterSelection()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>  1</th><th>        2</th><th>   3</th></tr>
      <tr><th>Content</th><td>0xa5(ID)</td><td>cid</td><td>pss0=0x11</td><td>pss1</td></tr>
    </table>
    where:
      - \e cid: the cid set using RATS command
      - \e pss0: not used internally set to 0x11, should always be set to 0x11
      - \e pss1: PSS1 according to standard, upper 4 bits shall be 0, lower 4 bits are DSI and DRI.
    no return value only status
  - #iso14443Deselect()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xa6(ID)</td></tr>
    </table>
    no return value only status
  - #mifareUlReadNBytes()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>    1</th></tr>
      <tr><th>Content</th><td>0xa7(ID)</td><td>start</td></tr>
    </table>
    This function reads starting from \e start *txSize bytes. Response is:
    <table>
      <tr><th>   Byte</th><th>0..actual_received length</th></tr>
      <tr><th>Content</th><td>received data</td></tr>
    </table>
  - #mifareUlWritePage()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>    1</th><th>2..6</th></tr>
      <tr><th>Content</th><td>0xa8(ID)</td><td>start</td><td>data</td></tr>
    </table>
    no return value only status
  */
static ReturnCode processIso14443a(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    ReturnCode err;
    uint16_t bufSize = rxSize - 1;
    uint16_t actlength;
    iso14443AProximityCard_t card;
    const uint8_t *buf = rxData + 1;
    uint8_t cmd = rxData[0];
    uint8_t perform_ac = 1;
    ST_MEMSET(txData, 0xff, *txSize);

    switch (cmd)
    {
        case 0xa0:
            err = iso14443AInitialize();
            *txSize = 0;
            return err;

        case 0xaf:
            {
                uint8_t keep_on = 0;
                if (bufSize > 0) keep_on = buf[0];
                err = iso14443ADeinitialize(keep_on);
                *txSize = 0;
            }
            return err;

        case 0xa1:
            if (bufSize > 1) perform_ac = buf[1];
            if (*txSize < 17) return ERR_PARAM;
            err = iso14443ASelect((iso14443ACommand_t)buf[0], &card, perform_ac);
            txData[0] = card.collision;
            txData[1] = card.atqa[0];
            txData[2] = card.atqa[1];
            ST_MEMSET(txData+3, 0, 14);
            if(err == ERR_NONE){
              logUsart("ISO14443A/NFC-A card found. ATQA: %s", hex2Str(card.atqa, 2));
              if (perform_ac)
              {
                txData[3] = card.cascadeLevels;
                txData[4] = card.sak[0];
                txData[5] = card.sak[1];
                txData[6] = card.sak[2];
                ST_MEMCPY(txData+7, card.uid, card.actlength);
                logUsart(" SAK: %s UID: %s", hex2Str(card.sak, 3), hex2Str(card.uid, card.actlength));
              }
              logUsart("\n");
            }
            if(*txSize > 0) *txSize = 17;
            break;

        case 0xa3:
            err = iso14443ASendHlta();
            *txSize = 0;
            break;

        case 0xa4:
            if (bufSize < 2) return ERR_PARAM;
            if (*txSize < 7) return ERR_PARAM;
            {
                uint8_t fsdi = buf[0];
                uint8_t cid = buf[1];
                uint8_t fscid = (cid&0xf) | ((fsdi&0xf)<<4);
                err = iso14443AEnterProtocolMode(fscid, txData, *txSize, &actlength);
                *txSize = actlength;
            }
            break;

        case 0xa5:
            {
                uint8_t cid = buf[0] & 0xf;
                /* buf[1] may be used for PSS0, should be 0x11 */
                uint8_t pss1 = buf[2];
                err = iso14443ASendProtocolAndParameterSelection(cid, pss1);
            }
            *txSize = 0;
            break;

        case 0xa6:
            err = iso14443Deselect();
            *txSize = 0;
            break;

        case 0xa7:
            /* mifare ul read */
            {
                uint8_t actlen;
                err = mifareUlReadNBytes(buf[0], txData, *txSize, &actlen);
                *txSize = actlen;
            }
            break;

        case 0xa8:
            /* mifare ul write page */
            if (bufSize != 5)
            {
                return ERR_PARAM;
            }
            err = mifareUlWritePage(buf[0], buf+1);
            *txSize = 0;
            break;

        default:
            err = ERR_PARAM;
            *txSize = 0;
    }

    if (ERR_NONE == err){
      ledOnOff(LED_A, VISUAL_FEEDBACK_DELAY);
    }

    return err;
}

/*!
  Process ISO14443B type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #iso14443BInitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xb0(ID)</td></tr>
    </table>
    response:
    <table>
      <tr><th>   Byte</th></tr>
      <tr><th>Content</th></tr>
    </table>
  - #iso14443BDeinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>         1</th></tr>
      <tr><th>Content</th><td>0xbf(ID)</td><td>keep_rf_on</td></tr>
    </table>
    no return value only status
  - #iso14443Deselect()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xb4(ID)</td></tr>
    </table>
    no return value only status
  - #iso14443BSelect() sends REQB or WUPB
    <table>
      <tr><th>   Byte</th><th>       0</th><th>  1           </th><th>  2</th><th>      3    </th></tr>
      <tr><th>Content</th><td>0xb1(ID)</td><td>wubp_else_reqb</td><td>afi</td><td>slotcnt_exp</td></tr>
    </table>
    where:
    - \e wupb_else_reqb : if true wupb is sent else reqb
    - \e afi : The AFI field as in the standard
    - \e slotcnt_exp : lower three bits determine slot count(0=1,1=2,2=4,3=8,4=16)
    response:
    <table>
      <tr><th>   Byte</th><th>   0</th><th>1..4</th><th>   5..8 </th><th>   9..11 </th><th>12 </th></tr>
      <tr><th>Content</th><td>atqb</td><td>pupi</td><td>app_data</td><td>prot_info</td><td>col</td></tr>
    </table>
    where:
    - \e col: 1=collision has happened and was resolved, 0=no collision
    .
  - #iso14443BSendHltb()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>1..4</th></tr>
      <tr><th>Content</th><td>0xb2(ID)</td><td>PUPI</td></tr>
    </table>
    no response data only status
  - #iso14443BEnterProtocolMode() sends ATTRIB
    <table>
      <tr><th>   Byte</th><th>       0</th><th>1..4</th><th> 5..8 </th></tr>
      <tr><th>Content</th><td>0xb3(ID)</td><td>pupi</td><td>params</td></tr>
    </table>
    response:
    <table>
      <tr><th>   Byte</th><th>   0    </th></tr>
      <tr><th>Content</th><td>mbli_cid</td></tr>
    </table>
    where:
    - \e mbli_cid : upper 4 bits: mbli and lower 4 bits: cid

*/
static ReturnCode processIso14443b(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    ReturnCode err;
    uint8_t cmd = rxData[0];
    const uint8_t *buf = rxData + 1;
    const uint16_t bufSize = rxSize - 1;
    iso14443BProximityCard_t card;
    iso14443BAttribParameter_t param;
    iso14443BAttribAnswer_t answer;

    switch (cmd)
    {
        case 0xb0:
            *txSize = 0;
            err = iso14443BInitialize();
            //break;
            return err;

        case 0xbf:
            {
                uint8_t keep_on = 0;
                if (bufSize > 0) keep_on = buf[0];
                err = iso14443BDeinitialize(keep_on);
            }
            *txSize = 0;
            return err;

        case 0xb1:
            if(bufSize < 3) return ERR_PARAM;
            if(*txSize > 13) *txSize = 13;
            err = iso14443BSelect(buf[0] ?  ISO14443B_CMD_WUPB : ISO14443B_CMD_REQB,
                    &card,
                    buf[1],
                    (iso14443BSlotCount_t)(buf[2] & 0x7));
            ST_MEMCPY(txData , &card, *txSize);
            if (ERR_NONE == err)
            {
              logUsart("ISO14443B/NFC-B card found. UID: %s\n", hex2Str(card.pupi, ISO14443B_PUPI_LENGTH));
            }
            break;

        case 0xb2:
            ST_MEMCPY(card.pupi, &buf[0], ISO14443B_PUPI_LENGTH);
            err = iso14443BSendHltb(&card);
            *txSize = 0;
            break;

        case 0xb3:
            /* map to iso14443BEnterProtocolMode */
            ST_MEMCPY(card.pupi, &buf[0], ISO14443B_PUPI_LENGTH);
            ST_MEMCPY(&param, &buf[4], 4);
            err = iso14443BEnterProtocolMode(&card, &param, &answer);
            if (ERR_NONE == err)
            {
                txData[0] = (answer.mbli << 4) | answer.cid;
            }
            if(*txSize > 0) *txSize = 1;
            break;

        case 0xb4:
            err = iso14443Deselect();
            *txSize = 0;
            break;

        case 0xb5:
            {
                iso14443B_ST25TB_t stcard;
                err = iso14443B_ST25TB_Initiate(&stcard);
                ST_MEMCPY(txData, &stcard.Chip_ID, 1);
                *txSize = 1;
            }
            break;

        case 0xb6:
            {
                iso14443B_ST25TB_t stcard;
                err = iso14443B_ST25TB_Pcall16(&stcard);
                ST_MEMCPY(txData, &stcard.Chip_ID, 1);
                *txSize = 1;
            }
            break;

        case 0xb7:
            {
                iso14443B_ST25TB_t stcard;
                uint8_t sn = buf[0];
                if(bufSize < 1) return ERR_PARAM;
                err = iso14443B_ST25TB_Slot_marker(sn, &stcard);
                ST_MEMCPY(txData, &stcard.Chip_ID, 1);
                *txSize = 1;
            }
            break;

        case 0xb8:
            {
                iso14443B_ST25TB_t stcard;
                if(bufSize < 1) return ERR_PARAM;
                stcard.Chip_ID = buf[0];
                err = iso14443B_ST25TB_Select(&stcard);
                *txSize = 0;
            }
            break;

        case 0xb9:
            {
                iso14443B_ST25TB_t stcard;
                if (*txSize < 8) return ERR_PARAM;
                *txSize = 0;
                err = iso14443B_ST25TB_Get_UID(&stcard);
                if (ERR_NONE == err)
                {
                    *txSize = 8;
                    ST_MEMCPY(txData, &stcard.uid, ISO14443B_ST25TB_UIDSIZE);
                    logUsart("ST25TB card found. UID: %s\n", hex2Str(stcard.uid, ISO14443B_ST25TB_UIDSIZE));
                }
            }
            break;

        case 0xba:
            {
                iso14443B_ST25TB_t stcard;
                if (*txSize < 9) return ERR_PARAM;
                *txSize = 0;
                err = iso14443B_ST25TB_SingulateAndGetUID(&stcard);
                if (ERR_NONE == err)
                {
                    *txSize = 9;
                    ST_MEMCPY(txData, &stcard.Chip_ID, 1);
                    ST_MEMCPY(txData+1, stcard.uid, 8);
                    logUsart("ST25TB card found. UID: %s\n", hex2Str(stcard.uid, ISO14443B_ST25TB_UIDSIZE));
                }
            }
            break;

        case 0xbb:
            {
                err = iso14443B_ST25TB_Completion();
                *txSize = 0;
            }
            break;

        case 0xbc:
            {
                err = iso14443B_ST25TB_Reset_to_inventory();
                *txSize = 0;
            }
            break;

        case 0xbd:
            {
                uint8_t address = buf[0];
                if (*txSize < 4) return ERR_PARAM;
                *txSize = 0;
                if(bufSize < 1) return ERR_PARAM;
                err = iso14443B_ST25TB_Read_block(address, txData);
                if (ERR_NONE == err)
                {
                    *txSize = 4;
                }
            }
            break;

        case 0xbe:
            {
                uint8_t address = buf[0];
                if(bufSize < 5) return ERR_PARAM;
                err = iso14443B_ST25TB_Write_block(address, rxData + 1);
                *txSize = 0;
            }
            break;

        default:
            err = ERR_REQUEST;
            *txSize = 0;
            break;
    }

    if (ERR_NONE == err){
      ledOnOff(LED_B, VISUAL_FEEDBACK_DELAY);
    }

    return err;
}

/*!
  Process NFC type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #nfcInitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>        1</th><th>        2</th><th>      3</th></tr>
      <tr><th>Content</th><td>0xc0(ID)</td><td>is_active</td><td>initiator</td><td>bitrate</td></tr>
    </table>
    no return value only status
  - #nfcDeinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xcf(ID)</td></tr>
    </table>
    no return value only status
  - #nfcTxNBytes()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>       1                   </th><th>2..txSize-2</th></tr>
      <tr><th>Content</th><td>0xc1(ID)</td><td>perform_collision_avoidance</td><td>       data</td></tr>
    </table>
    no return value only status
  - #nfcRxNBytes() receives at most *txSize bytes from the other NFC device
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xc2(ID)</td></tr>
    </table>
    response:
    ERR_NOMSG is returned as status in case there was no message.
    <table>
      <tr><th>   Byte</th><th>0..actlength</th></tr>
      <tr><th>Content</th><td>        data</td></tr>
    </table>
  - #nfcSetTxBitrate() sets the transmit bitrate
    <table>
      <tr><th>   Byte</th><th>       0</th><th>        1</th>
      <tr><th>Content</th><td>0xc4(ID)</td><td>bitrate</td>
    </table>
    no return value only status
  - #nfcSetRxBitrate() sets the receive bitrate
    <table>
      <tr><th>   Byte</th><th>       0</th><th>        1</th>
      <tr><th>Content</th><td>0xc5(ID)</td><td>bitrate</td>
    </table>
    no return value only status
*/
static ReturnCode processNfc(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    const uint8_t *buf = rxData + 1;
    uint16_t bufSize = rxSize - 1;
    uint8_t cmd = rxData[0];
    ReturnCode err = ERR_NONE;
    uint16_t actlength;

    switch (cmd)
    {
        case NFC_CMD_INITIALIZE:
            if (bufSize < 3) return ERR_PARAM;
            {
                nfc_is_active = !!buf[0];
                nfc_is_initiator = !!buf[1];
                nfc_bitrate = buf[2]; /* speed is 2^bitrate * 106 kb/s */
                nfc_lmMask = buf[3];
                err = nfcInitialize(nfc_is_active, nfc_is_initiator, nfc_bitrate, nfc_lmMask);
            }
            return err;

        case NFC_CMD_DEINITIALIZE:
            err = nfcDeinitialize();
            return err;

        case NFC_CMD_TX_NBYTES:
            if (bufSize < 1) return ERR_PARAM;
            {
                err = nfcTxNBytes(buf, bufSize);
            }
            break;

        case NFC_CMD_RX_NBYTES:
            err = nfcRxNBytes(txData, &txData[1], *txSize - 1, &actlength);
            *txSize = actlength + 1;
            break;

        case NFC_CMD_RFU1:
        case NFC_CMD_RFU2:
            return ERR_REQUEST;

        case NFC_CMD_SET_TXBITRATE:
            err = nfcSetTxBitrate(buf[0]);
            break;

        case NFC_CMD_SET_RXBITRATE:
            err = nfcSetRxBitrate(buf[0]);
            break;

        case NFC_CMD_NFCDEP_TX:
            err = nfcDepTx( buf, bufSize );
            break;

        case NFC_CMD_NFCDEP_RX:
            err = nfcDepRx( &txData[0], (*txSize), &actlength );
            *txSize = actlength;
            break;

        case NFC_CMD_NFCDEP_INIT_HANDLEACTIVATION:
            if( *txSize < (2 + RFAL_NFCDEP_ATRRES_MAX_LEN) )  /* Response: current bit rate, ATR_RES length, ATR_RES */
            {
                return ERR_PARAM;
            }
            err = nfcDepInitiatorHandleActivation( (uint8_t*)&buf[0], buf[10], buf[11], (uint8_t*)&buf[12], &txData[0], &txData[1], &txData[2] );
            break;

        case NFC_CMD_NFCDEP_DESELECT:
            err = nfcDepDeselect( );
            break;

        case NFC_CMD_NFCDEP_TARG_SETPARAMS:
            nfcDepTargetSetParams( (uint8_t*)&buf[0], buf[10], buf[11], (uint8_t*)&buf[12] );
            err = ERR_NONE;
            break;

        case NFC_CMD_NFCDEP_ATR:
            if( *txSize < (1 + RFAL_NFCDEP_ATRRES_MAX_LEN) )  /* Response: ATR_RES length, ATR_RES */
            {
                return ERR_PARAM;
            }
            err = nfcDepATR( (uint8_t*)&buf[0], buf[10], (uint8_t*)&buf[11], &txData[0], &txData[1] );
            break;

        case NFC_CMD_NFCDEP_PSL:
            err = nfcDepPSL( buf[0], buf[1], buf[2] );
            break;

        case NFC_CMD_NFCDEP_TARG_HANDLEACTIVATION:
            if( *txSize < (2 + RFAL_NFCDEP_ATRRES_MAX_LEN) )  /* Response: bitrate, ATR_REQ length, ATR_REQ */
            {
                return ERR_PARAM;
            }
            err = NfcDepTargetHandleActivation( &txData[0], (bool*)&txData[1], &txData[2], &txData[3] );
            break;

        default:
            err = ERR_REQUEST;
    }

    if (ERR_NONE == err){
      ledOnOff(LED_AP2P, VISUAL_FEEDBACK_DELAY);
    }

    return err;
}

/*!
  Process ISO15693 type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #iso15693Initialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>           1</th></tr>
      <tr><th>Content</th><td>0xd0(ID)</td><td>use_1_of_256</td></tr>
    </table>
    no return value only status
  - #iso15693Deinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>         1</th></tr>
      <tr><th>Content</th><td>0xdf(ID)</td><td>keep_rf_on</td></tr>
    </table>
    no return value only status
  - #iso15693Inventory()
    <table>
      <tr><th>   Byte</th><th>       0        </th></tr>
      <tr><th>Content</th><td>0xd2 or 0xd3(ID)</td></tr>
    </table>
    Command \e 0xd2 uses 1 slot, \e 0xd3 uses 16 slots inventory round.
    txSize has to be set to accomodate enough cards.
    Response is:
    <table>
      <tr><th>   Byte</th><th>   0     </th><th>                1..8 </th><th>..</th><th>1+(num_cards*8)..8*(num_cards+1)</th></tr>
      <tr><th>Content</th><td>num_cards</td><td>flags_0,dsfid_0,uid_0</td><td>..</td><td>flags,dsfid,uid_num_cards</td></tr>
    </table>
  - #iso15693SendStayQuiet()
    <table>
      <tr><th>   Byte</th><th>   0    </th><th>  1  </th><th>2..9</th></tr>
      <tr><th>Content</th><td>0xd4(ID)</td><td>flags</td><td> UID</td></tr>
    </table>
    If SELECT flag(0x10) is set in \e flags, then UID can be omitted, operations works on selected flag.
    no response data only status
  - #iso15693SelectPicc()
    <table>
      <tr><th>   Byte</th><th>   0    </th><th>  1  </th><th>2..9</th></tr>
      <tr><th>Content</th><td>0xd5(ID)</td><td>flags</td><td> UID</td></tr>
    </table>
    If SELECT flag(0x10) is set in \e flags, then UID can be omitted, operations works on selected flag.
    no response data only status

  - #iso15693GetPiccSystemInformation()
    <table>
      <tr><th>   Byte</th><th>   0    </th><th>  1  </th><th>2..9</th></tr>
      <tr><th>Content</th><td>0xd6(ID)</td><td>flags</td><td> UID</td></tr>
    </table>
    If SELECT flag(0x10) is set in \e flags, then UID can be omitted, operations works on selected flag.
    response is:
    <table>
      <tr><th>   Byte</th><th> 0   </th><th>        1</th><th>2..9</th><th>10   </th><th>11 </th><th>12 </th><th>13 </th><th>14 </th></tr>
      <tr><th>Content</th><td>flags</td><td>infoFlags</td><td>uid </td><td>dsfid</td><td>afi</td><td>memNumBlocks</td><td>memBlockSize</td><td>icReference</td></tr>
    </table>
  - #iso15693ReadSingleBlock()
    <table>
      <tr><th>   Byte</th><th>   0    </th><th>  1  </th><th>2..9</th><th>10</th></tr>
      <tr><th>Content</th><td>0xd7(ID)</td><td>flags</td><td> UID</td><td>block_no</td></tr>
    </table>
    If SELECT flag(0x10) is set in \e flags, then UID can be omitted, operations works on selected flag.
    Response is:
    <table>
      <tr><th>   Byte</th><th>   0     </th><th>1..memBlock.actualSize+1</th></tr>
      <tr><th>Content</th><td>rec_flags</td><td>read_data               </td></tr>
    </table>
  - #iso15693WriteSingleBlock()
    <table>
      <tr><th>   Byte</th><th>   0    </th><th>  1  </th><th>2..9</th><th>   10   </th><th>11..11+blocksize</th></tr>
      <tr><th>Content</th><td>0xd8(ID)</td><td>flags</td><td> UID</td><td>block_no</td><td>   data         </td></tr>
    </table>
    If SELECT flag(0x10) is set in \e flags, then UID can be omitted, operations works on selected flag.
    No response data only status is returned.

  - #iso15693TxRxNBytes() generic sending of a byte stream, receives at most txSize
    <table>
      <tr><th>   Byte</th><th>   0    </th><th>1..rxSize</th></tr>
      <tr><th>Content</th><td>0xde(ID)</td><td>tx_buf   </td></tr>
    </table>
    Response is:
    <table>
      <tr><th>   Byte</th><th>0..txSize</th></tr>
      <tr><th>Content</th><td>read_data</td></tr>
    </table>
*/

static iso15693ProximityCard_t cards[20];
static ReturnCode processIso15693(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    const uint8_t *buf = rxData + 1;
    const uint16_t bufSize = rxSize - 1;
    uint8_t cmd = rxData[0];
    ReturnCode err = ERR_NONE;
    iso15693ProximityCard_t* cardptr;
    iso15693PiccSystemInformation_t sysInfo;
    iso15693PiccMemoryBlock_t memBlock;
    uint8_t actcnt;
    uint8_t  resFlags;
    uint16_t actLen;

    cardptr = NULL;
    if (cmd>=0xd4 && cmd<0xdf)
    {
        if (bufSize == 0) return ERR_PARAM;
        if (bufSize >= 1+ISO15693_UID_LENGTH && !(buf[0]&0x10))
        {
            ST_MEMCPY(cards[0].uid, buf + 1, ISO15693_UID_LENGTH);
            cardptr = cards + 0;
        }
    }
    switch (cmd)
    {
        case 0xd0:
            iso15693Initialize( buf[0]?true:false, buf[1]?true:false );
            return err;

        case 0xdf:
            {
                uint8_t keep_on = 0;
                if (bufSize > 0) keep_on = buf[0];
                err = iso15693Deinitialize(keep_on);
            }
            return err;

        case 0xd2:
        case 0xd3:
            err = iso15693Inventory((0xd2 == cmd) ?
                    ISO15693_NUM_SLOTS_1 :
                    ISO15693_NUM_SLOTS_16,
                    0,
                    NULL,
                    cards,
                    sizeof(cards)/sizeof(iso15693ProximityCard_t),
                    &actcnt);
#if 0
            /* This code automatically sends stay_quiet to all tags and repeats anticol */
            {
                uint8_t i, currcnt = actcnt;

                for (i = 0 ; i<actcnt ; i++)
                {
                    iso15693SendStayQuiet(cards + i);
                }
#if 1

                /* Disable this here for easy testing from GUI on tags found by additional rounds */
                while (currcnt && actcnt < sizeof(cards)/sizeof(iso15693ProximityCard_t))
                {
                    uint8_t i;
                    iso15693Inventory((0xd2 == cmd) ?
                            ISO15693_NUM_SLOTS_1 :
                            ISO15693_NUM_SLOTS_16,
                            0,
                            NULL,
                            cards + actcnt,
                            sizeof(cards)/sizeof(iso15693ProximityCard_t) - actcnt,
                            &currcnt);

                    for (i = actcnt ; i<actcnt+currcnt ; i++)
                    {
                        iso15693SendStayQuiet(cards + i);
                    }
                    actcnt += currcnt;
                }
#endif
            }
#endif
            if (actcnt > 0)
            {
                uint8_t i, *tx = txData;
                *tx = actcnt;
                tx++;

               logUsart("ISO15693/NFC-V card(s) found. Cnt: %d UID: ", actcnt);

                for (i = 0; i < actcnt; i++)
                {
                    if (*txSize < (i+1) * ISO15693_UID_LENGTH + 1)
                    {
                        i++;
                        break;
                    }
                    /* flags, dsfid, uid */
                    ST_MEMCPY(tx, &cards[i].flags, ISO15693_UID_LENGTH + 2);
                    tx += ISO15693_UID_LENGTH + 2;

                    logUsart("%s, ", hex2Str(cards[i].uid, ISO15693_UID_LENGTH));
                }
                logUsart("\n");
                *txSize = 1 + i * (ISO15693_UID_LENGTH + 2);
                err = ERR_NONE;
            }
            else
            {
                *txData = 0;
                *txSize = 1;
            }
            break;

        case 0xd4:
            if (bufSize < 9) return ERR_PARAM;
            err = iso15693SendStayQuiet(cardptr);
            break;

        case 0xd5:
            if (bufSize < 9) return ERR_PARAM;
            err = iso15693SelectPicc(cardptr);
            break;

        case 0xd6:
            if (bufSize < 8) return ERR_PARAM;
            err = iso15693GetPiccSystemInformation(cardptr, &sysInfo, &actLen);
            if (ERR_NONE == err)
            {
                if (*txSize > sizeof(sysInfo))
                    *txSize = sizeof(sysInfo);
                ST_MEMCPY(txData, &sysInfo, *txSize);
            }
            else
            {
              *txSize = 0;
            }
            break;

        case 0xd7:
            if (bufSize < ISO15693_UID_LENGTH + 2) return ERR_PARAM;
            if (*txSize < 2) return ERR_PARAM;
            memBlock.blocknr = buf[9];
            err = iso15693ReadSingleBlock(cardptr, &memBlock);
            if (ERR_NONE == err)
            {
                txData[0] = memBlock.flags;
                if (memBlock.errorCode == 0)
                {
                    if (*txSize > memBlock.actualSize + 1)
                        *txSize = memBlock.actualSize + 1;
                    ST_MEMCPY(txData+1,memBlock.data,*txSize - 1);
                }
                else
                {
                    txData[1] = memBlock.errorCode;
                    *txSize = 2;
                }
            }
            else
            {
                *txSize = 0;
            }
            break;

        case 0xd8:
            if (bufSize < ISO15693_UID_LENGTH + 3) return ERR_PARAM;
            memBlock.actualSize = bufSize - ISO15693_UID_LENGTH - 2;
            memBlock.blocknr = buf[9];
            ST_MEMCPY((uint8_t*)memBlock.data, buf + 10, memBlock.actualSize);
            err = iso15693WriteSingleBlock(cardptr, buf[0], &memBlock);
            break;

        case 0xd9:  /* Fast Read Single Block */
            if (bufSize < ISO15693_UID_LENGTH + 2) return ERR_PARAM;
            if (*txSize < 2) return ERR_PARAM;

            memBlock.blocknr = buf[9];
            err = iso15693FastReadSingleBlock(cardptr, &memBlock);
            if (ERR_NONE == err)
            {
                txData[0] = memBlock.flags;
                if (memBlock.errorCode == 0)
                {
                    if (*txSize > memBlock.actualSize + 1)
                        *txSize = memBlock.actualSize + 1;
                    ST_MEMCPY(txData+1,memBlock.data,*txSize - 1);
                }
                else
                {
                    txData[1] = memBlock.errorCode;
                    *txSize = 2;
                }
            }
            else
            {
              *txSize = 0;
            }
            break;

         case 0xda:   /* Fast Read Multiple Blocks */
            if (bufSize < ISO15693_UID_LENGTH + 3) return ERR_PARAM;
            if (*txSize < 2) return ERR_PARAM;

            err = iso15693FastReadMultipleBlocks(cardptr, buf[9], buf[10], &resFlags, &txData[1], *txSize, &actLen  );
            if (ERR_NONE == err)
            {
                txData[0] = resFlags;
                if (err == 0)
                {
                    *txSize = (actLen + 1); /* Flags + Data */
                }
                else
                {
                    txData[1] = err;
                    *txSize = 2;
                }
            }
            else
            {
              *txSize = 0;
            }
            break;

        case 0xdb:    /* Read Multiple Blocks */
            if (bufSize < ISO15693_UID_LENGTH + 3) return ERR_PARAM;
            if (*txSize < 2) return ERR_PARAM;

            err = iso15693ReadMultipleBlocks(cardptr, buf[9], buf[10], &resFlags, &txData[1], *txSize, &actLen  );
            if (ERR_NONE == err)
            {
                txData[0] = resFlags;
                if (err == 0)
                {
                    *txSize = (actLen + 1);  /* Flags + Data */
                }
                else
                {
                    txData[1] = err;
                    *txSize = 2;
                }
            }
            else
            {
              *txSize = 0;
            }
            break;

        case 0xdd:
            {
                uint16_t actlength = 0;
                uint16_t response_wait_time_ms = buf[0] | (buf[1] << 8);

                if (bufSize < 2) return ERR_PARAM;

                /* Generic command iso15693_2 command, i.e. no flag and crc handling */
                /* Relax settings of mask receive timer for protocols which answer earlier */
                rfalSetFDTListen( 1 ); /* a low value, RFAL will adapt it to the allowed minimum */
                err = rfalTransceiveBlockingTxRx( (uint8_t*)(buf + 2), (bufSize - 2), txData, *txSize, &actlength,
                                            (RFAL_TXRX_FLAGS_CRC_TX_MANUAL | RFAL_TXRX_FLAGS_CRC_RX_KEEP | RFAL_TXRX_FLAGS_NFCIP1_OFF | RFAL_TXRX_FLAGS_AGC_ON | RFAL_TXRX_FLAGS_PAR_RX_REMV | RFAL_TXRX_FLAGS_NFCV_FLAG_MANUAL),
                                             rfalConvMsTo1fc(response_wait_time_ms)  );
                *txSize = actlength;
            }
            break;

        case 0xde:
            {
                uint16_t actlength = 0;
                if (bufSize < 2) return ERR_PARAM;
                uint16_t response_wait_time_ms = buf[0] | (buf[1] << 8);
                /* Generic command */
                err = iso15693TxRxNBytes((uint8_t*)buf + 2, bufSize - 2, txData, *txSize, &actlength, response_wait_time_ms);
                *txSize = actlength;
            }
            break;

        default:
            err = ERR_REQUEST;
    }

    if (ERR_NONE == err){
      ledOnOff(LED_V, VISUAL_FEEDBACK_DELAY);
    }

    return err;
}

/*!
  Process direct commmands. Some direct commands produce a value which can be read back.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  All ST25R3911 direct commands between 0xC0 and 0xFF are executed.
  The Host->Device packets all have the same layout:

  <table>
    <tr><th>   Byte</th><th>       0</th><th>       1             </th></tr>
    <tr><th>Content</th><td>0x15(ID)</td><td>ST25R3911 direct command</td></tr>
  </table>

  Some direct commands have special handling, i.e. they return values:

  - #ST25R3911_CMD_MEASURE_AMPLITUDE 0xD3:
    Function st25r3911MeasureRF() is called which returns  the measured amplitude read from register ST25R3911_REG_AD_RESULT.
    Response is:
    <table>
      <tr><th>   Byte</th><th>         0          </th></tr>
      <tr><th>Content</th><td>ST25R3911_REG_AD_RESULT</td></tr>
    </table>
  - #ST25R3911_CMD_ADJUST_REGULATORS 0xD6:
    Clear reg_s bit and call st25r3911AdjustRegulators(). The register
    ST25R3911_REG_REGULATOR_RESULT is read and converted to uint16_t milliVolts.
    Response is:
    <table>
      <tr><th>   Byte</th><th>         0    </th><th>         0    </th></tr>
      <tr><th>Content</th><td>LSB millivolts</td><td>MSB millivolts</td></tr>
    </table>
  - #ST25R3911_CMD_CALIBRATE_MODULATION 0xD7:
    Function st25r3911CalibrateModulationDepth() is called which returns register ST25R3911_REG_AM_MOD_DEPTH_RESULT.
    Response is:
    <table>
      <tr><th>   Byte</th><th>                   0          </th></tr>
      <tr><th>Content</th><td>ST25R3911_REG_AM_MOD_DEPTH_RESULT</td></tr>
    </table>
  - #ST25R3911_CMD_CALIBRATE_ANTENNA 0xD8:
    First the bit trim_s gets cleared to make command do something.
    Function st25r3911CalibrateAntenna() gets called which returns register ST25R3911_REG_ANT_CAL_RESULT.
    Response is:
    <table>
      <tr><th>   Byte</th><th>                   0     </th></tr>
      <tr><th>Content</th><td>ST25R3911_REG_ANT_CAL_RESULT</td></tr>
    </table>
  - #ST25R3911_CMD_MEASURE_PHASE 0xD9:
    Function st25r3911MeasureAntennaResonance() is called which returns the register ST25R3911_REG_AD_RESULT.
    Response is:
    <table>
      <tr><th>   Byte</th><th>         0          </th></tr>
      <tr><th>Content</th><td>ST25R3911_REG_AD_RESULT</td></tr>
    </table>
  - #ST25R3911_CMD_CALIBRATE_C_SENSOR 0xDD:
    Response is:
    <table>
      <tr><th>   Byte</th><th>         0          </th></tr>
      <tr><th>Content</th><td>ST25R3911_REG_CAP_SENSOR_RESULT</td></tr>
    </table>
  - #ST25R3911_CMD_MEASURE_CAPACITANCE 0xDE:
    Response is:
    <table>
      <tr><th>   Byte</th><th>         0          </th></tr>
      <tr><th>Content</th><td>ST25R3911_REG_AD_RESULT</td></tr>
    </table>
  - #ST25R3911_CMD_MEASURE_VDD 0xDF :
    This command is special as it takes as additional parameter mpsv the id
    of the power supply which should be measured. If set the value is written
    into register ST25R3911_REG_REGULATOR_CONTROL_mpsv_vdd:
    <table>
      <tr><th>  0</th><th>    1</th><th>  2  </th><th>   3  </th></tr>
      <tr><th>VDD</th><td>VSP_A</td><td>VSP_D</td><td>VSP_RF</td></tr>
    </table>
    So the packet sent to the host looks like this:
    <table>
      <tr><th>   Byte</th><th>       0</th><th> 1  </th><th> 2  </th></tr>
      <tr><th>Content</th><td>0x15(ID)</td><td>0xDF</td><td>mpsv</td></tr>
    </table>
    Function st25r3911MeasureVoltage() returns the measured voltage as uint16_t milliVolts:
    <table>
      <tr><th>   Byte</th><th>         0    </th><th>         0    </th></tr>
      <tr><th>Content</th><td>LSB millivolts</td><td>MSB millivolts</td></tr>
    </table>
*/
static ReturnCode processDirectCommand(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    uint8_t cmd = rxData[0];
    ST_MEMSET(txData, 0xff, *txSize);
    ReturnCode err = ERR_NONE;

    /* direct commands */
    switch (cmd)
    {
        case ST25R3911_CMD_MEASURE_AMPLITUDE:
            rfalChipMeasureAmplitude(txData);
            if (*txSize) *txSize = 1;
            break;
        case ST25R3911_CMD_ADJUST_REGULATORS:
            { /* make sure MSB of voltage definition register is cleared */
                uint16_t mV;
                st25r3911ModifyRegister(ST25R3911_REG_REGULATOR_CONTROL, ST25R3911_REG_REGULATOR_CONTROL_reg_s, 0x0);
                st25r3911AdjustRegulators(&mV);
                txData[0] = mV & 0xff;
                txData[1] = mV >> 8;
                if (*txSize) *txSize = 2;
            }
            break;
        case ST25R3911_CMD_CALIBRATE_MODULATION:
            st25r3911CalibrateModulationDepth(txData );
            if (*txSize) *txSize = 1;
            break;
        case ST25R3911_CMD_CALIBRATE_ANTENNA:
            /* make sure MSB of external trim register is cleared */
            st25r3911ModifyRegister(ST25R3911_REG_ANT_CAL_CONTROL, ST25R3911_REG_ANT_CAL_CONTROL_trim_s, 0x0);
            if (ERR_NONE == err)
            {
                st25r3911CalibrateAntenna(txData);

            }
            if (*txSize) *txSize = 1;
            break;
        case ST25R3911_CMD_MEASURE_PHASE:
            rfalChipMeasurePhase(txData );
            if (*txSize) *txSize = 1;
            break;
        case ST25R3911_CMD_MEASURE_CAPACITANCE:
            st25r3911MeasureCapacitance(txData);
            if (*txSize) *txSize = 1;
            break;
        case ST25R3911_CMD_CALIBRATE_C_SENSOR:
            st25r3911CalibrateCapacitiveSensor(txData);
            if (*txSize) *txSize = 1;
            break;
        case ST25R3911_CMD_MEASURE_VDD:
            {
                uint16_t mV;
                uint8_t mpsv = 0;
                if (rxSize > 1)
                    mpsv = rxData[1];
                else
                {
                    st25r3911ReadRegister(ST25R3911_REG_REGULATOR_CONTROL, &mpsv);
                    mpsv &= ST25R3911_REG_REGULATOR_CONTROL_mask_mpsv;
                }

                mV = st25r3911MeasureVoltage(mpsv);
                txData[0] = mV & 0xff;
                txData[1] = mV >> 8;
                if (*txSize > 0) *txSize = 2;
            }
            break;
        default:
            st25r3911ExecuteCommand(cmd);
            *txSize = 0;
    }

    return err;
}

/*!
  Process FeliCa type commands.

  \param rxData : forward from applProcessCmd()
  \param rxSize : forward from applProcessCmd()
  \param txData : forward from applProcessCmd()
  \param txSize : forward from applProcessCmd()

  Implemented commands:

  - #felicaInitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th></tr>
      <tr><th>Content</th><td>0xf0(ID)</td></tr>
    </table>
    no response except status.
  - #felicaDeinitialize()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>         1</th></tr>
      <tr><th>Content</th><td>0xff(ID)</td><td>keep_rf_on</td></tr>
    </table>
    no return value only status
  - #felicaPoll()
    <table>
      <tr><th>   Byte</th><th>       0</th><th>       1 </th><th>   2</th><th>   3</th><th>     1</th></tr>
      <tr><th>Content</th><td>0xf1(ID)</td><td>num_slots</td><td>sys1</td><td>sys2</td><td>compar</td></tr>
    </table>
    where
    <ul>
      <li> \e num_slots : (0,1,3,7,15) the number of slots(1,2,4,8,16) to be performed
      <li> \e sys1: paramter accoring standard for POLL command.
      <li> \e sys2: paramter accoring standard for POLL command.
      <li> \e compar: paramter accoring standard for POLL command:
          <ul>
              <li> 0: no additional info requested
              <li> 1: request system code
              <li> 2: request communication information
              <li> others: reserved
           </ul>
    </ul>
    Response is:
    <table>
      <tr><th>   Byte</th><th>   0     </th><th>   1    </th><th> 2    </th><th>..</th><th> 22   </th><th>..</th><th> 42   </th><th>..</th></tr>
      <tr><th>Content</th><td>num_cards</td><td>num_cols</td><td>card 0</td><td>..</td><td>card 1</td><td>..</td><td>card 2</td><td>..</td></tr>
    </table>
    <ul>
      <li> \e num_cards : the number of cards following
      <li> \e num_cols: the number of collisions detected in responses to POLL command
      <li> \e card: felica Proximity card response according to layout of struct #felicaProximityCard.
    </ul>
    */
static ReturnCode processFeliCa(const uint8_t *rxData, uint16_t rxSize, uint8_t *txData, uint16_t *txSize)
{
    uint8_t cmd = rxData[0];
    ReturnCode err = ERR_NONE;
    const uint8_t *buf = rxData + 1;
    uint16_t bufSize = rxSize - 1;
    uint16_t numBytesReceived;

    ST_MEMSET(txData, 0xff, *txSize);

    switch (cmd)
    {
        case 0xF0:
            /* Configure demoboard for FeliCa. */
            err = felicaInitialize();
            *txSize = 0;
            return err;

        case 0xF1:
            {
            uint8_t i;
            uint8_t slots = buf[0];
            uint8_t sys1 = buf[1];
            uint8_t sys2 = buf[2];
            uint8_t compar = buf[3];
            uint8_t *num_cards, *num_cols;
            if (bufSize < 4) return ERR_PARAM;
            if (*txSize < 2) return ERR_PARAM;
            num_cards = txData;
            num_cols = txData + 1;
            *num_cards = (*txSize - 2) / sizeof(struct felicaProximityCard);
            *num_cols = 0;

            err = felicaPoll( (enum felicaSlots) slots, sys1, sys2, (enum felicaComParamRequest) compar,
                    (struct felicaProximityCard*) (txData + 2),
                    num_cards, num_cols);

            if ((*num_cards > 0)&&(ERR_NONE == err)){
              ledOnOff(LED_F, VISUAL_FEEDBACK_DELAY);
              logUsart("Felica/NFC-F card(s) found. Cnt: %d UID: ", *num_cards);
              for(i = 0; i < *num_cards; i++)
              {
                logUsart("%s, ", hex2Str(((struct felicaProximityCard*) (txData + 2 + (i*sizeof(struct felicaProximityCard))))->IDm, FELICA_MAX_ID_LENGTH));
              }
              logUsart("\n");
            }



            break;
            }

        case 0xF2:
            err =  felicaTxRxNBytes(buf, bufSize, txData, *txSize, &numBytesReceived);
            *txSize = numBytesReceived;

            if (ERR_NONE == err){
              ledOnOff(LED_F, VISUAL_FEEDBACK_DELAY);
            }
            break;

        case 0xFF:
            if (bufSize < 1) return ERR_PARAM;
            *txSize = 0;
            felicaDeinitialize(buf[0]);
            return err;

        default:
            err = ERR_REQUEST;
    }
    return err;
}

uint8_t applProcessCyclic ( uint8_t * protocol, uint16_t * txSize, uint8_t * txData, uint16_t remainingSize )
{
  if ( counter == 0 ){ /* do not log this every time : is called cyclic */
  }
  counter++;
  *txSize = 0;
  return ST_STREAM_NO_ERROR; /* cyclic is always called, so it is no error
                                   if there is no function */
}

uint8_t applReadReg ( uint16_t rxSize, const uint8_t * rxData, uint16_t *txSize, uint8_t * txData)
{
    uint8_t addr;

    if(rxSize < 1) return (uint8_t)ERR_REQUEST;

    addr = (rxData[0] & 0x3f);

    if ( (rxData[0] & 0xc0) )
    { /* virtual addresses above 0x3f provide access to test registers */
        rfalChipReadTestReg( addr, txData );
    }
    else
    {
      rfalChipReadReg( addr, txData, 1);
    }

    *txSize = 1;
    return 0;
}

uint8_t applWriteReg  ( uint16_t rxSize, const uint8_t * rxData, uint16_t *txSize, uint8_t * txData)
{
    uint8_t addr, val;

    if(rxSize < 2) return (uint8_t)ERR_REQUEST;

    addr = (rxData[0] & 0x3f);
    val  = rxData[1];

    if ( (rxData[0] & 0xc0) )
    { /* virtual addresses above 0x3f provide access to test registers */
        rfalChipWriteTestReg( addr, val );
        return 0;
    }

    rfalChipWriteReg( addr, &val, 1);
    return 0;
}

//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
// Isodep part
static int IsoDep_DeSerialize_Info( rfalIsoDepInfo * info, const uint8_t * * ptrR)
{
    int             size = 0;
    const uint8_t * start  = * ptrR; // Kepp start value
    READ_VAL08_LE( info->FWI     , uint8_t         , * ptrR);
    READ_VAL32_LE( info->FWT     , uint32_t        , * ptrR);
    READ_VAL32_LE( info->dFWT    , uint32_t        , * ptrR);
    READ_VAL32_LE( info->SFGI    , uint32_t        , * ptrR);
    READ_VAL32_LE( info->SFGT    , uint32_t        , * ptrR);
    READ_VAL08_LE( info->FSxI    , uint8_t         , * ptrR);
    READ_VAL16_LE( info->FSx     , uint16_t        , * ptrR);
    READ_VAL32_LE( info->MBL     , uint32_t        , * ptrR);
    READ_VAL08_LE( info->DSI     , rfalBitRate     , * ptrR);
    READ_VAL08_LE( info->DRI     , rfalBitRate     , * ptrR);
    READ_VAL08_LE( info->DID     , uint8_t         , * ptrR);
    READ_VAL08_LE( info->NAD     , uint8_t         , * ptrR);
    READ_VAL08_LE( info->supDID  , bool            , * ptrR);
    READ_VAL08_LE( info->supNAD  , bool            , * ptrR);
    READ_VAL08_LE( info->supAdFt , bool            , * ptrR);
    size = * ptrR - start; // Compute nb bytes read
    return(size);
}

static int IsoDep_Serialize_Info( const rfalIsoDepInfo * info, uint8_t * * ptrW)
{
    int       size = 0;
    uint8_t * start = * ptrW;
    WRITE_VAL08_LE( info->FWI     , uint8_t         , * ptrW);
    WRITE_VAL32_LE( info->FWT     , uint32_t        , * ptrW);
    WRITE_VAL32_LE( info->dFWT    , uint32_t        , * ptrW);
    WRITE_VAL32_LE( info->SFGI    , uint32_t        , * ptrW);
    WRITE_VAL32_LE( info->SFGT    , uint32_t        , * ptrW);
    WRITE_VAL08_LE( info->FSxI    , uint8_t         , * ptrW);
    WRITE_VAL16_LE( info->FSx     , uint16_t        , * ptrW);
    WRITE_VAL32_LE( info->MBL     , uint32_t        , * ptrW);
    WRITE_VAL08_LE( info->DSI     , rfalBitRate     , * ptrW);
    WRITE_VAL08_LE( info->DRI     , rfalBitRate     , * ptrW);
    WRITE_VAL08_LE( info->DID     , uint8_t         , * ptrW);
    WRITE_VAL08_LE( info->NAD     , uint8_t         , * ptrW);
    WRITE_VAL08_LE( info->supDID  , bool            , * ptrW);
    WRITE_VAL08_LE( info->supNAD  , bool            , * ptrW);
    WRITE_VAL08_LE( info->supAdFt , bool            , * ptrW);
    size = * ptrW - start;
    return(size);
}


static ReturnCode processIsoDep(const uint8_t *rxData, uint16_t rxSize, uint8_t * txData, uint16_t *txSize)
{
    uint16_t        cmd     = (uint16_t) rxData[0];
    ReturnCode      err     = ERR_NONE;
    const uint8_t * buf     = rxData + 1;
    uint16_t        BuffInSize = rxSize - 1;
    uint8_t         len8;
    const uint16_t  OutSizeMax = * txSize;
    uint16_t        OutSize = 0;
    if (cmd == ISODEP_CMD_COMMANDS)
    { // Extension
        cmd =  ((uint16_t) rxData[1]) + ISODEP_CMD_EXTENSION1;
        buf        = rxData + 2;
        BuffInSize = rxSize - 2;
    }

    if (OutSize >= sizeof(  uint32_t))
    {
        OutSize =  sizeof(  uint32_t); // At least return ErrorCode
    }

    ST_MEMSET(txData, 0xff, * txSize);
    logUsart(" processIsoDep Cmd = %d: Input=%u chars output buf size=%u", cmd, rxSize, *txSize);

    err      = ERR_PARAM; // by default
    * txSize = 0;

    switch (cmd)
    {
        case ISODEP_CMD_INITIALIZE:
            /* Configure demoboard for IsoDep. */
            rfalIsoDepInitialize();
            err     = ERR_NONE;
            OutSize = 0;
            break;
        case ISODEP_CMD_FSXI2FSX: // uint16_t   rfalIsoDepFSxI2FSx ( uint8_t fsxi)
            OutSize = 2;
            if ( (BuffInSize == 1) && ( OutSizeMax >= OutSize) )
            {
                *((uint16_t *)txData) = rfalIsoDepFSxI2FSx (buf[0]);
                err     = ERR_NONE;
            }
            break;
        case ISODEP_CMD_FWI2FWT: // uint32_t   rfalIsoDepFWI2FWT ( uint8_t fwi);
            OutSize = 4;
            if ( (BuffInSize == 1) && ( OutSizeMax >= OutSize) )
            {
                *((uint32_t *)txData) = rfalIsoDepFWI2FWT (buf[0]);
                err     = ERR_NONE;
            }
            break;
        case ISODEP_CMD_PPS:
            OutSize = sizeof(rfalIsoDepPpsRes);
            if ( (BuffInSize == 3) && ( OutSizeMax == OutSize) )
            {
                err = rfalIsoDepPPS ( buf[0] /* DID*/,  (rfalBitRate) buf[1] /*DSI*/, (rfalBitRate) buf[2] /* DRI */, (rfalIsoDepPpsRes *) (txData));
            }
            break;
        case ISODEP_CMD_RATS:
            OutSize = sizeof(uint8_t)+ sizeof(rfalIsoDepAts);
            if ( (BuffInSize == 2) && ( OutSizeMax >= OutSize) )
            {   // ReturnCode rfalIsoDepRATS ( rfalIsoDepFSxI FSDI, uint8_t DID, rfalIsoDepAts * ats, uint8_t * atsLen)
                err = rfalIsoDepRATS ( (rfalIsoDepFSxI) buf[0],  buf[1] /*DID*/, (rfalIsoDepAts *) (txData + 1), (uint8_t *) (txData ));
            }
            break;
        case ISODEP_CMD_POLLHANDLESPARAMETERS: 
            OutSize = ISODEP_DEVICE_INFO_STRUCT_SIZE;
            if ( (BuffInSize == (2U + ISODEP_DEVICE_INFO_STRUCT_SIZE) ) && ( OutSizeMax == OutSize) )
            { // Only the info part is used so only transfert in both sides this part
                rfalIsoDepDevice IsoDepDevice; // Must create an object to manage unaligned buffer
                const uint8_t *  ptrR = & buf[2];
                IsoDep_DeSerialize_Info( & IsoDepDevice.info, & ptrR);
                // ReturnCode rfalIsoDepPollHandleSParameters ( INOUT rfalIsoDepDevice * isoDepDev, rfalBitRate maxTxBR, rfalBitRate maxRxBR);
                err = rfalIsoDepPollHandleSParameters ( & IsoDepDevice,  (rfalBitRate) buf[0] /*maxTxBR*/, (rfalBitRate) buf[1] /* maxRxBR*/ );
                if (  err == 0)
                {
                    uint8_t * ptrW = txData;
                    IsoDep_Serialize_Info( & IsoDepDevice.info, & ptrW);
                }
            }
            break;
        case ISODEP_CMD_POLLBHANDLEACTIVATION:
            OutSize = ISODEP_POLL_B_STRUCT_SIZE;
            err     =  ERR_NOMEM;
            if ( (BuffInSize ==  0x14U ) /*4 + 4 + sizeof(rfalNfcbListenDevice) */
                && ( OutSizeMax == OutSize) )
            {
                const uint8_t* HLInfo = NULL;
                len8 =  * (buf + 4u + sizeof(rfalNfcbListenDevice) );  /* HLInfoLen */
                rfalIsoDepDevice   IsoDepDevice;
                if (len8 != 0)
                {
                     HLInfo = ( buf + 5 + sizeof(rfalNfcbListenDevice));  /* HLInfo */
                }
                err =  rfalIsoDepPollBHandleActivation (
                        (rfalIsoDepFSxI) buf[0] /*FSDI*/
                        , buf[1]                /*DID*/
                        , (rfalBitRate) buf[2]  /*maxBR*/
                        , buf[3]                /*PARAM1*/
                        , (rfalNfcbListenDevice *) (buf+4) /* nfcbDev*/
                        , HLInfo
                        , len8                  /* HLInfoLen */
                        , & IsoDepDevice
                        );
                uint8_t * ptr = txData;
                WRITE_VAL08_LE( IsoDepDevice.activation.B.Listener.ATTRIB_RES.mbliDid, uint8_t                      , ptr);
                WRITE_BUF     ( IsoDepDevice.activation.B.Listener.ATTRIB_RES.HLInfo , RFAL_ISODEP_ATTRIB_HLINFO_LEN, ptr);
                WRITE_VAL08_LE( IsoDepDevice.activation.B.Listener.ATTRIB_RESLen     , uint8_t                      , ptr);
                IsoDep_Serialize_Info( & IsoDepDevice.info, & ptr);
            }
            break;
        case ISODEP_CMD_INITIALIZEWITHPARAMS:
            OutSize = 0;
            if (BuffInSize == 5 )
            {
                rfalIsoDepInitializeWithParams
                    ( (rfalComplianceMode ) buf[0]
                    ,  buf[1] /*maxRetriesR*/
                    ,  buf[2] /*maxRetriesS*/
                    ,  buf[3] /*maxRetriesI*/
                    ,  buf[4] /*maxRetriesRATS*/);
                err     = ERR_NONE;
            }
            break;
        case ISODEP_CMD_ATTRIB:
            OutSize = sizeof(uint8_t)+ sizeof(rfalIsoDepAttribRes);
            len8    =    buf[RFAL_NFCB_NFCID0_LEN + 6];  // param   HLInfoLen
            if ( (BuffInSize == (12 + len8)) && ( OutSizeMax >= OutSize) )
            {
                err = rfalIsoDepATTRIB (
                                      (uint8_t *)      buf                             /* nfcid0 */
                                    ,                  buf[RFAL_NFCB_NFCID0_LEN + 0]   /*PARAM1*/
                                    , (rfalBitRate)    buf[RFAL_NFCB_NFCID0_LEN + 1]   /* DSI */
                                    , (rfalBitRate)    buf[RFAL_NFCB_NFCID0_LEN + 2]   /* DRI */
                                    , (rfalIsoDepFSxI) buf[RFAL_NFCB_NFCID0_LEN + 3]   /* FSDI */
                                    , (uint8_t)        buf[RFAL_NFCB_NFCID0_LEN + 4]   /* PARAM3 */
                                    , (uint8_t)        buf[RFAL_NFCB_NFCID0_LEN + 5]   /* DID */
                                    , (const uint8_t *)buf + (RFAL_NFCB_NFCID0_LEN + 7)/* HLInfo*/
                                    , (uint8_t)        len8                            /* HLInfoLen */
                                    , (uint8_t)        buf[RFAL_NFCB_NFCID0_LEN + 7 + len8]   /* fwt */
                                    , (rfalIsoDepAttribRes *) txData + 1   /* attribRes */
                                    , (uint8_t *)      txData              /* attribResLen */
                );
            }
            break;
        case ISODEP_CMD_STARTAPDUTRANSCEIVE:
            OutSize = 0;
            if ( (BuffInSize >= 15) && ( OutSizeMax >= OutSize) )
            {
                const char *              pDataIn = (const char *) buf;
                rfalIsoDepApduTxRxParam   ApduParam;
                READ_VAL16_LE(ApduParam.txBufLen, uint16_t           , pDataIn);
                READ_VAL32_LE(ApduParam.FWT     , uint32_t           , pDataIn);
                READ_VAL32_LE(ApduParam.dFWT    , uint32_t           , pDataIn);
                READ_VAL16_LE(ApduParam.FSx     , uint16_t           , pDataIn);
                READ_VAL16_LE(ApduParam.ourFSx  , uint16_t           , pDataIn);
                READ_VAL08_LE(ApduParam.DID     , uint8_t            , pDataIn);
                ApduParam.txBuf    = (rfalIsoDepApduBufFormat *) pDataIn;
                ApduParam.rxBuf    = & gIsoDepApduBuffer;
                ApduParam.rxLen    = & gRcvdLen;
                ApduParam.tmpBuf   = (rfalIsoDepBufFormat *) gRxBuf; //We can use this buffer!
                pDataIn +=  ApduParam.txBufLen + RFAL_ISODEP_PROLOGUE_SIZE;
                err = rfalIsoDepStartApduTransceive ( ApduParam);
                gIsoDepTransceiveOngoing = true;
            }
            break;
        case ISODEP_CMD_POLLAHANDLEACTIVATION:
            OutSize = ISODEP_POLL_A_STRUCT_SIZE;
            err     =  ERR_NOMEM;
            if ( (BuffInSize == 3) && ( OutSizeMax == OutSize) )
            {
                rfalIsoDepDevice IsoDepDevice;
                err = rfalIsoDepPollAHandleActivation
                        ( (rfalIsoDepFSxI) buf[0]       /* FSDI   */
                        , buf[1]                        /* DID    */
                        , (rfalBitRate) buf[2]          /* maxBR  */
                        , & IsoDepDevice);
                uint8_t * ptr = txData;
                WRITE_VAL08_LE( IsoDepDevice.activation.A.Listener.ATS.TL, uint8_t      , ptr);
                WRITE_VAL08_LE( IsoDepDevice.activation.A.Listener.ATS.T0, uint8_t      , ptr);
                WRITE_VAL08_LE( IsoDepDevice.activation.A.Listener.ATS.TA, uint8_t      , ptr);
                WRITE_VAL08_LE( IsoDepDevice.activation.A.Listener.ATS.TB, uint8_t      , ptr);
                WRITE_VAL08_LE( IsoDepDevice.activation.A.Listener.ATS.TC, uint8_t      , ptr);
                WRITE_BUF     ( IsoDepDevice.activation.A.Listener.ATS.HB, RFAL_ISODEP_ATS_HB_MAX_LEN, ptr);
                IsoDep_Serialize_Info( & IsoDepDevice.info, & ptr);
            }
            if (err != 0)
            {
                OutSize = 0;
            }
            break;
        case ISODEP_CMD_DESELECT:
            OutSize = 0;
            if ( (BuffInSize == 0) && ( OutSizeMax >= OutSize) )
            {
                err = rfalIsoDepDeselect();
            }
            break;
#if RFAL_FEATURE_ISO_DEP_LISTEN
        case ISODEP_CMD_LISTENGETACTIVATIONSTATUS:
            OutSize = 0;
            if ( (BuffInSize == 0) && ( OutSizeMax >= OutSize) )
            {
                err = rfalIsoDepListenGetActivationStatus();
            }
            break;
        case ISODEP_CMD_ISATTRIB:
            OutSize = 1;
            len8    =  buf[0];
            if ( (BuffInSize >= 1 ) && ( BuffInSize == len8 + 1) )
            {
                * txData = rfalIsoDepIsAttrib ( buf+1,  buf[0] /*bufLen*/);
                err     = ERR_NONE;
            }
            break;
        case ISODEP_CMD_ISRATS:
            len8 =  buf[0];
            if ( (BuffInSize >= 1 ) && ( BuffInSize == len8 + 1) )
            {
                OutSize = 1;
                * txData = rfalIsoDepIsRats ( buf+1,  buf[0] /*bufLen*/);
                err      = ERR_NONE;
            }
            break;
#endif //  RFAL_FEATURE_ISO_DEP_LISTEN
        case ISODEP_CMD_GETAPDUTRANSCEIVESTATUS:
            OutSize = 0;
            // err     = rfalIsoDepGetApduTransceiveStatus();
            err     = gIsoDepTransceiveError;
            // platformLog("\nrfalIsoDepStartApduTransceive Timer %d. Status = %d rcvLen=%d",  timerCalculateTimer( 0), * pErrorCode , gRcvdLen);
            if (err == ERR_NONE)
            {
                uint16_t len = ((uint16_t) gRcvdLen) + RFAL_ISODEP_PROLOGUE_SIZE;
                OutSize =  sizeof(uint16_t) + len;
                if (OutSize <= OutSizeMax)
                {
                    uint8_t * ptr = txData;
                    WRITE_VAL16_LE( gRcvdLen           , uint16_t      , ptr);
                    WRITE_BUF     ( & gIsoDepApduBuffer, len           , ptr);
                }
                else
                {
                    err     = ERR_NOMEM;
                }
            }
            break;
        case ISODEP_CMD_GETTRANSCEIVESTATUS:
            OutSize = 0;
            if ( (BuffInSize == 0) && ( OutSizeMax >= OutSize) )
            {
                err = rfalIsoDepGetTransceiveStatus ();
            }
            break;
        case ISODEP_CMD_GETMAXINFLEN:
            OutSize = 2;
            if ( (BuffInSize == 0) && ( OutSizeMax >= OutSize) )
            {
                uint16_t res = rfalIsoDepGetMaxInfLen ();
                WRITE_VAL16_LE( res, uint16_t      , txData);
                err     = ERR_NONE;
            }
            break;
        default: // non implemented Isodep commands
            err     = ERR_NOT_IMPLEMENTED;;
            break;
    }
    if ( err != ERR_NONE)
    {
        * txSize = 0;
    }
    else
    {
        if (OutSize <= OutSizeMax)
        {
            * txSize = OutSize;
        }
        else
        {
            err      = ERR_PARAM;
            * txSize = 0;
        }
    }
    return err;
}

/*
ack                 ( rfalNfcDepDeactCallback pFunc );

*/


//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
//***************************************************************************************
const uint32_t firmwareNumber = 0x010206;

const char * applFirmwareInformation(void)
{
    return "ST25R3911B-DISCO Firmware v1.2.6";
}
