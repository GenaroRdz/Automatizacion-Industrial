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
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "usbd_cdc_if.h"
#include<stdbool.h>// Booleanos
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
uint8_t QMC5883L_ADDR = 0x0D << 1;
int16_t x_offset = 0, y_offset = 0; // Offset de calibración
int16_t x, y, z;
uint8_t vel=0;
uint8_t pato=0;
uint8_t bucle=1;
int8_t a=20;
char buffer[64];
float angulo;
// Variables globales para el control integral
float integral_sum = 0.0; // Acumulador del término integral
float previous_time = 0.0; // Tiempo previo para cálculo de la integración
// Constantes del controlador
#define Kp 2.0 // Constante proporcional
#define Ki 0.1 // Constante integral

// Valores PWM del servo
#define MIN_PWM 350
#define CENTER_PWM 500
#define MAX_PWM 650
#define ENCODER_PPR 673 // Pulsos por revolución del encoder
#define TIMER_PERIOD 65535 // Rango máximo del contador del timer
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint8_t rxData;
int8_t rot = 0;     // Rango de -9 a 9, valor inicial 0
float grad = 0.0;   // Puede tener valores decimales para PWM
uint16_t posicion = 0;
char mi_string[15];
uint8_t calibrara = 0;
int16_t set_point;
int32_t target_pulses = 15295;
int16_t timer_counter, last_timer;
float rpm;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Función para inicializar el sensor QMC5883L
void QMC5883L_Init() {
    uint8_t data[2];
    data[0] = 0x0B; // Registro de configuración 2
    data[1] = 0x01; // Reinicio de software
    HAL_I2C_Master_Transmit(&hi2c2, QMC5883L_ADDR, data, 2, 100);

    data[0] = 0x09; // Registro de control
    data[1] = 0x1D; // Configuración (ODR = 50Hz, RNG = 2G, OSR = 512)
    HAL_I2C_Master_Transmit(&hi2c2, QMC5883L_ADDR, data, 2, 100);
}
// Función para leer los datos de los ejes X, Y, Z
void QMC5883L_Read(int16_t *x, int16_t *y, int16_t *z, float *angulo) {
    uint8_t data[6];
    HAL_I2C_Mem_Read(&hi2c2, QMC5883L_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, data, 6, 100);

    *x = (int16_t)((data[1] << 8) | data[0]) - x_offset;
    *y = (int16_t)((data[3] << 8) | data[2]) - y_offset;
    *z = (int16_t)((data[5] << 8) | data[4]);

    // Calcular el ángulo en grados
    *angulo = atan2((float)*y, (float)*x) * (180.0 / M_PI);
    if (*angulo < 0) {
        *angulo += 360.0;  // Ajuste para obtener el ángulo en [0, 360] grados
    }
}
void Calibrate_Sensor() {
    int16_t x_min = 32767, x_max = -32768;
    int16_t y_min = 32767, y_max = -32768;

    uint32_t calibration_duration = HAL_GetTick() + 10000; // 10 segundos de calibración
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "Calibrando... Gire el sensor 360 grados\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), 100);
    while (HAL_GetTick() < calibration_duration) {
    	QMC5883L_Read(&x, &y, &z, NULL);
        if (x < x_min) x_min = x;
        if (x > x_max) x_max = x;
        if (y < y_min) y_min = y;
        if (y > y_max) y_max = y;

        HAL_Delay(100); // Esperar antes de la siguiente lectura
    }
    // Calcular los offsets para centrar en cero
    x_offset = (x_max + x_min) / 2;
    y_offset = (y_max + y_min) / 2;

    snprintf(buffer, sizeof(buffer), "Calibracion completa.\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), 100);
    CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
}
// Función para configurar la dirección del motor
void moveforward(){
    // Dirección hacia adelante
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);
}
void movebackward(){
    // Dirección hacia adelante
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);
}
void stop(){
    // Dirección hacia adelante
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);
}
// Función para configurar la velocidad del motor (PWM)
void Motor_SetSpeed(uint8_t speed) {
    if (speed > 100) speed = 100; // Limita el valor máximo a 100%
    if (speed < 0) speed = 0; // Limita el valor minimo a 0%
    // Calcula el valor de comparación para el duty cycle
    uint32_t pulse = (speed * __HAL_TIM_GET_AUTORELOAD(&htim2)) / 100;

    // Establece el duty cycle
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse);
}
// Función para calcular el tiempo transcurrido
float getElapsedTime() {
    static uint32_t last_tick = 0;
    uint32_t current_tick = HAL_GetTick(); // Obtiene el tiempo actual en ms
    float elapsed = (current_tick - last_tick) / 1000.0; // Convierte a segundos
    last_tick = current_tick;
    return elapsed;
}
// Función para calcular y aplicar el control PI
void PIControl(float current_angle, float set_point) {
    // Calcula el error
    float error = set_point - current_angle;

    // Tiempo transcurrido
    float dt = getElapsedTime();

    // Acumula el término integral
    integral_sum += error * dt;

    // Calcula el ajuste PI
    float adjustment = (Kp * error) + (Ki * integral_sum);

    // Calcula el nuevo valor PWM
    int pwm_value = CENTER_PWM + (int)adjustment;

    // Limita el valor PWM al rango permitido
    if (pwm_value < MIN_PWM) pwm_value = MIN_PWM;
    if (pwm_value > MAX_PWM) pwm_value = MAX_PWM;

    // Enviar el valor PWM al servo
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_value);
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) { // Asegúrate de que sea el temporizador configurado
    	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // Ejemplo: Alternar un LED
    	QMC5883L_Read(&x, &y, &z, &angulo);
    	PIControl(angulo, set_point);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
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
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_I2C2_Init();
  MX_USB_DEVICE_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0);
  HAL_UART_Receive_IT(&huart1,&rxData,1); // Enabling interrupt receive
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // Servo motor
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // Velocidad de motor
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3); // Posible nuevo motor
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    // Inicializa el motor
  htim2.Instance -> CCR1 = 500;
  QMC5883L_Init();
  Calibrate_Sensor(); // Llamar a la función de calibración al inicio
  int16_t x, y, z;
  float angulo;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 0);
  HAL_Delay(3000);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);
  HAL_Delay(5000);
  moveforward();
  Motor_SetSpeed(20);
  moveforward();
  QMC5883L_Read(&x, &y, &z, &angulo);
  set_point = angulo;
  //HAL_TIM_Base_Start_IT(&htim4); // Inicia el Timer con interrupción habilitada

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	int16_t timer_counter, last_timer;
  	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13); // Ejemplo: Alternar un LED
  	QMC5883L_Read(&x, &y, &z, &angulo);
  	PIControl(angulo, set_point);
	timer_counter = __HAL_TIM_GET_COUNTER(&htim3);
	if( timer_counter >= 0 && timer_counter < 45885){
		Motor_SetSpeed(20);
		moveforward();
	}
	else{
		stop();
	}
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
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 72-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 20000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.Pulse = 0;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.Pulse = 500;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 720;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 10000;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance==USART1)
  {
    if(rxData==78) // Ascii value of 'N' is 78 (N for NO)
    {
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 1);
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);

    }
    else if (rxData==89) // Ascii value of 'Y' is 89 (Y for YES)
    {
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
    	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);

    }
    else if (rxData==69) // Ascii value of 'E' is 89 (E for EXIT)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);

    }
    else if (rxData==67) // Ascii value of 'C' (C is for calibrate)
    {
        calibrara = 1;

    }
    else if (rxData==89) // Ascii value of 'Y' (Speed up)
    {
    	if (vel > 100){
    		vel = 100;
    		Motor_SetSpeed(vel);
    	}
    	else{
    		vel += 10;
    		Motor_SetSpeed(vel);
    	}
    }
    else if (rxData==90) // Ascii value of 'Z' (Speed down)
    {
    	if (vel < 0){
    		vel = 0;
    		Motor_SetSpeed(vel);
    	}
    	else{
    		vel -= 10;
    		Motor_SetSpeed(vel);
    	}
    }
    else if (rxData==71) // Ascii value of 'G' (Speed down)
    {
    	if (posicion < 9000){
    		vel = 10;
    		Motor_SetSpeed(vel);
    	}
    	else{
    		vel =0;
    		Motor_SetSpeed(vel);
    	}
    }
    HAL_UART_Receive_IT(&huart1,&rxData,1); // Enabling interrupt receive again
  }
}
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
