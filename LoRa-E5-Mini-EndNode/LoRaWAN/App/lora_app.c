/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lora_app.c
  * @author  MCD Application Team
  * @brief   Application of the LRWAN Middleware
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "Region.h" /* Needed for LORAWAN_DEFAULT_DATA_RATE */
#include "sys_app.h"
#include "lora_app.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "lora_app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "stm32_lpm.h"
#include "adc_if.h"
#include "sys_conf.h"

/* USER CODE BEGIN Includes */
#include "usart.h"
#include "board_resources.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/**
  * @brief LoRa State Machine states
  */
typedef enum TxEventType_e
{
  /**
    * @brief Appdata Transmission issue based on timer every TxDutyCycleTime
    */
  TX_ON_TIMER,
  /**
    * @brief Appdata Transmission external event plugged on OnSendEvent( )
    */
  TX_ON_EVENT
  /* USER CODE BEGIN TxEventType_t */

  /* USER CODE END TxEventType_t */
} TxEventType_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  LoRa End Node send request
  */
static void SendTxData(void);

/**
  * @brief  TX timer callback function
  * @param  context ptr of timer context
  */
static void OnTxTimerEvent(void *context);

/**
  * @brief  join event callback function
  * @param  joinParams status of join
  */
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);

/**
  * @brief  tx event callback function
  * @param  params status of last Tx
  */
static void OnTxData(LmHandlerTxParams_t *params);

/**
  * @brief callback when LoRa application has received a frame
  * @param appData data received in the last Rx
  * @param params status of last Rx
  */
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/*!
 * Will be called each time a Radio IRQ is handled by the MAC layer
 *
 */
static void OnMacProcessNotify(void);

/* USER CODE BEGIN PFP */
/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

/**
  * @brief  LED Tx timer callback function
  * @param  LED context
  * @retval none
  */
static void OnTxTimerLedEvent(void *context);

/**
  * @brief  LED Rx timer callback function
  * @param  LED context
  * @retval none
  */
static void OnRxTimerLedEvent(void *context);

/**
  * @brief  LED Join timer callback function
  * @param  LED context
  * @retval none
  */
static void OnJoinTimerLedEvent(void *context);

/**
  * @brief  join event callback function
  * @param  params
  * @retval none
  */

/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

/**
  * @brief LoRaWAN handler Callbacks
  */
static LmHandlerCallbacks_t LmHandlerCallbacks =
{
  .GetBatteryLevel =           GetBatteryLevel,
  .GetTemperature =            GetTemperatureLevel,
  .GetUniqueId =               GetUniqueId,
  .GetDevAddr =                GetDevAddr,
  .OnMacProcess =              OnMacProcessNotify,
  .OnJoinRequest =             OnJoinRequest,
  .OnTxData =                  OnTxData,
  .OnRxData =                  OnRxData
};

/**
  * @brief LoRaWAN handler parameters
  */
static LmHandlerParams_t LmHandlerParams =
{
  .ActiveRegion =             ACTIVE_REGION,
  .DefaultClass =             LORAWAN_DEFAULT_CLASS,
  .AdrEnable =                LORAWAN_ADR_STATE,
  .TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
  .PingPeriodicity =          LORAWAN_DEFAULT_PING_SLOT_PERIODICITY
};

/**
  * @brief Type of Event to generate application Tx
  */
static TxEventType_t EventType = TX_ON_TIMER;

/**
  * @brief Timer to handle the application Tx
  */
static UTIL_TIMER_Object_t TxTimer;

/* USER CODE BEGIN PV */
/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

/**
  * @brief User application buffer
  */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];

/**
  * @brief User application data structure
  */
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };

/**
  * @brief Specifies the state of the application LED
  */
static uint8_t AppLedStateOn = RESET;

/**
  * @brief Specifies the number of minutes TX interval
  */
static uint16_t AppTxIntervalMinutes = 0;


/**
  * @brief Timer to handle the application Tx Led to toggle
  */
static UTIL_TIMER_Object_t TxLedTimer;

/**
  * @brief Timer to handle the application Rx Led to toggle
  */
static UTIL_TIMER_Object_t RxLedTimer;

/**
  * @brief Timer to handle the application Join Led to toggle
  */
static UTIL_TIMER_Object_t JoinLedTimer;

/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void LoRaWAN_Init(void)
{
  /* USER CODE BEGIN LoRaWAN_Init_1 */
  /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

	#if defined (STATUS_LED_ENABLE) && (STATUS_LED_ENABLE == 1)
	  /* Enable status LEDs */
		SYS_LED_Init(SYS_LED_BLUE);
	#elif !defined (STATUS_LED_ENABLE)
	#error STATUS_LED_ENABLE not defined
	#endif /* STATUS_LED_ENABLE */
  SYS_LED_DeInit(SYS_LED2);

  /* Get LoRa APP version*/
  APP_LOG(TS_OFF, VLEVEL_M, "APP_VERSION:        V%X.%X.%X\r\n",
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORA_APP_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Get MW LoraWAN info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION: V%X.%X.%X\r\n",
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__LORAWAN_VERSION >> __APP_VERSION_SUB2_SHIFT));

  /* Get MW SubGhz_Phy info */
  APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:   V%X.%X.%X\r\n",
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_MAIN_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB1_SHIFT),
          (uint8_t)(__SUBGHZ_PHY_VERSION >> __APP_VERSION_SUB2_SHIFT));

  #if defined (STATUS_LED_ENABLE) && (STATUS_LED_ENABLE == 1)
	  /* Enable status LEDs */
  	  UTIL_TIMER_Create(&TxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
  	  UTIL_TIMER_Create(&RxLedTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
	  UTIL_TIMER_Create(&JoinLedTimer, 0xFFFFFFFFU, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);
	  UTIL_TIMER_SetPeriod(&TxLedTimer, 500);
	  UTIL_TIMER_SetPeriod(&RxLedTimer, 500);
	  UTIL_TIMER_SetPeriod(&JoinLedTimer, 500);
  #elif !defined (STATUS_LED_ENABLE)
  #error STATUS_LED_ENABLE not defined
  #endif /* STATUS_LED_ENABLE */


  /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
  /* USER CODE END LoRaWAN_Init_1 */

  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LmHandlerProcess), UTIL_SEQ_RFU, LmHandlerProcess);
  UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), UTIL_SEQ_RFU, SendTxData);
  /* Init Info table used by LmHandler*/
  LoraInfo_Init();

  /* Init the Lora Stack*/
  LmHandlerInit(&LmHandlerCallbacks);

  LmHandlerConfigure(&LmHandlerParams);

  /* USER CODE BEGIN LoRaWAN_Init_2 */
  /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

#if defined (STATUS_LED_ENABLE) && (STATUS_LED_ENABLE == 1)
	  /* Enable join status LED */
  	  //UTIL_TIMER_Start(&JoinLedTimer);
  #elif !defined (STATUS_LED_ENABLE)
  #error STATUS_LED_ENABLE not defined
  #endif /* STATUS_LED_ENABLE */


  /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
  /* USER CODE END LoRaWAN_Init_2 */

  LmHandlerJoin(ActivationType);

  if (EventType == TX_ON_TIMER)
  {
    /* send every time timer elapses */
    UTIL_TIMER_Create(&TxTimer,  0xFFFFFFFFU, UTIL_TIMER_ONESHOT, OnTxTimerEvent, NULL);
    UTIL_TIMER_SetPeriod(&TxTimer,  APP_TX_DUTYCYCLE);
    UTIL_TIMER_Start(&TxTimer);
  }
  else
  {
    /* USER CODE BEGIN LoRaWAN_Init_3 */
	/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

	SYS_PB_Init(SYS_BUTTON1, SYS_BUTTON_MODE_EXTI);

	/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
    /* USER CODE END LoRaWAN_Init_3 */
  }

  /* USER CODE BEGIN LoRaWAN_Init_Last */

  /* USER CODE END LoRaWAN_Init_Last */
}

/* USER CODE BEGIN PB_Callbacks */
/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

/* Note: Current MX does not support EXTI IP neither BSP. */
/* In order to get a push button IRS by code automatically generated */
/* this function is today the only available possibility. */
/* Calling BSP_PB_Callback() from here it shortcuts the BSP. */
/* If users wants to go through the BSP, it can remove BSP_PB_Callback() from here */
/* and add a call to BSP_PB_IRQHandler() in the USER CODE SESSION of the */
/* correspondent EXTIn_IRQHandler() in the stm32wlxx_it.c */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case  SYS_BUTTON1_PIN:
      /* Note: when "EventType == TX_ON_TIMER" this GPIO is not initialised */
      UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);
      break;
     default:
      break;
  }
 }
/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
/* USER CODE END PB_Callbacks */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
  /* USER CODE BEGIN OnRxData_1 */
  /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

  if ((appData != NULL) && (params != NULL))
  {
	#if defined (STATUS_LED_ENABLE) && (STATUS_LED_ENABLE == 1)
	  /* Enable Rx status LED */
	  //SYS_LED_On(SYS_LED_BLUE);
	  //UTIL_TIMER_Start(&RxLedTimer);
	#elif !defined (STATUS_LED_ENABLE)
	#error STATUS_LED_ENABLE not defined
	#endif /* STATUS_LED_ENABLE */



	static const char *slotStrings[] = { "1", "2", "C", "C Multicast", "B Ping-Slot", "B Multicast Ping-Slot" };

	APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Indication ==========\r\n");


	APP_LOG(TS_OFF, VLEVEL_M, "###### D/L FRAME:%04d\r\n",
			params->DownlinkCounter);
	APP_LOG(TS_OFF, VLEVEL_M, "###### SLOT:%s\r\n",
			slotStrings[params->RxSlot]);

	APP_LOG(TS_OFF, VLEVEL_M, "###### PORT:%d\r\n",
			appData->Port);
	APP_LOG(TS_OFF, VLEVEL_M, "###### DR:%d\r\n",
			params->Datarate);

	APP_LOG(TS_OFF, VLEVEL_M, "###### RSSI:%d | SNR:%d\r\n",
			params->Rssi, params->Snr);

	switch (appData->Port)
	{
	  case LORAWAN_SWITCH_CLASS_PORT:
		/*this port switches the class*/
		if (appData->BufferSize == 1)
		{
		  switch (appData->Buffer[0])
		  {
			case 0:
			{
			  LmHandlerRequestClass(CLASS_A);
			  break;
			}
			case 1:
			{
			  LmHandlerRequestClass(CLASS_B);
			  break;
			}
			case 2:
			{
			  LmHandlerRequestClass(CLASS_C);
			  break;
			}
			default:
			  break;
		  }
		}
		break;
	  case LORAWAN_USER_APP_PORT:
		if (appData->BufferSize == 2)
		{
			uint16_t configValue = appData->Buffer[1] | (appData->Buffer[0] << 8);
			APP_LOG(TS_OFF, VLEVEL_M, "Config Tx interval minutes:%u\r\n", (unsigned int)configValue);
			if(configValue > 0)
			{
				AppTxIntervalMinutes = configValue;
				UTIL_TIMER_SetPeriod(&TxTimer,  AppTxIntervalMinutes * 60 * 1000);
				UTIL_TIMER_Start(&TxTimer);
			}
		}
		break;
	  default:
		break;
	}
  }

  /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
  /* USER CODE END OnRxData_1 */
}

static void SendTxData(void)
{
  /* USER CODE BEGIN SendTxData_1 */
  /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
  uint16_t distance = 0;
  int16_t temperature = 0;
  uint16_t voltage = 0;
  UTIL_TIMER_Time_t nextTxIn = 0;
  uint8_t bufferIndex = 0;

  APP_LOG(TS_ON, VLEVEL_M, "\r\n UART: NOW READ\r\n");

  if (readUltraSonicDistance()) {
	  distance = getLastReading();
  } else {
	  // Should send error message over LoRaWAN.
	  APP_LOG(TS_OFF, VLEVEL_M, "\r\n UART: read failed\r\n");
  }

  temperature = SYS_GetTemperatureLevel();
  // Recover 2 byte representation.
  temperature >>= 8;
  voltage = SYS_GetBatteryLevel();

  AppData.Port = LORAWAN_USER_APP_PORT;
  // Split measurements into bytes and prepare buffer.
  AppData.Buffer[bufferIndex++] = (uint8_t)((distance >> 8) & 0xFF);
  AppData.Buffer[bufferIndex++] = (uint8_t)(distance & 0xFF);
  AppData.Buffer[bufferIndex++] = (uint8_t)((temperature >> 8) & 0xFF);
  AppData.Buffer[bufferIndex++] = (uint8_t)(temperature & 0xFF);
  AppData.Buffer[bufferIndex++] = (uint8_t)((voltage >> 8) & 0xFF);
  AppData.Buffer[bufferIndex++] = (uint8_t)(voltage & 0xFF);

  AppData.BufferSize = bufferIndex;

  if (LORAMAC_HANDLER_SUCCESS == LmHandlerSend(&AppData, LORAMAC_HANDLER_UNCONFIRMED_MSG, &nextTxIn, false))
  {
	APP_LOG(TS_ON, VLEVEL_L, "SEND REQUEST\r\n");
  }
  else if (nextTxIn > 0)
  {
	APP_LOG(TS_ON, VLEVEL_L, "Next Tx in  : ~%d minutes(s)\r\n", (nextTxIn / 60 / 1000));
  }

  /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
  /* USER CODE END SendTxData_1 */
}

static void OnTxTimerEvent(void *context)
{
  /* USER CODE BEGIN OnTxTimerEvent_1 */

  /* USER CODE END OnTxTimerEvent_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LoRaSendOnTxTimerOrButtonEvent), CFG_SEQ_Prio_0);

  /*Wait for next tx slot*/
  UTIL_TIMER_Start(&TxTimer);
  /* USER CODE BEGIN OnTxTimerEvent_2 */

  /* USER CODE END OnTxTimerEvent_2 */
}

/* USER CODE BEGIN PrFD_LedEvents */
/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

static void OnTxTimerLedEvent(void *context)
{
#if defined(USE_BSP_DRIVER)
  //BSP_LED_Off(LED_GREEN) ;
#else
  // SYS_LED_Off(SYS_LED_GREEN) ;
#endif
}

static void OnRxTimerLedEvent(void *context)
{
#if defined(USE_BSP_DRIVER)
  //BSP_LED_Off(LED_BLUE) ;
#else
  //SYS_LED_Off(SYS_LED_BLUE) ;
#endif
}

static void OnJoinTimerLedEvent(void *context)
{
#if defined(USE_BSP_DRIVER)
  //BSP_LED_Toggle(LED_RED) ;
#else
  //SYS_LED_Toggle(SYS_LED_RED) ;
#endif
}

/* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
/* USER CODE END PrFD_LedEvents */

static void OnTxData(LmHandlerTxParams_t *params)
{
  /* USER CODE BEGIN OnTxData_1 */
  /* USER CODE END OnTxData_1 */
}

static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
  /* USER CODE BEGIN OnJoinRequest_1 */
  /* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */

  if (joinParams != NULL)
  {
	if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
	{
	  UTIL_TIMER_Stop(&JoinLedTimer);

#if defined(USE_BSP_DRIVER)
	  //BSP_LED_Off(LED_RED) ;
#elif defined(MX_BOARD_PSEUDODRIVER)
	  // SYS_LED_Off(SYS_LED_RED) ;
#endif /* USE_BSP_DRIVER || MX_BOARD_PSEUDODRIVER */

	  APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOINED = ");
	  if (joinParams->Mode == ACTIVATION_TYPE_ABP)
	  {
		APP_LOG(TS_OFF, VLEVEL_M, "ABP ======================\r\n");
	  }
	  else
	  {
		APP_LOG(TS_OFF, VLEVEL_M, "OTAA =====================\r\n");
	  }
	}
	else
	{
	  APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOIN FAILED\r\n");
	}
  }

  /* ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ */
  /* USER CODE END OnJoinRequest_1 */
}

static void OnMacProcessNotify(void)
{
  /* USER CODE BEGIN OnMacProcessNotify_1 */
  /* USER CODE END OnMacProcessNotify_1 */
  UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_LmHandlerProcess), CFG_SEQ_Prio_0);

  /* USER CODE BEGIN OnMacProcessNotify_2 */
  /* USER CODE END OnMacProcessNotify_2 */
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
