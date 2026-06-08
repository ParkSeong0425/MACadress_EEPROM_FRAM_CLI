/*
 * cli.c
 *
 *  Created on: Jun 8, 2026
 *      Author: HWNOT
 */
#include "cli.h"

#include "main.h"
#include "usart.h"
#include "i2c.h"
#include "spi.h"

#include "mac_eeprom.h"
#include "fram.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CLI_UART_HANDLE     huart6

#define CLI_RX_BUF_SIZE     128U
#define CLI_MAX_ARGS        24U

static char g_cli_buf[CLI_RX_BUF_SIZE];
static uint16_t g_cli_len = 0U;

static void CLI_PrintHelp(void);
static void CLI_PrintPrompt(void);
static void CLI_ProcessLine(char *line);
static int CLI_SplitArgs(char *line, char *argv[], int max_args);

static uint8_t ParseU32(const char *s, uint32_t *out);
static uint8_t ParseHexByte(const char *s, uint8_t *out);

static void CLI_PrintMac(void);
static void CLI_I2CScan(void);

static void CLI_CmdFram(int argc, char *argv[]);

static void FRAM_Status(void);
static void FRAM_Dump(uint16_t addr, uint16_t len);
static HAL_StatusTypeDef FRAM_Clear(uint16_t addr, uint16_t len);

void CLI_Init(void)
{
  CLI_PrintHelp();
  CLI_PrintPrompt();
}

void CLI_Poll(void)
{
  uint8_t ch;

  __HAL_UART_CLEAR_OREFLAG(&CLI_UART_HANDLE);

  if (HAL_UART_Receive(&CLI_UART_HANDLE, &ch, 1, 1) != HAL_OK)
  {
    return;
  }

  /*
   * Enter 처리
   * 화면에 입력 글자는 Tera Term Local echo가 보여주고,
   * 펌웨어는 명령 실행만 담당합니다.
   */
  if (ch == '\r' || ch == '\n')
  {
    printf("\r\n");

    if (g_cli_len > 0U)
    {
      g_cli_buf[g_cli_len] = '\0';
      CLI_ProcessLine(g_cli_buf);
      g_cli_len = 0U;
    }

    CLI_PrintPrompt();
    return;
  }

  /*
   * Backspace / Delete
   * 화면 지우기는 Tera Term Local echo가 처리하고,
   * 펌웨어는 내부 버퍼만 줄입니다.
   */
  if (ch == 0x08U || ch == 0x7FU)
  {
    if (g_cli_len > 0U)
    {
      g_cli_len--;
    }

    return;
  }

  /*
   * 일반 문자
   * 화면 출력은 Tera Term Local echo가 담당.
   * 펌웨어는 버퍼에 저장만 함.
   */
  if (ch >= 32U && ch <= 126U)
  {
    if (g_cli_len < (CLI_RX_BUF_SIZE - 1U))
    {
      g_cli_buf[g_cli_len++] = (char)ch;
    }
  }
}

static void CLI_PrintHelp(void)
{
  printf("\r\n");
  printf("Commands:\r\n");
  printf("  help\r\n");
  printf("  info\r\n");
  printf("  mac\r\n");
  printf("  i2c scan\r\n");
  printf("  fram status\r\n");
  printf("  fram dump <addr> <len>\r\n");
  printf("  fram write <addr> <hex bytes...>\r\n");
  printf("    ex) fram write 0x0100 12 34 56 78 A5 5A\r\n");
  printf("  fram clear <addr> <len>\r\n");
  printf("  reboot\r\n");
  printf("\r\n");
}

static void CLI_PrintPrompt(void)
{
  printf("> ");
}

static void CLI_ProcessLine(char *line)
{
  char *argv[CLI_MAX_ARGS];
  int argc = CLI_SplitArgs(line, argv, CLI_MAX_ARGS);

  if (argc <= 0)
  {
    return;
  }

  if (strcmp(argv[0], "help") == 0)
  {
    CLI_PrintHelp();
  }
  else if (strcmp(argv[0], "info") == 0)
  {
    CLI_PrintMac();
    FRAM_Status();
  }
  else if (strcmp(argv[0], "mac") == 0)
  {
    CLI_PrintMac();
  }
  else if (strcmp(argv[0], "i2c") == 0)
  {
    if (argc >= 2 && strcmp(argv[1], "scan") == 0)
    {
      CLI_I2CScan();
    }
    else
    {
      printf("Usage: i2c scan\r\n");
    }
  }
  else if (strcmp(argv[0], "fram") == 0)
  {
    CLI_CmdFram(argc, argv);
  }
  else if (strcmp(argv[0], "reboot") == 0)
  {
    printf("Rebooting...\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();
  }
  else
  {
    printf("Unknown command: %s\r\n", argv[0]);
    printf("Type 'help'\r\n");
  }
}

static int CLI_SplitArgs(char *line, char *argv[], int max_args)
{
  int argc = 0;
  char *tok = strtok(line, " \t");

  while (tok != NULL && argc < max_args)
  {
    argv[argc++] = tok;
    tok = strtok(NULL, " \t");
  }

  return argc;
}

static uint8_t ParseU32(const char *s, uint32_t *out)
{
  char *end;
  unsigned long v;

  if (s == NULL || out == NULL)
  {
    return 0U;
  }

  v = strtoul(s, &end, 0);

  if (*end != '\0')
  {
    return 0U;
  }

  *out = (uint32_t)v;

  return 1U;
}

static uint8_t ParseHexByte(const char *s, uint8_t *out)
{
  char *end;
  unsigned long v;

  if (s == NULL || out == NULL)
  {
    return 0U;
  }

  v = strtoul(s, &end, 16);

  if (*end != '\0' || v > 0xFFUL)
  {
    return 0U;
  }

  *out = (uint8_t)v;

  return 1U;
}

static void CLI_PrintMac(void)
{
  uint8_t mac[MAC_EEPROM_EUI48_LEN] = {0};

  HAL_StatusTypeDef st = MacEeprom_ReadMac(&hi2c1, mac);

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

static void CLI_I2CScan(void)
{
  uint8_t found = 0U;

  printf("[I2C] scan start\r\n");

  for (uint8_t addr = 1U; addr < 128U; addr++)
  {
    HAL_StatusTypeDef st;

    st = HAL_I2C_IsDeviceReady(&hi2c1,
                               (uint16_t)(addr << 1),
                               2,
                               20);

    if (st == HAL_OK)
    {
      found++;
      printf("[I2C] found 7-bit address 0x%02X\r\n", addr);
    }
  }

  printf("[I2C] scan end, found=%u\r\n", found);
}

static void FRAM_Status(void)
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

static void FRAM_Dump(uint16_t addr, uint16_t len)
{
  uint8_t buf[16];

  while (len > 0U)
  {
    uint16_t chunk = (len > sizeof(buf)) ? sizeof(buf) : len;

    memset(buf, 0, sizeof(buf));

    if (Fram_Read(&hspi3, addr, buf, chunk) != HAL_OK)
    {
      printf("[FRAM] read failed at 0x%04X\r\n", addr);
      return;
    }

    printf("0x%04X: ", addr);

    for (uint16_t i = 0; i < chunk; i++)
    {
      printf("%02X ", buf[i]);
    }

    printf("\r\n");

    addr += chunk;
    len -= chunk;
  }
}

static HAL_StatusTypeDef FRAM_Clear(uint16_t addr, uint16_t len)
{
  uint8_t zero[16] = {0};

  while (len > 0U)
  {
    uint16_t chunk = (len > sizeof(zero)) ? sizeof(zero) : len;

    if (Fram_Write(&hspi3, addr, zero, chunk) != HAL_OK)
    {
      return HAL_ERROR;
    }

    addr += chunk;
    len -= chunk;
  }

  return HAL_OK;
}

static void CLI_CmdFram(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("Usage: fram status | dump | write | clear\r\n");
    return;
  }

  if (strcmp(argv[1], "status") == 0)
  {
    FRAM_Status();
  }
  else if (strcmp(argv[1], "dump") == 0)
  {
    uint32_t addr;
    uint32_t len;

    if (argc < 4 ||
        ParseU32(argv[2], &addr) == 0U ||
        ParseU32(argv[3], &len) == 0U)
    {
      printf("Usage: fram dump <addr> <len>\r\n");
      return;
    }

    if (addr >= FRAM_MB85RS64_SIZE_BYTES ||
        len == 0U ||
        (addr + len) > FRAM_MB85RS64_SIZE_BYTES)
    {
      printf("Invalid FRAM range\r\n");
      return;
    }

    FRAM_Dump((uint16_t)addr, (uint16_t)len);
  }
  else if (strcmp(argv[1], "write") == 0)
  {
    uint32_t addr;
    uint8_t data[32];
    uint16_t len = 0U;

    if (argc < 4 || ParseU32(argv[2], &addr) == 0U)
    {
      printf("Usage: fram write <addr> <hex bytes...>\r\n");
      return;
    }

    for (int i = 3; i < argc && len < sizeof(data); i++)
    {
      if (ParseHexByte(argv[i], &data[len]) == 0U)
      {
        printf("Invalid hex byte: %s\r\n", argv[i]);
        return;
      }

      len++;
    }

    if (addr >= FRAM_MB85RS64_SIZE_BYTES ||
        len == 0U ||
        (addr + len) > FRAM_MB85RS64_SIZE_BYTES)
    {
      printf("Invalid FRAM range\r\n");
      return;
    }

    if (Fram_Write(&hspi3, (uint16_t)addr, data, len) == HAL_OK)
    {
      printf("[FRAM] write OK\r\n");
      FRAM_Dump((uint16_t)addr, len);
    }
    else
    {
      printf("[FRAM] write failed\r\n");
    }
  }
  else if (strcmp(argv[1], "clear") == 0)
  {
    uint32_t addr;
    uint32_t len;

    if (argc < 4 ||
        ParseU32(argv[2], &addr) == 0U ||
        ParseU32(argv[3], &len) == 0U)
    {
      printf("Usage: fram clear <addr> <len>\r\n");
      return;
    }

    if (addr >= FRAM_MB85RS64_SIZE_BYTES ||
        len == 0U ||
        (addr + len) > FRAM_MB85RS64_SIZE_BYTES)
    {
      printf("Invalid FRAM range\r\n");
      return;
    }

    if (FRAM_Clear((uint16_t)addr, (uint16_t)len) == HAL_OK)
    {
      printf("[FRAM] clear OK\r\n");
      FRAM_Dump((uint16_t)addr, (uint16_t)len);
    }
    else
    {
      printf("[FRAM] clear failed\r\n");
    }
  }
  else
  {
    printf("Unknown fram command\r\n");
  }
}

