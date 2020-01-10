/*
 * drv_conf.h
 *
 *  Created on: Dec 2, 2019
 *      Author: H369117
 */

#ifndef INC_HW_CONF_H_
#define INC_HW_CONF_H_


#define IRQ_3911_Pin 			GPIO_PIN_0
#define IRQ_3911_GPIO_Port 		GPIOA
#define IRQ_3911_EXTI_IRQn 		EXTI0_IRQn
#define LED_F_Pin 				GPIO_PIN_1
#define LED_F_GPIO_Port 		GPIOA
#define LED_B_Pin 				GPIO_PIN_4
#define LED_B_GPIO_Port 		GPIOA
#define LED_A_Pin 				GPIO_PIN_0
#define LED_A_GPIO_Port 		GPIOB
#define LED_FIELD_Pin 			GPIO_PIN_8
#define LED_FIELD_GPIO_Port 	GPIOA
#define LED_V_Pin 				GPIO_PIN_4
#define LED_V_GPIO_Port 		GPIOB
#define LED_AP2P_Pin 			GPIO_PIN_5
#define LED_AP2P_GPIO_Port 		GPIOB
#define SPI1_CS_Pin 			GPIO_PIN_6
#define SPI1_CS_GPIO_Port 		GPIOB
#define LED_FIELD_Pin_MB1325B 	GPIO_PIN_3  /* Support for old board version MB1325-A */
#endif /* INC_HW_CONF_H_ */
