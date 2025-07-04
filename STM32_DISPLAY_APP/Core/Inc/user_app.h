/*
 * user_app.h
 *
 *  Created on: Jun 27, 2025
 *      Author: kjeyabalan
 */

#ifndef INC_USER_APP_H_
#define INC_USER_APP_H_

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "freeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "display_app.h"
#include "cJSON.h"

#define UART_RING_BUFFER_SIZE 256

#define BOOT_CMD    0x50

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

void setup_freeRTOS(void);
void SPI_handler(void *param);
void UART_handler(void *param);
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif /* INC_USER_APP_H_ */
