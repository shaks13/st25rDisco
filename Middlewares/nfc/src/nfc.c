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
 *  \brief Implementation of NFCIP-1
 *
 */

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include <string.h>
#include "platform.h"
#include "nfc.h"
#include "utils.h"
#include "rfal_rf.h"
#include "utils.h"
#include "rfal_nfcDep.h"
#include "rfal_nfca.h"
#include "rfal_nfcf.h"


/*
******************************************************************************
* LOCAL MACROS
******************************************************************************
*/

/*! Sanity timeout for initial RF collision avoidance completed interrupt (milliseconds). */
#define NFC_INITIAL_RFCA_IRQ_TIMEOUT    10
/*! Sanity timeout for response RF collision avoidance completed interrupt (milliseconds). */
#define NFC_RESPONSE_RFCA_IRQ_TIMEOUT     1

/*! Size of the nfc module receive buffer (bytes).
 *   1 byte:  SOD (0xF0) marker for 106 kBit/s frames this is stored in the FIFO by v1 silicon.
 * 255 bytes: maximum size für the length byte + payload
 */
#define NFC_RX_BUFFER_SIZE  256

/*! Response RF collision avoidance timeout is 8192 carrier cycles. */
#define RESPONSE_RFCA_64fcs (8192/64)

/*! Timeout is for receive new data is ~2400 milliseconds */
#define TIMEOUT_DATA_64fcs  rfalConvMsTo1fc(2400)

/*
******************************************************************************
* LOCAL VARIABLES
******************************************************************************
*/


/*! Indicates whether the we are operating in NFCIP1 active or passive mode */
static bool nfcIsActive;
/*! Stores the response from the P2P partner. */
static uint8_t nfcRxBuffer[NFC_RX_BUFFER_SIZE];
/*! Points to one byte after the last valid byte in the receive buffer. */
static uint16_t nfcRxBufferActLength;
/*! Error status of the last data reception. */
static uint8_t nfcRxBitrate;

/*! Listen Mode states used during Activation*/
enum
{
    NFC_LISTEN_NONE,
    NFC_LISTEN_WAIT_ATR,
    NFC_LISTEN_WAIT_ACTIVATION
} nfcListenState;


rfalTransceiveContext     nfcTransceiveCtx;
rfalNfcDepTxRxParam       rfalNfcDepTxRx;
rfalNfcDepBufFormat       nfcDepRxBuf;
rfalNfcDepBufFormat       nfcDepTxBuf;
bool                      isRxChaining;
rfalNfcDepTargetParam     nfcDepTagetParam;
rfalNfcDepListenActvParam actvParam;
rfalNfcDepDevice          nfcDepDev;
rfalLmConfPA              lmConfA;
rfalLmConfPF              lmConfF;
uint8_t                   nfcLmState;
uint8_t                   nfcid[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
uint8_t                   sensfRes[] = {RFAL_NFCF_CMD_POLLING_RES, RFAL_NFCF_SENSF_NFCID2_BYTE1_NFCDEP, RFAL_NFCF_SENSF_NFCID2_BYTE2_NFCDEP, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


/*
******************************************************************************
* LOCAL PROTOTYPES
******************************************************************************
*/
bool nfcDeactivateCheck( void );

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/

ReturnCode nfcInitialize(uint8_t is_active, uint8_t is_initiator, uint8_t bitrate, uint8_t listenModeMask)
{
    ReturnCode err = ERR_NONE;
    
    nfcIsActive  = !!is_active;
    nfcRxBitrate = bitrate;
    
    rfalNfcDepInitialize();
    
    /* Initialize the default RFAL RF transceive context */
    nfcTransceiveCtx.rxBuf = nfcRxBuffer;
    nfcTransceiveCtx.rxBufLen = rfalConvBytesToBits(sizeof(nfcRxBuffer));
    nfcTransceiveCtx.rxRcvdLen = &nfcRxBufferActLength;
    nfcTransceiveCtx.flags = RFAL_TXRX_FLAGS_CRC_TX_AUTO | RFAL_TXRX_FLAGS_CRC_RX_REMV | RFAL_TXRX_FLAGS_NFCIP1_ON | RFAL_TXRX_FLAGS_AGC_ON | RFAL_TXRX_FLAGS_PAR_RX_REMV;
    
    
    /* Initialize the NFC-DEP protocol transceive context */
    rfalNfcDepTxRx.rxBuf        = &nfcDepRxBuf;
    rfalNfcDepTxRx.rxLen        = &nfcRxBufferActLength;
    rfalNfcDepTxRx.DID          = RFAL_NFCDEP_DID_NO;
    rfalNfcDepTxRx.FSx          = RFAL_NFCDEP_FRAME_SIZE_MAX_LEN;
    rfalNfcDepTxRx.FWT          = rfalNfcDepCalculateRWT( RFAL_NFCDEP_WT_INI_DEFAULT );
    rfalNfcDepTxRx.dFWT         = rfalNfcDepCalculateRWT( RFAL_NFCDEP_WT_DELTA );
    rfalNfcDepTxRx.isRxChaining = &isRxChaining;
    
    
    /* Initialize the NFC-DEP Listen Mode activation parameters */
    actvParam.isRxChaining      = &isRxChaining;
    actvParam.nfcDepDev         = &nfcDepDev;
    actvParam.rxBuf             = &nfcDepRxBuf;
    actvParam.rxLen             = &nfcRxBufferActLength;
    
    /* Initialize the NFC-DEP Target parameters with default values, updated on nfcDepTargetSetParams() */
    nfcDepTagetParam.commMode   = ((is_active) ? RFAL_NFCDEP_COMM_ACTIVE : RFAL_NFCDEP_COMM_PASSIVE);
    nfcDepTagetParam.operParam  = (RFAL_NFCDEP_OPER_FULL_MI_DIS | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS | RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);
    nfcDepTagetParam.bst        = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    nfcDepTagetParam.brt        = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    nfcDepTagetParam.to         = RFAL_NFCDEP_WT_TRG_MAX_D11;
    nfcDepTagetParam.ppt        = rfalNfcDepLR2PP(RFAL_NFCDEP_LR_254);
    nfcDepTagetParam.GBtLen     = 0;
    ST_MEMSET( nfcDepTagetParam.nfcid3, 0x00, RFAL_NFCDEP_NFCID3_LEN );
    
        
    /* Initialize the  RFAL RF Listen Mode parameters */
    lmConfA.SENS_RES[0] = 0x41;                 /* Double size NFCID and SDD  */
    lmConfA.SENS_RES[1] = 0x00;                 /* Other than T1T             */
    lmConfA.SEL_RES     = 0x40;                 /* Configured for the NFC-DEP Protocol */
    lmConfA.nfcidLen    = RFAL_LM_NFCID_LEN_07; /* Double size NFCID          */
    ST_MEMCPY(lmConfA.nfcid, nfcid, RFAL_NFCID2_LEN);
    
    lmConfF.SC[0] = (uint8_t)(RFAL_NFCF_SYSTEMCODE);     /* The NFC-F Listen device MUST be ready to receive a SC equal to FFFFh */
    lmConfF.SC[1] = (uint8_t)(RFAL_NFCF_SYSTEMCODE>>8);
    ST_MEMCPY( lmConfF.SENSF_RES, sensfRes, sizeof(sensfRes) );
    
    
    if (is_initiator)
    {
        nfcTransceiveCtx.fwt = TIMEOUT_DATA_64fcs;
        
        if( !nfcIsActive )
        {
            switch (bitrate)
            {
                case 0:
                    err = rfalSetMode(RFAL_MODE_POLL_NFCA, (rfalBitRate) bitrate, (rfalBitRate) bitrate);
                    break;
                case 1:
                    err = rfalSetMode(RFAL_MODE_POLL_NFCF, (rfalBitRate) bitrate, (rfalBitRate) bitrate);
                    break;
                case 2:
                    err = rfalSetMode(RFAL_MODE_POLL_NFCF, (rfalBitRate) bitrate, (rfalBitRate) bitrate);
                    break;
                default:
                    return ERR_PARAM;
            }
        }
        else
        {
            err = rfalSetMode(RFAL_MODE_POLL_ACTIVE_P2P, (rfalBitRate) bitrate, (rfalBitRate) bitrate);
            rfalSetGT(rfalConvMsTo1fc( 30 ));
            rfalFieldOnAndStartGT();
        }
    }
    else
    {
        err = rfalListenStart( (((uint32_t)listenModeMask) << RFAL_MODE_LISTEN_NFCA), &lmConfA, NULL, &lmConfF,  nfcRxBuffer, rfalConvBytesToBits(sizeof(nfcRxBuffer)), &nfcRxBufferActLength );
        nfcLmState = ((err == ERR_NONE) ? NFC_LISTEN_WAIT_ATR : NFC_LISTEN_NONE);
    }


    return ( (ERR_NONE != err) ? ERR_IO : ERR_NONE);
}

ReturnCode nfcDeinitialize()
{
    nfcLmState = NFC_LISTEN_NONE;
    rfalListenStop();
    
    return rfalFieldOff();
}

ReturnCode nfcSetTxBitrate(uint8_t bitrate)
{
    ReturnCode err = ERR_NONE;
    rfalBitRate txBR, rxBR;

    if (bitrate > RFAL_BR_424)
    {
        return ERR_PARAM;
    }
        
    rfalGetBitRate( &txBR, &rxBR );

    if (!nfcIsActive)
    {
        if (RFAL_BR_106 == bitrate)
        {
            err = rfalSetMode(RFAL_MODE_POLL_NFCA, (rfalBitRate) bitrate, rxBR);
        }
        else
        {
            err = rfalSetMode(RFAL_MODE_POLL_NFCF, (rfalBitRate) bitrate, rxBR);
        }
    }
    else
    {
        err = rfalSetBitRate( (rfalBitRate) bitrate, RFAL_BR_KEEP);
    }

    return ( (ERR_NONE != err) ? ERR_IO : ERR_NONE);
}

ReturnCode nfcSetRxBitrate(uint8_t bitrate)
{
    ReturnCode err = ERR_NONE;
    rfalBitRate txBR, rxBR;

    if (bitrate > RFAL_BR_424)
    {
        return ERR_PARAM;
    }
        
    rfalGetBitRate( &txBR, &rxBR );

    if (!nfcIsActive)
    {
        if (RFAL_BR_106 == bitrate)
        {
            err = rfalSetMode( RFAL_MODE_POLL_NFCA, (rfalBitRate) txBR, (rfalBitRate) bitrate);
        }
        else
        {
            err = rfalSetMode( RFAL_MODE_POLL_NFCF, (rfalBitRate) txBR, (rfalBitRate) bitrate);
        }
    }
    else
    {
        err = rfalSetBitRate( RFAL_BR_KEEP, (rfalBitRate) bitrate);
    }

    nfcRxBitrate = bitrate;

    return ( (ERR_NONE != err) ? ERR_IO : ERR_NONE);
}

ReturnCode nfcTxNBytes(const uint8_t *buf, uint16_t bufSize)
{
    /* rfalTransceiveBlockingTx() already creates the TxRx context (in bytes) *
       Updating global nfcTransceiveCtx just for consistency                  */
    nfcTransceiveCtx.txBuf    = (uint8_t*)buf;
    nfcTransceiveCtx.txBufLen = rfalConvBytesToBits(bufSize);
    nfcRxBufferActLength = 0;
        
    return rfalTransceiveBlockingTx( nfcTransceiveCtx.txBuf, bufSize, nfcTransceiveCtx.rxBuf, nfcTransceiveCtx.rxBufLen, &nfcRxBufferActLength, nfcTransceiveCtx.flags, nfcTransceiveCtx.fwt);
}

ReturnCode nfcRxNBytes(uint8_t *bitrate, uint8_t *buf, uint16_t bufsize, uint16_t *actlen)
{
    ReturnCode  ret;
   
    *actlen = 0;

    ret = rfalGetTransceiveStatus();
    if( ret != ERR_BUSY )
    {
        if (rfalConvBitsToBytes(nfcRxBufferActLength) > bufsize)
        {
            return ERR_NOMEM;
        }

        if (0 == nfcRxBufferActLength)
        {
          return ERR_NOMSG;
        }

        *bitrate = nfcRxBitrate;

        *actlen = rfalConvBitsToBytes(nfcRxBufferActLength);
        nfcRxBufferActLength = 0;

        memcpy(buf, nfcRxBuffer, *actlen);
    }
    
    return ret;
}


ReturnCode nfcDepTx( const uint8_t *buf, uint16_t bufSize )
{
  
    /* Set Deactivate Callback */
    rfalNfcDepSetDeactivatingCallback( (rfalNfcDepDeactCallback) nfcDeactivateCheck );
  
    /* Copy data to send */
    ST_MEMCPY( nfcDepTxBuf.inf, buf, MIN( bufSize, RFAL_NFCDEP_FRAME_SIZE_MAX_LEN ) );
    
    rfalNfcDepTxRx.txBuf        = &nfcDepTxBuf;
    rfalNfcDepTxRx.txBufLen     = bufSize;
    rfalNfcDepTxRx.isTxChaining = false;
    
    return rfalNfcDepStartTransceive( &rfalNfcDepTxRx );
}


ReturnCode nfcDepRx( uint8_t *buf, uint16_t bufsize, uint16_t *actlen )
{
    ReturnCode ret;
    
    *actlen = 0;
    
    ret = rfalNfcDepGetTransceiveStatus();
    switch( ret )
    {
        case ERR_AGAIN:
        case ERR_NONE:
            *actlen = MIN( *rfalNfcDepTxRx.rxLen, bufsize );
            ST_MEMCPY( buf, &rfalNfcDepTxRx.rxBuf->inf, *actlen );
            break;
            
        default:
            break;
    }
    
    return ret;
}

ReturnCode nfcDepInitiatorHandleActivation( uint8_t* nfcid3, uint8_t desiredBitRate, uint8_t gbLen, uint8_t* gb, uint8_t* currentBitRate, uint8_t* atrResLen, uint8_t *atrRes )
{
    rfalNfcDepAtrParam   param;
    ReturnCode           ret;
    
    /* Set NFC-DEP Initiator Activation parameters */
    param.nfcid    = nfcid3;
    param.nfcidLen = RFAL_NFCDEP_NFCID3_LEN;
    param.BS       = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    param.BR       = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    
    param.DID      = RFAL_NFCDEP_DID_NO;
    param.NAD      = RFAL_NFCDEP_NAD_NO;    
    param.LR       = RFAL_NFCDEP_LR_254;
    
    param.GBLen    = gbLen;
    param.GB       = gb;
    
    param.commMode  = ((nfcIsActive) ? RFAL_NFCDEP_COMM_ACTIVE : RFAL_NFCDEP_COMM_PASSIVE);
    param.operParam = (RFAL_NFCDEP_OPER_FULL_MI_DIS | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS | RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);
    
    /* Perform Initiator Activation */
    ret = rfalNfcDepInitiatorHandleActivation( &param, (rfalBitRate) desiredBitRate, &nfcDepDev );
    if( ret == ERR_NONE )
    {
        rfalNfcDepTxRx.DID  = nfcDepDev.info.DID;
        rfalNfcDepTxRx.FSx  = nfcDepDev.info.FS;
        rfalNfcDepTxRx.FWT  = nfcDepDev.info.FWT;
        rfalNfcDepTxRx.dFWT = nfcDepDev.info.dFWT;
        
        *currentBitRate  = nfcDepDev.info.DSI;  
        *atrResLen       = nfcDepDev.activation.Target.ATR_RESLen;
        ST_MEMCPY(atrRes, &nfcDepDev.activation.Target.ATR_RES, nfcDepDev.activation.Target.ATR_RESLen);
    }
    
    return ret;
}


ReturnCode nfcDepATR( uint8_t* nfcid3, uint8_t gbLen, uint8_t* gb, uint8_t* atrResLen, uint8_t *atrRes )
{
    rfalNfcDepAtrParam   param;
    ReturnCode           ret;
    
    /* Compute ATR parameters */
    param.nfcid    = nfcid3;
    param.nfcidLen = RFAL_NFCDEP_NFCID3_LEN;
    param.BS       = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    param.BR       = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    
    param.DID      = RFAL_NFCDEP_DID_NO;
    param.NAD      = RFAL_NFCDEP_NAD_NO;    
    param.LR       = RFAL_NFCDEP_LR_254;
    
    param.GBLen    = gbLen;
    param.GB       = gb;
    
    param.commMode  = ((nfcIsActive) ? RFAL_NFCDEP_COMM_ACTIVE : RFAL_NFCDEP_COMM_PASSIVE);
    param.operParam = (RFAL_NFCDEP_OPER_FULL_MI_DIS | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS | RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);
    
    
    /* Send ATR_REQ */
    ret = rfalNfcDepATR( &param, (rfalNfcDepAtrRes*)atrRes, atrResLen);
    
    
    /* If ATR_RES was properly received, update Nfc Dep Transceive parameters */
    if( ret == ERR_NONE )
    {
        rfalNfcDepTxRx.DID          = ((rfalNfcDepAtrRes*)atrRes)->DID;
        rfalNfcDepTxRx.FSx          = rfalNfcDepLR2FS( rfalNfcDepPP2LR( ((rfalNfcDepAtrRes*)atrRes)->PPt) );
        rfalNfcDepTxRx.FWT          = rfalNfcDepCalculateRWT( (((rfalNfcDepAtrRes*)atrRes)->TO & RFAL_NFCDEP_WT_MASK) );
        rfalNfcDepTxRx.dFWT         = rfalNfcDepCalculateRWT( RFAL_NFCDEP_WT_DELTA );
    }
    
    return ret;
}

ReturnCode nfcDepPSL( uint8_t txBR, uint8_t rxBR, uint8_t lr )
{
    ReturnCode ret;
    uint8_t         BRS;
    uint8_t         FSL;
    
    /* Compute BRS (bitrates) and FSL (frame size) for PSL_REQ */
    BRS = ((txBR & RFAL_NFCDEP_BRS_Dx_MASK) << RFAL_NFCDEP_BRS_DSI_POS) | (rxBR & RFAL_NFCDEP_BRS_Dx_MASK);
    FSL = (lr & RFAL_NFCDEP_LR_254);
    
    if( !nfcIsActive && (txBR != rxBR) && ((RFAL_BR_106==txBR) || (RFAL_BR_106==rxBR)) )
    { /* In Passive mode receiving and sending in different technologies is not possible. */
        return ERR_REQUEST;
    }
    
    /* Send PSL_REQ */
    ret = rfalNfcDepPSL( BRS, FSL );
    
    
    /* IF PSL_RES was properly received, change bitrates and the update Frame Size */
    if( ret == ERR_NONE )
    {
        rfalNfcDepTxRx.FSx = rfalNfcDepLR2FS( FSL );
        rfalSetBitRate( (rfalBitRate)txBR, (rfalBitRate)rxBR );
        
        if( !nfcIsActive && (txBR == RFAL_BR_106) )
        {
            /* In Passive P2P = 106kbps, it goes into NFC-A technology */
            rfalSetMode(RFAL_MODE_POLL_NFCA, (rfalBitRate) txBR, (rfalBitRate) rxBR);
        }
        if( !nfcIsActive && (txBR > RFAL_BR_106) )
        {
            /* In Passive P2P > 106kbps, it goes into NFC-F technology */
            rfalSetMode(RFAL_MODE_POLL_NFCF, (rfalBitRate) txBR, (rfalBitRate) rxBR);
        }
    }
    
    return ret;
}


ReturnCode nfcDepDeselect( void )
{
    return rfalNfcDepDSL();
}

ReturnCode nfcDepRelease( void )
{
    return rfalNfcDepRLS();
}

void nfcDepTargetSetParams( uint8_t *nfcid, uint8_t lr, uint8_t gbLen, uint8_t *gb )
{
    nfcDepTagetParam.commMode  = ((nfcIsActive) ? RFAL_NFCDEP_COMM_ACTIVE : RFAL_NFCDEP_COMM_PASSIVE);
    nfcDepTagetParam.operParam = (RFAL_NFCDEP_OPER_FULL_MI_DIS | RFAL_NFCDEP_OPER_EMPTY_DEP_DIS | RFAL_NFCDEP_OPER_ATN_EN | RFAL_NFCDEP_OPER_RTOX_REQ_EN);
    nfcDepTagetParam.bst       = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    nfcDepTagetParam.brt       = RFAL_NFCDEP_Bx_NO_HIGH_BR;
    nfcDepTagetParam.to        = RFAL_NFCDEP_WT_TRG_MAX_D11;
    nfcDepTagetParam.ppt       = rfalNfcDepLR2PP(lr);
    nfcDepTagetParam.GBtLen    = MIN(gbLen, RFAL_NFCDEP_GB_MAX_LEN);
    
    ST_MEMCPY( nfcDepTagetParam.nfcid3, nfcid, RFAL_NFCDEP_NFCID3_LEN );
    ST_MEMCPY( nfcDepTagetParam.GBt, gb, MIN(gbLen, RFAL_NFCDEP_GB_MAX_LEN) );
}


ReturnCode NfcDepTargetHandleActivation( uint8_t *bitrate, bool *isActive, uint8_t *atrReqLen, uint8_t *atrReq )
{
    ReturnCode  ret;
    rfalLmState lmState;
    bool        dataFlag;
    uint8_t     dataHdr;
    uint16_t    dataLen;
    
    ret = ERR_BUSY;
    
    switch( nfcLmState )
    {
        case NFC_LISTEN_WAIT_ATR:
            
            lmState = rfalListenGetState( &dataFlag, (rfalBitRate*)bitrate );
            if( ((lmState == RFAL_LM_STATE_IDLE) || (lmState == RFAL_LM_STATE_READY_F) || (lmState == RFAL_LM_STATE_ACTIVE_A)) && (dataFlag == true) && (nfcRxBufferActLength > 0))
            {
                dataLen = rfalConvBitsToBytes(nfcRxBufferActLength);
                
                /* If NFC-DEP frame skip the LEN byte */
                dataHdr  = RFAL_NFCDEP_LEN_LEN;
                dataLen -= RFAL_NFCDEP_LEN_LEN;
                
                /* If NFC-DEP frame at 106kbps skip the SB byte */
                if (RFAL_BR_106 == *bitrate)
                {
                    dataHdr += RFAL_NFCDEP_SB_LEN;
                    dataLen -= RFAL_NFCDEP_SB_LEN;
                }
                
                /* Check for ATR_REQ */
                if( rfalNfcDepIsAtrReq( (nfcRxBuffer+dataHdr), dataLen, NULL ) )
                {
                    /* Got a valid ATR_REQ set State/Mode accordingly to how this request (Passive/Active P2P) */
                    
                    if(lmState == RFAL_LM_STATE_IDLE)  /* Active P2P */
                    {
                        rfalListenSetState((RFAL_BR_106 == bitrate) ? RFAL_LM_STATE_TARGET_A : RFAL_LM_STATE_TARGET_F);
                        nfcDepTagetParam.commMode = RFAL_NFCDEP_COMM_ACTIVE;
                        rfalSetMode( (RFAL_MODE_LISTEN_ACTIVE_P2P), (rfalBitRate) *bitrate, (rfalBitRate) *bitrate );
                    }
                    else if(lmState == RFAL_LM_STATE_READY_F) /* Passive NFC-F P2P */
                    {
                        nfcDepTagetParam.commMode = RFAL_NFCDEP_COMM_PASSIVE; 
                        rfalListenSetState(RFAL_LM_STATE_TARGET_F);
                    }
                    else if(lmState == RFAL_LM_STATE_ACTIVE_A) /* Passive NFC-A P2P */
                    {
                        nfcDepTagetParam.commMode = RFAL_NFCDEP_COMM_PASSIVE;
                        rfalListenSetState(RFAL_LM_STATE_TARGET_A);
                    }
                    
                    /* Configure and start the NFC-DEP Listen/Target activation Handler */
                    ret = rfalNfcDepListenStartActivation( &nfcDepTagetParam, (nfcRxBuffer+dataHdr), dataLen, actvParam );
                    nfcLmState = NFC_LISTEN_WAIT_ACTIVATION;
                    ret = ERR_BUSY;
                }
                /* Check for Passive NFC-A Sleep request SLP_REQ */
                else if( rfalNfcaListenerIsSleepReq(nfcRxBuffer, rfalConvBitsToBytes(nfcRxBufferActLength) ) )
                {
                    rfalListenSleepStart( RFAL_LM_STATE_SLEEP_A, nfcRxBuffer, rfalConvBytesToBits(sizeof(nfcRxBuffer)), &nfcRxBufferActLength );
                }
                /* Otherwise ignore received frame */
                else
                {
                    rfalListenSetState(RFAL_LM_STATE_IDLE);
                }
            }
            break;
            
            
        case NFC_LISTEN_WAIT_ACTIVATION:
            /* Check whether NFC-DEP Listen/Target activation has finished */
            ret = rfalNfcDepListenGetActivationStatus();
            if( ret == ERR_NONE )
            {
                *bitrate   = nfcDepDev.info.DSI;
                *isActive  = ((nfcDepTagetParam.commMode == RFAL_NFCDEP_COMM_ACTIVE) ? true : false);
                *atrReqLen = (uint8_t)nfcDepDev.activation.Initiator.ATR_REQLen;
                memcpy(atrReq, &nfcDepDev.activation.Initiator.ATR_REQ, nfcDepDev.activation.Initiator.ATR_REQLen);
            }
            else if( ERR_BUSY != ret )
            {
                    rfalListenSetState(RFAL_LM_STATE_POWER_OFF);
            }
            break;
            
        case NFC_LISTEN_NONE:
            return ERR_REQUEST;
            
        default:
            return ERR_IO;
    }
    
    return ret;
}


bool nfcDeactivateCheck( void )
{
    return true;
}


