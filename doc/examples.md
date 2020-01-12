# examples

## docklight project
st25r2911b-disco.ptp (https://docklight.de/downloads/)

## command parsing 

### fw version 
=> 48 05 00 67 00 00 00 03 
<= 81 08 00 67 00 00 00 03 01 02 06 
with 
- 48 05 00 : Phy header (Phy|payload|PHY status)
- 67 : ST_COM_CTRL_CMD_FW_NUMBER
- 00 00 : reserved and protocol staus 
- 00 03 : Tx prot 
and 
- 81 08 00 :Phy header (Phy|payload|PHY status) 
- 67 : ST_COM_CTRL_CMD_FW_NUMBER
- 00 03 : reserved and protocol staus 
- 01 02 06  : payload (version 01.02.06)

### using the  Transceive Blocking TxRx

#### inventory (payload=260100) 
111500 44 0010 0200 59 0003 260100 00000042 0014b0e0 01fe

with 
- 00000042 flag  (= RFAL_TXRX_FLAGS_CRC_RX_KEEP |RFAL_TXRX_FLAGS_NFCV_FLAG_MANUAL see RFAL transceive flags)
- 0014b0e0 fwt Frame Waiting Time in 1/fc  (=1356000 /fc = 0.1s)
- 01fe	 rxBufSize (=510) 	
   
#### Read Dynamic Configuration (payload = 02AD0202)
121600 44 0011 0200 59 0004 02ad0202 00000042 0014b0e0 01fe

02ad0202 with 
- 02 : iso15693 flags ( Data_rate_flag see st25dv datasheet)
- AD : cmd  Read Dynamic Configuration
- 02 : IC Mfg code 
- 02 : pointer address 

#### RFAL transceive flags 	
- RFAL_TXRX_FLAGS_CRC_TX_AUTO      = (0U<<0),   < CRC will be generated automatic upon transmission                                     
- RFAL_TXRX_FLAGS_CRC_TX_MANUAL    = (1U<<0),   < CRC was calculated manually, included in txBuffer                                     
- RFAL_TXRX_FLAGS_CRC_RX_KEEP      = (1U<<1),   < Upon Reception keep the CRC in rxBuffer (reflected on rcvd length)                    
- RFAL_TXRX_FLAGS_CRC_RX_REMV      = (0U<<1),   < Upon Reception remove the CRC from rxBuffer                                           
- RFAL_TXRX_FLAGS_NFCIP1_ON        = (1U<<2),   < Enable NFCIP1 mode: Add SB(F0) and LEN bytes during Tx and skip SB(F0) byte during Rx 
- RFAL_TXRX_FLAGS_NFCIP1_OFF       = (0U<<2),   < Disable NFCIP1 mode: do not append protocol bytes while Tx nor skip while Rx          
- RFAL_TXRX_FLAGS_AGC_OFF          = (1U<<3),   < Disable Automatic Gain Control, improving multiple devices collision detection        
- RFAL_TXRX_FLAGS_AGC_ON           = (0U<<3),   < Enable Automatic Gain Control, improving single device reception                      
- RFAL_TXRX_FLAGS_PAR_RX_KEEP      = (1U<<4),   < Disable Parity and CRC check and keep the Parity and CRC bits in the received buffer  
- RFAL_TXRX_FLAGS_PAR_RX_REMV      = (0U<<0),   < Enable Parity check and remove the parity bits from the received buffer               
- RFAL_TXRX_FLAGS_PAR_TX_NONE      = (1U<<5),   < Disable automatic Parity generation (ISO14443A) and use the one provided in the buffer
- RFAL_TXRX_FLAGS_PAR_TX_AUTO      = (0U<<5),   < Enable automatic Parity generation (ISO14443A)                                        
- RFAL_TXRX_FLAGS_NFCV_FLAG_MANUAL = (1U<<6),   < Disable automatic adaption of flag byte (ISO15693) according to current comm params   
- RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO   = (0U<<6),   < Enable automatic adaption of flag byte (ISO115693) according to current comm params   

at init : RFAL_TXRX_FLAGS_CRC_TX_AUTO | RFAL_TXRX_FLAGS_CRC_RX_REMV | RFAL_TXRX_FLAGS_NFCIP1_ON | RFAL_TXRX_FLAGS_AGC_ON | RFAL_TXRX_FLAGS_PAR_RX_REMV;
