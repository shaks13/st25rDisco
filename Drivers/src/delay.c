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
 *      PROJECT:   ST25R391x firmware
 *      $Revision: $
 *      LANGUAGE:  ANSI C
 */

/*! \file delay.c
 *
 *  \brief SW Delay implementation
 *
 *  \author 
 *
 *   This module makes use of a System Tick in millisconds and provides
 *   an abstraction for SW delay
 *
 */

/*
******************************************************************************
* INCLUDES
******************************************************************************
*/
#include "delay.h"


/*
******************************************************************************
* LOCAL DEFINES
******************************************************************************
*/
//uint32_t getUs(void);

/*
******************************************************************************
* LOCAL VARIABLES
******************************************************************************
*/

/*
******************************************************************************
* GLOBAL FUNCTIONS
******************************************************************************
*/

/*******************************************************************************/
void delayUs(uint32_t micros) 
{
  uint32_t start = getUs();
  while (getUs() - start < micros)
  {
      __NOP();
  }
}

/*
******************************************************************************
* LOCAL FUNCTIONS
******************************************************************************
*/
uint32_t getUs(void) {
  uint32_t usTicks = HAL_RCC_GetSysClockFreq() / 1000000;
  register uint32_t ms, cycle_cnt;
  do {
    ms = HAL_GetTick();
    cycle_cnt = SysTick->VAL;
  } while (ms != HAL_GetTick());
  
  return (ms * 1000) + (usTicks * 1000 - cycle_cnt) / usTicks;
}


