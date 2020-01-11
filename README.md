# overview

## harware 
st25r3911b-disco (https://www.st.com/en/evaluation-tools/st25r3911b-disco.html)

## IDE
sw4stm32 (https://www.st.com/en/development-tools/sw4stm32.html)

# protocol

## physical
On the physical line the following byte-stream is seen: 

    Byte 0    1        2         3         4         5         6         7      8            (8 + tx-prot) (9+tx-prot) (10+tx-prot)                
    +------+-------+-------+----------+---------+---------+---------+---------+-------...---+-------------+-----------+-----------+-------
    | TID  |payload|PHY    | protocol | reserved| protocol| tx-prot | tx-prot | data        | protocol B  | reserved  | status  B |                
    |      |       | status|          |         |  status |   MSB   |  LSB    |             |             |           |
    +------+-------+-------+----------+---------+---------+---------+---------+-------...---+-------------+-----------+-----------+-------

    TID = the transaction ID is reflected th every report sent.
    payload = the amount of data in this report (sent from host to device) - a packet can be longer than this size (i.e. the packet is sent with several packet - note *)
    PHY status = if a protocol was not processed (because e.g. the protocol id was unknown) the next PHY packet that is sent back will contain a PHY status byte <> 0 indicating that an error occurred (some time in the back).
    protocol status = a single byte representing the result of the command if the command generated either data to be sent back or is a read command (has the MSBit cleared)
    tx-prot = number of bytes transmitted (for this protocol packet) from device to host

## protocol
    *) if a packet cannot be sent in one report the next report will not have the "protocol header" set again, but continue with the raw data. 
	
	  This bytestream is build by through the following 3 steps:

    ------------------------------------------------------

    1. The Firmware-Application provides a single data packet containing the result:

	        Byte 0         1   ...                   tx-size -1
    +------------+-------- ... ----------------------------+
    |   data                                               |
    +--------------------- ... ----------------------------+

	2. The function processReceivedPackets of file stream_appl_handler.c in the firmware adds the protocol byte, the reserved and the status byte and the tx-prot 16-bit word (from the information provided by the firmware application):

    Byte 0         1          2         3        4          5                                   4 + tx-size
    +------------+---------+---------+---------+---------+-------------------------- ... ------------------+
    | protocol   | reserved| status  | tx-prot | tx-prot |  data                                           |
    |            |         |         |  MSB    |  LSB    |                                                 |
    +------------+---------+---------+---------+---------+-------------------------- ... ------------------+
    ^                                                    ^
    |                                                    |
    +----- this is the protocol header ------------------+


    Steps 1. and 2. can be repeated several times before handing a single buffer (containing all packets) to the HID driver.

    ------------------------------------------------------

    3. The Hid driver on the Device side splits the buffer into Hid Reports and adds for each Hid Report 
       (the following report header):

    a) if the data-buffer fits in one HID-report:

    Byte 0      1      3       4                 3+ payload         ....       63
    +------+-------+-------+----------+---... +-------------+-------     -----------+
    | TID  |payload|status | packet(s)                      | padding if necessary  | 
    +------+-------+-------+----------+---...-+-------------+-------.... -----------+
    ^                      ^
    |                      |
    +----------------------+
    this is the HID
     driver header

    b) if the packet(s) need more than 1 HID-report

    Byte 0      1     2       3                                    ....       63
    +------+-------+------+----------+---... +-------------+-------     -----------+
    | TID  |payload|status| packet(s)                                              | 
    |      | = 61  |      |                                                        |
    +------+-------+------+----------+---...-+-------------+-------.... -----------+
		           
    +------+-------+------+----------+---... +-------------+-------     -----------+
    | new  |payload|status| packet(s)                                              | 
    | TID  | = 61  |      |      continued                                         |
    +------+-------+------+----------+---...-+-------------+-------.... -----------+

    .... as many HID reports as needed until the packet(s) are totally transmitted (here is how the final report might look like):

    +------+-------+------+----------+---... +-------------+-------     -----------+
    | next |payload|status| packet(s)                      | padding if necessary  | 
    | TID  | <= 61 |      |      continued                 |                       |
    +------+-------+------+----------+---...-+-------------+-------.... -----------+

    */


## TID-Rules

    On the host side, the TID is generated as a 4-bit number counting from 0 to 0x0F and than rolling over to 0. 

    The device side takes the TID received from the Host and moves it to the upper nibble, increments its own TID counter by one (range is again 0 to 0x0F) and 
    adds its own TID counter.
        txTID = (rxTID << 4) | ( ++txTID & 0xF )

## Protocol-Rules:

    The MSBit of the protocol byte defines the direction: 1 = write, 0 = read.
	This bit is set by the ST Stream class itself during sending or receiving.
	
    The following number range is special:
	0x60 - 0x7F: reserved for generic commands (part of the common firmware)
	0x60: is a configuration protocol for the firmware itself 
	0x6B: is reserved for the bootloader
	0x7F: is reserved for the flush
	
	An application can use the numbers: 0x00 - 0x5F for its own commands

## command set

### low level
- ST_COM_CTRL_CMD_FW_INFORMATION  	0x66  # returns zero-terminated string with information about fw e.g. chip, board */
- ST_COM_CTRL_CMD_FW_NUMBER  		0x67  # returns the 3-byte FW number */
- ST_COM_WRITE_REG 		 			0x68
- ST_COM_READ_REG 					0x69

### ISO 15693 with command code 
- deinitialize 			0xdf ( ex: 920700 44 0002 0000 DF 00)
- initialize 			0xd0 ( ex: 930800 44 0003 0004 D0 00 00)
- inventory 			0xd2 ( ex: 940600 44 0001 00A1 D2)
- readSingleBlock		0xd7 ( ex: 951000 44 000b 0028 d7 00 1e156502002402e0 00)
- writeSingleBlock		0xd8
- readMultipleBlock		0xdA (ex : 961000 44 000B 0028 DA 00 1e156502002402e0 00 04)
- 

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
121600 44 0011 0200 5900 04 02ad0202 00000042 0014b0e0 01fe

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


#link
https://www.st.com/resource/en/datasheet/st25r3911b.pdf
https://www.st.com/resource/en/datasheet/st25dv04k.pdf
