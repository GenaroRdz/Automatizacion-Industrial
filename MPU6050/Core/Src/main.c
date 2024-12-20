/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <usbd_cdc_if.h>

#define MPU6050_ADDR 0x68 << 1  // Dirección I2C del MPU6050
#define WHO_AM_I_REG 0x75       // Registro de identificación del MPU6050
#define PWR_MGMT_1_REG 0x6B     // Registro de gestión de energía
#define ACCEL_XOUT_H 0x3B       // Registro de acelerómetro (X alto)
#define GYRO_XOUT_H  0x43       // Registro de giroscopio (X alto)
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Enviar datos por USB
void MPU6050_Init(void) {
    uint8_t data;

    // Verifica el WHO_AM_I para asegurarte de que el MPU6050 está presente
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, WHO_AM_I_REG, 1, &data, 1, HAL_MAX_DELAY);
    if (data != 0x68) {
        while (1) {
            // Error: MPU6050 no detectado
        }
    }

    // Salir del modo de suspensión
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, PWR_MGMT_1_REG, 1, &data, 1, HAL_MAX_DELAY);
}

void MPU6050_ReadAccel(int16_t *accel) {
    uint8_t buffer[6];

    // Leer registros de acelerómetro
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, ACCEL_XOUT_H, 1, buffer, 6, HAL_MAX_DELAY);

    // Convertir datos a valores de 16 bits
    accel[0] = (int16_t)(buffer[0] << 8 | buffer[1]); // X
    accel[1] = (int16_t)(buffer[2] << 8 | buffer[3]); // Y
    accel[2] = (int16_t)(buffer[4] << 8 | buffer[5]); // Z
}

void MPU6050_ReadGyro(int16_t *gyro) {
    uint8_t buffer[6];

    // Leer registros de giroscopio
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, GYRO_XOUT_H, 1, buffer, 6, HAL_MAX_DELAY);

    // Convertir datos a valores de 16 bits
    gyro[0] = (int16_t)(buffer[0] << 8 | buffer[1]); // X
    gyro[1] = (int16_t)(buffer[2] << 8 | buffer[3]); // Y
    gyro[2] = (int16_t)(buffer[4] << 8 | buffer[5]); // Z
}

void CalibrateGyro(int16_t *gyro_bias) {
    int16_t gyro[3];
    int32_t gyro_sum[3] = {0, 0, 0};
    const int num_samples = 100;

    // Toma múltiples lecturas en reposo
    for (int i = 0; i < num_samples; i++) {
        MPU6050_ReadGyro(gyro);
        gyro_sum[0] += gyro[0];
        gyro_sum[1] += gyro[1];
        gyro_sum[2] += gyro[2];
        HAL_Delay(10); // Espera entre lecturas
    }

    // Calcula el promedio
    gyro_bias[0] = gyro_sum[0] / num_samples;
    gyro_bias[1] = gyro_sum[1] / num_samples;
    gyro_bias[2] = gyro_sum[2] / num_samples;

    // Enviar datos de calibración por Bluetooth
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "Gyro Bias: X=%d, Y=%d, Z=%d\r\n",
             gyro_bias[0], gyro_bias[1], gyro_bias[2]);
    Send_Data_USB(buffer);
}
void Send_Data_USB(const char *data) {
    CDC_Transmit_FS((uint8_t *)data, strlen(data));
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval intv
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  MPU6050_Init();

  int16_t accel[3], gyro[3], gyro_bias[3];
  char buffer[128];
  float angle_acc, angle_gyro = 0, angle_fused = 0;
  const float dt = 0.01; // Intervalo de tiempo (10 ms)
  const float alpha = 0.98; // Constante del filtro complementario
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
      MPU6050_ReadAccel(accel);
      MPU6050_ReadGyro(gyro);

      // Calcular el ángulo del acelerómetro (basado en la inclinación)
      angle_acc = atan2((float)accel[1], (float)accel[2]) * (180.0 / M_PI);

      // Corregir el sesgo del giroscopio y calcular el cambio de ángulo
      gyro[0] -= gyro_bias[0]; // Usamos solo el eje X para el giro en este caso
      angle_gyro += ((float)gyro[0] / 131.0) * dt; // 131.0 es la sensibilidad del giroscopio en dps

      // Fusión del ángulo usando el filtro complementario
      angle_fused = alpha * (angle_fused + ((float)gyro[0] / 131.0) * dt) + (1 - alpha) * angle_acc;

      // Formatea los datos en un buffer
      snprintf(buffer, sizeof(buffer),
               "Accel Angle: %.2f | Gyro Angle: %.2f | Fused Angle: %.2f\r\n",
               angle_acc, angle_gyro, angle_fused);

      // Enviar los datos por Bluetooth
      Send_Data_USB(buffer);

      HAL_Delay(10); // Espera 10 ms
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 144;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
