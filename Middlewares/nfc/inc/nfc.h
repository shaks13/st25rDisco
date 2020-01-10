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
 *  \author Oliver Regenfelder
 *
 *  \brief Implementation of basic commands for the NFC-P2P stack implemented
 *  in the GUI.
 *
 * NFC-P2P defines two roles: the initiator role and the target role. One device of a P2P
 * communication must act as an initiator and the other device must act as a target. This
 * module implements just the time critical parts of the NFC-P2P protocol. These parts are
 * then used by the GUI to implement the NFC-IP1 stack in active target and active initiator
 * role. The following section describes how the functions provided by this module must
 * be used to implement the initiator and target role respectively.
 *
 * \section InitiatorRole Initiator Role
 *
 * At first nfcInitialize() must be called with appropriate paramters to configure the nfc
 * module for the active initiator role and the desired bitrate. This bitrate is used for
 * the ATR_REQ (attribute request) command and any subsequent communication.
 *
 * After the module has been initialized an ATR_REQ can be send at any time using the
 * function nfcTxNBytes() with \a perform_collision_avoidance set to \c true.
 * As the reader field is not activate on the first call to nfcTxNbytes() an initial response
 * collision is performed before the ATR_REQ command is send. Afterwards nfcRxNBytes() can be used
 * to observe the status of the peers answer. This function is non blocking and will return
 * immediately with \c ERR_BUSY to notify the caller that a reception is still in progress,
 * or it will store the received message in the given buffer and report the error status of the
 * received message. If a response has indeed been received from the peer device, then a
 * response collision avoidance will already have been performed before the received message
 * is returned.
 *
 * From there on communcition with the peer device continues with calls to nfcTxNBytes() with
 * \a perform_collision_avoidance set to \c TRUE to send a message and calls to nfcRxNBytes() to
 * receive the response.
 *
 * On the last message transmission the parameter \a perform_collision_avoidance of nfcTxNBytes()
 * has to be set to \a false. So that no response collision avoidance is performed after the
 * peer device acknowledges its deselction/deactivation.
 *
 * If another peer device activation using the initiator role is required, then a new communication
 * can start directly with a call to nfcTxNBytes() - the call to nfcInitialize() can be skipped.
 *
 * \section TargetRole Target Role
 *
 * At first nfcInitialize() must be called with appropriate parameters to configure the nfc
 * module for the active target role.
 * Then nfcStartInitialTargetRx() must be called. This function initializes the low power target
 * bitrate detection mode of the ST25R3911 and enables reception of an initiator message. Afterwards
 * calls to the non blocking nfcRxNBytes() can
 * be used to check whether a message from an initiator has been received or not. If nfcRxNBytes()
 * returns \c ERR_BUSY, then no message has been received yet. If it returns any other error status, then
 * a message has been received and the content of that message is stored in the buffers passed to nfcRxNBytes().
 * From there on communication is performed via nfcTxNBytes() and nfcRxNBytes() as in the
 * initiator role. Once communication with the peer device is completed a call to nfcStartInitialTargetRx()
 * is required to reenter the low power target bitrate detection mode and wait for the next activation
 * from an initiator.
 *
 * Also, if the first message received is not an ATR_REQ, then nfcStartInitialTargetRx() must
 * be called to go back to the low power target bitrate detection mode and wait for the next potential
 * initiator message.
 */
/*!
 * 
 */

#ifndef NFC_H
#define NFC_H

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
 *  \brief Initialize the NFC mode.
 *  \note This function needs to be called every time after switching
 *  from a different mode.
 *
 *  \warning For Target mode nfcDepTargetSetParams() must be called right 
 *           after to configure Target's parameters 
 *
 *  \param[in] is_active : whether it should be active or passive mode
 *  \param[in] is_initiator : whether we want to be initiator or target
 *  \param[in] bitrate : target bitrate: 2^bitrate * 106kBit/s
 *  \param[in] listenModeMask : listen modes to be accepted as Target 
 * 
 *  \return ERR_IO : Error during communication.
 *  \return ERR_PARAM : Selected bitrate or \a is_active / \a is_initiator combination
 *  not supported.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode nfcInitialize(uint8_t is_active, uint8_t is_initiator, uint8_t bitrate, uint8_t listenModeMask);

/*!
 *****************************************************************************
 *  \brief Deinitialize the NFC mode.
 *  
 *  \note This function should be called every time NFC is not needed
 *  any more.
 *  
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode nfcDeinitialize(void);

/*!
 *****************************************************************************
 *  \brief Set the transmit bitrate.
 *  
 *  \param[in] bitrate : Transmit bitrate: 2^bitrate * 106kBit/s
 *  
 *  \return ERR_PARAM : Invalid \a bitrate value.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
ReturnCode nfcSetTxBitrate(uint8_t bitrate);

/*!
 *****************************************************************************
 *  \brief Set the receive bitrate.
 *  
 *  \param[in] bitrate : Receive bitrate: 2^bitrate * 106kBit/s
 *  
 *  \return ERR_PARAM : Invalid \a bitrate value.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
ReturnCode nfcSetRxBitrate(uint8_t bitrate);

/*!
 *****************************************************************************
 *  \brief  Transfer a given number of bytes via NFC.
 *
 *  Send \a bufSize bytes from \a buf to a P2P partner device. If the RF field
 *  of the ST25R3911 is off, then an initial RF collision avoidance will be performed
 *  prior to the data transmission.
 *
 *  \param[in] buf : Buffer to be transmitted.
 *  \param[in] bufSize : Number of bytes to be transmitted (= length of \a buf).
 *
 *  \return ERR_RF_COLLISION: Initial RF collision avoidance failed, due to
 *  presence of another RF field.
 *  \return ERR_INTERNAL : Timeout while waiting for CAT or CAC interrupt.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode nfcTxNBytes(const uint8_t *buf, uint16_t bufSize);

/*!
 *****************************************************************************
 *  \brief Check NFC data reception status.
 *
 *  Check the status of an ongoing NFC data reception. If the data reception is
 *  completed then the received data will be stored in \a buf.
 *
 * \param[out] bitrate : Set to the bitrate at which the frame was received.
 *  \param[out] buf : Pointer to memory area where received data will be stored.
 *  \param[in] bufsize : Max. number of bytes to receive (= length of \a buf).
 *  \param[out] actlen : Actual number of bytes received.
 *
 *  \return ERR_TIMEOUT : The P2P partner device has not performed its response
 *  RF collision avoidance within the required timeframe, or the response
 *  RF collision avoidance has been performed but not data has been send.
 *  \return ERR_RF_COLLISION: Response RF collision avoidance after the
 *    reception failed. \a buf may still contain valid data.
 *  \return ERR_INTERNAL : Timeout while waiting for the CAT, CAC, or OSC interrupt.
 *  \return ERR_BUSY : Reception in progress.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode nfcRxNBytes(uint8_t *bitrate, uint8_t *buf, uint16_t bufsize, uint16_t *actlen);

/*!
 *****************************************************************************
 *  \brief Transmit a given Data via NFC-DEP protocol.
 *
 *  This will transmit the given Data through NFC-DEP protocol, performing if
 *  required the necessary retransmissions and error handling.
 *  NFC-DEP header will be prepended to the given data before transmission.
 *  
 *  
 *  \param[in] buf : Buffer/Data to be transmitted.
 *  \param[in] bufSize : Number of bytes to be transmitted (= length of \a buf).
 *
 *  \return ERR_IO: Error during communication
 *  \return ERR_NONE : No error.
 *****************************************************************************
 */
extern ReturnCode nfcDepTx( const uint8_t *buf, uint16_t bufSize );

/*!
 *****************************************************************************
 *  \brief Receive Data via NFC-DEP protocol.
 *
 *  Checks the status of an ongoing NFC-DEP data reception. If the data 
 *  reception is completed then the received data will be stored in \a buf.
 *  The NFC-DEP header will be removed before returning to the caller
 *
 *  \param[out] buf    : Pointer to memory area where received Data will be stored.
 *  \param[in] bufsize : Max. number of bytes to receive (= length of \a buf).
 *  \param[out] actlen : Actual number of bytes received.
 *
 *  \return ERR_TIMEOUT : The P2P partner device has not performed its response
 *  RF collision avoidance within the required timeframe, or the response
 *  RF collision avoidance has been performed but not data has been send.
 *  \return ERR_RF_COLLISION: Response RF collision avoidance after the
 *    reception failed. \a buf may still contain valid data.
 *  \return ERR_INTERNAL : Timeout while waiting for the CAT, CAC, or OSC interrupt.
 *  \return ERR_BUSY : Reception in progress.
 *  \return ERR_IO : Error during communication.
 *  \return ERR_NONE : No error.
 *
 *****************************************************************************
 */
extern ReturnCode nfcDepRx( uint8_t *buf, uint16_t bufsize, uint16_t *actlen );

/*!
 *****************************************************************************
 *  \brief NFC-DEP Initiator Handle Activation
 *
 *  This will perform the whole NFC-DEP Initiator activation, by sending 
 *  the ATR_REQ and if necessary will also send a PSL_REQ to change the 
 *  communication parameters.
 *  
 *  
 *  \param[in]  nfcid3 : NFCID to be used on the ATR_REQ
 *  \param[in]  desiredBitRate : the bitrate that should be used for the 
 *                              following communications (if supported 
 *                              by both devices)
 *  \param[in]  gbLen :  General bytes length
 *  \param[in]  gb    :  General bytes
 *  \param[out] currentBitRate :  actual bitrate after activation
 *  \param[out] atrResLen :  received ATR_RES length
 *  \param[out] atrRes received ATR_RES                   
 *
 *  \return ERR_IO: Error during communication
 *  \return ERR_NONE : No error.
 *****************************************************************************
 */
extern ReturnCode nfcDepInitiatorHandleActivation( uint8_t* nfcid3, uint8_t desiredBitRate, uint8_t gbLen, uint8_t* gb, uint8_t* currentBitRate, uint8_t* atrResLen, uint8_t *atrRes );

/*!
 *****************************************************************************
 *  \brief NFC-DEP ATR_REQ
 *
 *  This Initiator method will send an ATR_REQ and wait for a valid 
 *  ATR_RES.
 *  
 *  \param[in]  nfcid3         : MFCID to be used on the ATR_REQ
 *  \param[in]  gbLen          :  General bytes length
 *  \param[in]  gb             :  General bytes
 *  \param[out] atrResLen      :  received ATR_RES length
 *  \param[out] atrRes         : received ATR_RES                   
 *
 *  \return ERR_IO     : Error during communication
 *  \return ERR_PARAM  : Invalid parameter
 *  \return ERR_TIMEOUT: A timeout error occurred 
 *  \return ERR_PROTO  : Protocol error occurred
 *  \return ERR_NONE   : No error.
 *****************************************************************************
 */
extern ReturnCode nfcDepATR( uint8_t* nfcid3, uint8_t gbLen, uint8_t* gb, uint8_t* atrResLen, uint8_t *atrRes );

/*!
 *****************************************************************************
 *  \brief NFC-DEP PSL_REQ
 *
 *  This Initiator method will send an PSL_REQ and wait for a valid 
 *  PSL_RES.
 *  
 *  \param[in]  txBR : transmission bitrate to be used
 *  \param[in]  rxBR : reception bitrate to be used
 *  \param[in]  lr   : length reduction (max frame size) to be used         
 *
 *  \return ERR_IO     : Error during communication
 *  \return ERR_PARAM  : Invalid parameter
 *  \return ERR_TIMEOUT: A timeout error occurred 
 *  \return ERR_PROTO  : Protocol error occurred
 *  \return ERR_NONE   : No error.
 *****************************************************************************
 */
extern ReturnCode nfcDepPSL( uint8_t txBR, uint8_t rxBR, uint8_t lr );

/*!
 *****************************************************************************
 *  \brief NFC-DEP Deselect
 *
 *  This Initiator method will send an Deselect (DSL_REQ) command and wait
 *  for a valid response (DSL_RES).
 *
 *  \return ERR_IO     : Error during communication
 *  \return ERR_PARAM  : Invalid parameter
 *  \return ERR_TIMEOUT: A timeout error occurred 
 *  \return ERR_PROTO  : Protocol error occurred
 *  \return ERR_NONE   : No error.
 *****************************************************************************
 */
extern ReturnCode nfcDepDeselect( void );

/*!
 *****************************************************************************
 *  \brief NFC-DEP Release
 *
 *  This Initiator method will send an Release (RLS_REQ) command and wait
 *  for a valid response (RLS_RES).
 *
 *  \return ERR_IO     : Error during communication
 *  \return ERR_PARAM  : Invalid parameter
 *  \return ERR_TIMEOUT: A timeout error occurred 
 *  \return ERR_PROTO  : Protocol error occurred
 *  \return ERR_NONE   : No error.
 *****************************************************************************
 */
extern ReturnCode nfcDepRelease( void );

/*!
 *****************************************************************************
 *  \brief NFC-DEP Set Target Parameters
 *
 *  This Target method will set all the necessary parameters for the  
 *  NFC-DEP protocol layer to handle Target activation 
 *  
 *  \warning Must be called/configured before performing nfcInitialize() 
 *           into Target mode
 *  
 *  \param[in]  nfcid : NFCID to be used on the ATR_RES
 *  \param[in]  lr    : length reduction (max frame size) to be announced
 *  \param[in]  gbLen :  General bytes length
 *  \param[in]  gb    :  General bytes
 *
 *****************************************************************************
 */
extern void nfcDepTargetSetParams( uint8_t *nfcid, uint8_t lr, uint8_t gbLen, uint8_t *gb );


/*!
 *****************************************************************************
 *  \brief NFC-DEP Target handle activation
 *
 *  This Target method will handle the target activation procedure and
 *  report its outcome.
 *  
 *  The activation handling is triggered when nfcInitialize() is set to 
 *  Target mode.
 *  
 *  \param[out] bitrate  : current bitrate used
 *  \param[out] isActive : target's communication mode
 *  \param[out] atrReqLen: received ATR_REQ length
 *  \param[out] atrReq   : received ATR_REQ
 *
 *  \return ERR_IO     : Error during communication
 *  \return ERR_PARAM  : Invalid parameter
 *  \return ERR_BUSY   : Activation not finished
 *  \return ERR_TIMEOUT: A timeout error occurred 
 *  \return ERR_PROTO  : Protocol error occurred
 *  \return ERR_NONE   : No error.
 *****************************************************************************
 */
extern ReturnCode NfcDepTargetHandleActivation( uint8_t *bitrate, bool *isActive, uint8_t *atrReqLen, uint8_t *atrReq );


#endif /* NFC_H */
