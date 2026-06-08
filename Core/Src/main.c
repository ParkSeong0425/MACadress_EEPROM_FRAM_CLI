/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 24AA02E48 MAC EEPROM + MB85RS64PNF FRAM CLI over RS485
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

#include "mac_eeprom.h"
#include "fram.h"
#include "cli.h"
#include "rs485.h"

#include <stdio.h>

void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void I2C1_ForcePullup(void);
static void Startup_PrintMac(void);
static void Startup_CheckFram(void);
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  MX_SPI3_Init();

  /*
   * 중요:
   * 첫 printf 전에 RS485_Init()을 먼저 호출해야 함.
   * RS485_Init()은 PF12 RS485_DIR을 수신 모드로 내려놓음.
   */
  RS485_Init();

  HAL_Delay(1000);

  printf("\r\n");
  printf("================================\r\n");
  printf(" 24AA02E48 + MB85RS64PNF CLI\r\n");
  printf(" USART6 RS485 CLI DEBUG\r\n");
  printf("================================\r\n");

  I2C1_ForcePullup();

  (void)Fram_InitPins();

  Startup_PrintMac();
  Startup_CheckFram();

  CLI_Init();

  while (1)
  {
    CLI_Poll();
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 |
                                RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

static void I2C1_ForcePullup(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*
   * 현재 보드 기준:
   * I2C1_SCL = PB6
   * I2C1_SDA = PB9
   */
  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;

  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  printf("[I2C] force pull-up: SCL=PB6, SDA=PB9\r\n");
}

static void Startup_PrintMac(void)
{
  uint8_t mac[MAC_EEPROM_EUI48_LEN] = {0};

  HAL_StatusTypeDef st;

  st = MacEeprom_ReadMac(&hi2c1, mac);

  if (st == HAL_OK)
  {
    printf("[MAC] EUI-48 = %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           mac[0],
           mac[1],
           mac[2],
           mac[3],
           mac[4],
           mac[5]);

    if (MacEeprom_IsMacPlausible(mac) != 0U)
    {
      printf("[MAC] read OK\r\n");
    }
    else
    {
      printf("[MAC] WARNING: MAC value looks invalid\r\n");
    }
  }
  else
  {
    printf("[MAC] read failed, status=%d\r\n", st);
  }
}

static void Startup_CheckFram(void)
{
  uint8_t s0 = 0U;
  uint8_t s1 = 0U;
  uint8_t s2 = 0U;

  HAL_StatusTypeDef st;

  st = Fram_CheckWriteEnableLatch(&hspi3, &s0, &s1, &s2);

  printf("[FRAM] WEL status=%d, before=0x%02X, after_WREN=0x%02X, after_WRDI=0x%02X\r\n",
         st,
         s0,
         s1,
         s2);

  if (st == HAL_OK)
  {
    printf("[FRAM] SPI communication OK\r\n");
  }
  else
  {
    printf("[FRAM] SPI communication failed\r\n");
  }
}

/* USER CODE END 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
