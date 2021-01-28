/**************************************************************************//**
  \file app.h

  \brief Usart-Anwendung Headerdatei.

  \author
    Markus Krauße

******************************************************************************/

#ifndef _APP_H
#define _APP_H

#define SENSOR_ADDRESS				0x40
#define TRIGGER_TEMP_MEASUREMENT	0xE3
#define TRIGGER_HMD_MEASUREMENT		0xE5



typedef struct _AppMessage_t {  // da definieren wir nur die größe der speicherblock und sein header und footer.
	uint8_t header[APS_ASDU_OFFSET]; // header	
	uint8_t data[90];
	uint8_t footer[APS_AFFIX_LENGTH - APS_ASDU_OFFSET];	// footer
} PACK App_Message_t;

typedef enum{
	APP_INIT_STATE,
	APP_START_TEMP_SENSOR_STATE,
	APP_READ_TEMP_SENSOR_STATE,
	APP_OUTPUT_TEMP_SENSOR_STATE,
	APP_START_HMD_SENSOR_STATE,
	APP_READ_HMD_SENSOR_STATE,
	APP_OUTPUT_HMD_SENSOR_STATE,
	APP_TRANSMIT_STATE,
	APP_NOTHING_STATE
} AppState_t;
#endif
