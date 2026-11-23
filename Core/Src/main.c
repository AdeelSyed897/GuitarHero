/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "Adafruit_ST7735_Macros.h"
#include "Adafruit_TFTShield18_Macros.h"
#include "dwt_module.h"
#include "Adafruit_TFTShield18_API.h"
#include "Adafruit_ST7735_API.h"
#include "stm32h5xx_hal.h"

#include "buzzer.h"
#include "stopwatch.h"
#include "stm32h5xx_hal_tim.h"

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

COM_InitTypeDef BspCOMInit;

I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef handle_GPDMA1_Channel6;
DMA_HandleTypeDef handle_GPDMA1_Channel5;

RNG_HandleTypeDef hrng;

SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef handle_GPDMA1_Channel7;

TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */

Buzzer buzzer1 = {
  &htim1,
  TIM_CHANNEL_1,
  false
};

extern Note const chromaticScale[NUM_NOTES];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_I2C1_Init(void);
static void MX_ICACHE_Init(void);
static void MX_SPI2_Init(void);
static void MX_RNG_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

typedef enum {
    WELCOME,
    COUNTDOWN,
    PLAYING,
    WIN,
    LOSE
} GameState;

#define BTN1 (1 << 0)
#define BTN2 (1 << 1)
#define BTN3 (1 << 2)
#define BTN4 (1 << 3)

typedef struct {
    uint32_t pitch;
    uint32_t duration;
    uint8_t buttons;
} SongNote;


const SongNote song[] = {
    // Intro - single notes
    {NOTE_C, 200, BTN1},
    {NOTE_D, 200, BTN2},
    {NOTE_E, 200, BTN3},
    {NOTE_G, 200, BTN4},

    // First chord
    {NOTE_C, 250, BTN1 | BTN3},
    {NOTE_E, 200, BTN3},
    {NOTE_D, 200, BTN2},
    {NOTE_C, 200, BTN1},

    // Second section
    {NOTE_E, 200, BTN3},
    {NOTE_G, 200, BTN4},
    {NOTE_E, 250, BTN2 | BTN4},
    {NOTE_C, 200, BTN1},

    // Third section
    {NOTE_D, 200, BTN2},
    {NOTE_E, 200, BTN3},
    {NOTE_G, 250, BTN1 | BTN4},
    {NOTE_D, 200, BTN2},

    // Fourth section
    {NOTE_C, 200, BTN1},
    {NOTE_C, 200, BTN1},
    {NOTE_G, 250, BTN3 | BTN4},
    {NOTE_G, 200, BTN4},

    // Fifth section
    {NOTE_E, 200, BTN3},
    {NOTE_E, 250, BTN1 | BTN2},
    {NOTE_D, 200, BTN2},
    {NOTE_D, 200, BTN2},

    // Ending
    {NOTE_C, 200, BTN1},
    {NOTE_E, 200, BTN3},
    {NOTE_G, 250, BTN2 | BTN3},
    {NOTE_C, 300, BTN1 | BTN4}
};

#define SONG_LENGTH (sizeof(song) / sizeof(song[0]))


void showWelcome(void)
{
    Display_fillScreen(ST77XX_BLACK);
    Display_setTextSize(2, 2);
    Display_setTextColor(ST7735_WHITE);

    Display_setCursor(15, 30);
    Display_print("Welcome to");

    Display_setCursor(15, 60);
    Display_print("Guitar Hero!");

    Display_setTextSize(1, 1);
    Display_setCursor(15, 100);
    Display_print("Press SELECT to start");
}

void showCountdown(uint32_t number)
{
    Display_fillScreen(ST77XX_BLACK);
    Display_setTextSize(3, 3);
    Display_setTextColor(ST7735_WHITE);
    Display_setCursor(70, 50);

    if (number == 3)
        Display_print("3");
    else if (number == 2)
        Display_print("2");
    else if (number == 1)
        Display_print("1");
}

void showPlaying(void)
{
    Display_fillScreen(ST77XX_BLACK);
    Display_setTextSize(2, 2);
    Display_setTextColor(ST7735_GREEN);
    Display_setCursor(15, 50);
    Display_print("PLAYING!");
}


void playSongNote(uint32_t index)
{
    buzzerDisable(&buzzer1);
    buzzerSetNote(&buzzer1, song[index].pitch, 1);
    buzzerEnable(&buzzer1);
}

void showSongFinished(void)
{
    Display_fillScreen(ST77XX_BLACK);
    Display_setTextSize(2, 2);
    Display_setTextColor(ST7735_GREEN);

    Display_setCursor(15, 40);
    Display_print("SONG DONE!");

    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_WHITE);
    Display_setCursor(15, 100);
    Display_print("Press SELECT to return");
}


void initGuitarButtons(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // BTN1 = PB13, BTN3 = PB8
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // BTN2 = PC12, BTN4 = PC11
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

uint8_t getButtonStates(void)
{
    uint8_t buttons = 0;

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
        buttons |= (1 << 0);

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_12) == GPIO_PIN_RESET)
        buttons |= (1 << 1);

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_RESET)
        buttons |= (1 << 2);

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11) == GPIO_PIN_RESET)
        buttons |= (1 << 3);

    return buttons;
}


const uint16_t laneColors[4] = {
    ST7735_RED,
    ST7735_GREEN,
    ST7735_BLUE,
    ST7735_YELLOW
};

void drawNoteLanes(void)
{
    Display_fillScreen(ST77XX_BLACK);

    for (int i = 0; i < 4; i++)
    {
        // Four lanes across the 160-pixel screen
        Display_drawRect(i * 40, 20, 40, 95, ST7735_WHITE);

        // Colored button indicators
        Display_fillRect(i * 40 + 10, 92, 20, 15, laneColors[i]);
    }
}

void drawCurrentNote(uint8_t buttons)
{
    // Clear previous note indicators
    Display_fillRect(0, 35, 160, 45, ST7735_BLACK);

    // Draw every required button
    for (int i = 0; i < 4; i++)
    {
        if (buttons & (1 << i))
        {
            Display_fillRect(i * 40 + 5, 45, 30, 25, laneColors[i]);
        }
    }
}

void drawScore(uint32_t hits, uint32_t misses)
{
    char scoreText[35];

    Display_fillRect(0, 0, 160, 18, ST7735_BLACK);
    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_WHITE);
    Display_setCursor(4, 4);

    snprintf(scoreText, sizeof(scoreText),
             "Hits:%lu Miss:%lu",
             (unsigned long)hits, (unsigned long)misses);

    Display_print(scoreText);
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_GPDMA1_Init();
  MX_I2C1_Init();
  MX_ICACHE_Init();
  MX_SPI2_Init();
  MX_RNG_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  // Enable the DWT delay timer module
  DWT_Init();
  if(!DWT_CheckInitialized()) printf("DWT failed to initialize!\n\r");

  // Initialize the TFT shield object in code and on chip, with failure handling
  TFTShield18_create(&hi2c1);
  if (!TFTShield18_begin())
  {
    printf("Failed to initialize TFT shield!\n\r");
    return 1;
  } else {
    printf("TFT shield initialized successfully!\n\r");
  }

  // Software reset the entire TFT shield to restore it to a known empty startup state
  TFTShield18_tftReset(true);
  DWT_delayMs(100);

  // Initialize the display object and the display itself to support graphics operations. No error handling is
  // provided since there is no way to read the status from the display. Proper operation of the display and response
  // to graphics commands provide enough confidence that the display is working correctly.
  Display_create(&hspi2, DISP_CS_Pin, DISP_CS_GPIO_Port, DISP_DCX_SEL_Pin, DISP_DCX_SEL_GPIO_Port);
  Display_init(INITR_BLACKTAB);
  DWT_delayMs(100);

  // Set the display to be in landscape mode and clear the screen
  Display_setRotation(1);
  Display_fillScreen(ST77XX_WHITE);

  // Turn on the TFT shield backlight to its maximum value
  TFTShield18_setBacklight(TFTSHIELD_BACKLIGHT_ON);

  /* Place extra user setup code here */
  initGuitarButtons();
  buzzerDisable(&buzzer1);

  GameState gameState = WELCOME;
  uint32_t stateStart = 0;
  uint32_t lastCountdown = 3;
  uint32_t lastButtonState = TFTSHIELD_BUTTON_ALL;
  uint32_t noteIndex = 0;
  uint32_t noteStart = 0;
  uint8_t noteSounding = 0;
  uint32_t hits = 0;
  uint32_t misses = 0;
  uint8_t noteHit = 0;
  uint8_t lastGuitarButtons = 0;

  runTimer2();
  showWelcome();

  while (1){

  uint32_t now = timer;
  uint32_t buttons = TFTShield18_readButtons();

  uint8_t guitarButtons = getButtonStates();

  // Only detect newly pressed buttons
  uint8_t newPresses = guitarButtons & ~lastGuitarButtons;
  lastGuitarButtons = guitarButtons;

  // Detect when joystick SELECT is newly pressed
  uint8_t selectPressed = (lastButtonState & TFTSHIELD_BUTTON_IN) &&
                          !(buttons & TFTSHIELD_BUTTON_IN);

  if (selectPressed)
  {
      if (gameState == WELCOME)
      {
          gameState = COUNTDOWN;
          stateStart = timer;
          lastCountdown = 3;
          showCountdown(3);
      }
      else
      {
          gameState = WELCOME;
          buzzerDisable(&buzzer1);
          showWelcome();
      }
  }

  lastButtonState = buttons;

  // Countdown logic
  if (gameState == COUNTDOWN)
  {
      uint32_t elapsed = timer - stateStart;

      if (elapsed >= 300)
      {
        gameState = PLAYING;

        // Reset game progress
        hits = 0;
        misses = 0;
        noteHit = 0;
        noteIndex = 0;

        // Draw gameplay screen
        drawNoteLanes();
        drawCurrentNote(song[noteIndex].buttons);
        drawScore(hits, misses);

        // Start the first note
        noteStart = timer;
        playSongNote(noteIndex);
        noteSounding = 1;
      }
      else
      {
          uint32_t number = 3 - elapsed / 100;

          if (number != lastCountdown)
          {
              lastCountdown = number;
              showCountdown(number);
          }
      }
    }
      // Song playback
  if (gameState == PLAYING)
  {
        uint32_t elapsed = timer - noteStart;
    uint32_t duration = song[noteIndex].duration;

    // Check for correct button presses
    if (!noteHit && elapsed < duration)
    {
        uint8_t expectedButtons = song[noteIndex].buttons;

        // All required buttons must be held together
        // At least one must have been newly pressed
        if ((guitarButtons & expectedButtons) == expectedButtons &&
            (newPresses & expectedButtons))
        {
            hits++;
            noteHit = 1;
            drawScore(hits, misses);
        }
    }

    // Silence the final 8 ticks of the note
    if (noteSounding && (duration <= 8 || elapsed >= duration - 8))
    {
        buzzerDisable(&buzzer1);
        noteSounding = 0;
    }

    // Advance when the note finishes
    if (elapsed >= duration)
    {
        // Count a missed note
        if (!noteHit)
        {
            misses++;
        }

        noteIndex++;

        if (noteIndex >= SONG_LENGTH)
        {
            buzzerDisable(&buzzer1);
            gameState = WIN;  // Temporary result state
            showSongFinished();
        }
        else
        {
            // Reset hit detection for the next note
            noteHit = 0;

            noteStart += duration;
            playSongNote(noteIndex);
            noteSounding = 1;

            // Display the next note
            drawCurrentNote(song[noteIndex].buttons);
            drawScore(hits, misses);
          }
        }
      } // End of PLAYING state

      /* USER CODE END WHILE */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_CSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 45;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 6;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/**
  * @brief GPDMA1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPDMA1_Init(void)
{

  /* USER CODE BEGIN GPDMA1_Init 0 */

  /* USER CODE END GPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  /* GPDMA1 interrupt Init */
    HAL_NVIC_SetPriority(GPDMA1_Channel5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel5_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel7_IRQn);

  /* USER CODE BEGIN GPDMA1_Init 1 */

  /* USER CODE END GPDMA1_Init 1 */
  /* USER CODE BEGIN GPDMA1_Init 2 */

  /* USER CODE END GPDMA1_Init 2 */

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
  hi2c1.Init.Timing = 0x00707CBB;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache (default 2-ways set associative cache)
  */
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief RNG Initialization Function
  * @param None
  * @retval None
  */
static void MX_RNG_Init(void)
{

  /* USER CODE BEGIN RNG_Init 0 */

  /* USER CODE END RNG_Init 0 */

  /* USER CODE BEGIN RNG_Init 1 */

  /* USER CODE END RNG_Init 1 */
  hrng.Instance = RNG;
  hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RNG_Init 2 */

  /* USER CODE END RNG_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_1LINE;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 0x7;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi2.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi2.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1400;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 600;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DISP_DCX_SEL_GPIO_Port, DISP_DCX_SEL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DISP_CS_GPIO_Port, DISP_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : DISP_DCX_SEL_Pin */
  GPIO_InitStruct.Pin = DISP_DCX_SEL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DISP_DCX_SEL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DISP_CS_Pin */
  GPIO_InitStruct.Pin = DISP_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DISP_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08FFF000;
  MPU_InitStruct.LimitAddress = 0x08FFFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
#ifdef USE_FULL_ASSERT
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
