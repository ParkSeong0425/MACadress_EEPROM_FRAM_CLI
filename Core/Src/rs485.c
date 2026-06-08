/*
 * rs485.c
 *
 *  Created on: Jun 8, 2026
 *      Author: HWNOT
 */
#include "rs485.h"

#include "usart.h"
#include "gpio.h"

#define RS485_UART_HANDLE    huart6

/*
 * 일반적인 RS-485 방향 제어:
 * RS485_DIR = 1 -> 송신 모드
 * RS485_DIR = 0 -> 수신 모드
 *
 * 만약 네 회로가 반대라면 RS485_SetTx/RS485_SetRx의 SET/RESET만 서로 바꾸면 됨.
 */

static void RS485_ShortDelay(void)
{
  for (volatile uint32_t i = 0; i < 300U; i++)
  {
    __NOP();
  }
}

void RS485_Init(void)
{
  RS485_SetRx();

  __HAL_UART_CLEAR_OREFLAG(&RS485_UART_HANDLE);
  __HAL_UART_CLEAR_NEFLAG(&RS485_UART_HANDLE);
  __HAL_UART_CLEAR_FEFLAG(&RS485_UART_HANDLE);
  __HAL_UART_CLEAR_PEFLAG(&RS485_UART_HANDLE);
}

void RS485_SetTx(void)
{
	  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12, GPIO_PIN_SET);
  RS485_ShortDelay();
}

void RS485_SetRx(void)
{
	  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_12, GPIO_PIN_RESET);
  RS485_ShortDelay();
}

HAL_StatusTypeDef RS485_Transmit(const uint8_t *data,
                                 uint16_t len,
                                 uint32_t timeout)
{
  HAL_StatusTypeDef st;
  uint32_t tick_start;

  if (data == NULL || len == 0U)
  {
    return HAL_ERROR;
  }

  RS485_SetTx();

  st = HAL_UART_Transmit(&RS485_UART_HANDLE, (uint8_t *)data, len, timeout);

  tick_start = HAL_GetTick();

  while (__HAL_UART_GET_FLAG(&RS485_UART_HANDLE, UART_FLAG_TC) == RESET)
  {
    if ((HAL_GetTick() - tick_start) > timeout)
    {
      RS485_SetRx();
      return HAL_TIMEOUT;
    }
  }

  RS485_SetRx();

  return st;
}

