/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_COMMON_H
#define __USB_COMMON_H


/*! contains the state of the USB   */
typedef enum
{
	USB_STATE_NONE = 0,		/*!< No event  */
	USB_STATE_RXFRAME ,		/*!< an RX frame has been received */
	USB_STATE_TXFRAME ,		/*!< an TX frame has been sent */

} SCH_EVENT;
#endif
