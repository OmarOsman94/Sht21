/**************************************************************************//**
  \file app.c

  \brief Basis-Anwendung.

  \author Markus Krauﬂe

******************************************************************************/


#include <appTimer.h>
#include <zdo.h>
#include <app.h>
#include <sysTaskManager.h>
#include <usartManager.h>
#include <i2cpacket.h>
#include <bspLeds.h>
#include <leds.h>



static void APS_DataConf(APS_DataConf_t *confInfo);
void APS_DataInd(APS_DataInd_t *indData);
static void ZDO_StartNetworkConf(ZDO_StartNetworkConf_t *confirmInfo);

uint8_t sht21Data[3];
uint8_t uart_output[]= "{\"Panid\":\"0x1AAAAAAAAAAACAFELL\",\"UID\":\"0x0A01LL\",\"tmp\":+XXX.XXX,\"hmw\":XXX.XXX}\n\r";
uint8_t tempData[1] = {TRIGGER_TEMP_MEASUREMENT};
uint8_t hmdData[1] = {TRIGGER_HMD_MEASUREMENT};
static uint8_t deviceType;

static HAL_AppTimer_t receiveTimerLed;
static HAL_AppTimer_t transmitTimerLed;
static HAL_AppTimer_t timertemp;
static HAL_AppTimer_t timerhmd;
static HAL_AppTimer_t timerstart;

static APS_RegisterEndpointReq_t endPoint; //Um in BitCloud einen Anwendungsendpunkt zu registrieren
static APS_DataReq_t dataReq;  // bestimmt die  Adressierungsart, die Zieladresse, die Zielendpunkt ID und die Senderendpunkt ID
static App_Message_t transmitData;
static SimpleDescriptor_t simpleDescriptor;// spezifiziert die eigenschaften des endpunktes

static AppState_t appstate = APP_INIT_STATE;
static ZDO_StartNetworkReq_t networkParams;

static HAL_I2cDescriptor_t i2c={
	.tty = TWI_CHANNEL_0,
	.clockRate = I2C_CLOCK_RATE_62,
	.id = SENSOR_ADDRESS, 
	.lengthAddr = HAL_NO_INTERNAL_ADDRESS,
	
};


static void receiveTimerLedFired(void) {
	BSP_OffLed(LED_RED);
}

static void transmitTimerLedFired(void) {
	BSP_OffLed(LED_YELLOW);
}


void APS_DataInd(APS_DataInd_t *indData) { // nach dem empfangen eines Frames blinkt die rote led und printet die daten durch die uart-schnittstelle
	BSP_OnLed(LED_RED);						
	HAL_StartAppTimer(&receiveTimerLed);
	appWriteDataToUsart(indData->asdu, indData->asduLength);
	appWriteDataToUsart((uint8_t*) "\r\n", 2);
}


static void initEndPoint(void) {	// hier wird die end punkt intialisiren des endpunktes wird die callback-funktion "APS_DataInd" aufgerufen
	simpleDescriptor.AppDeviceId = 1;
	simpleDescriptor.AppProfileId = 1;
	simpleDescriptor.endpoint = 1;
	simpleDescriptor.AppDeviceVersion = 1;
	endPoint.simpleDescriptor = &simpleDescriptor;
	endPoint.APS_DataInd = APS_DataInd;
	APS_RegisterEndpointReq(&endPoint);
}


void ZDO_StartNetworkConf(ZDO_StartNetworkConf_t *confirmInfo) {  // Diese ist so implementiert, dass der Koordinator bei einem erfolgreichen
	if(ZDO_SUCCESS_STATUS == confirmInfo->status) {				//  Netzaufbau ¸ber die UART-Schnittstelle das Wort Coordinator ausgibt.
		CS_ReadParameter(CS_DEVICE_TYPE_ID, &deviceType);
		if(deviceType == DEV_TYPE_COORDINATOR) appWriteDataToUsart((uint8_t*) "Koordinator\r\n", sizeof("Koordinator\r\n")-1);
		} else {
		appWriteDataToUsart ((uint8_t*) "Fehler(Koordinator)\r\n", sizeof("Fehler(Koordinator)\r\n")-1);
	}
	SYS_PostTask (APL_TASK_ID);
}


static void APS_DataConf(APS_DataConf_t *confInfo) { // nach dem aufrufen dieser callback-funktion blinkt die gelbe led 
	if(confInfo->status == APS_SUCCESS_STATUS) {     // zum best‰tigung dass die daten erfolgreich gesendet werden.
		BSP_OnLed(LED_YELLOW);
		HAL_StartAppTimer(&transmitTimerLed);
		appstate = APP_NOTHING_STATE;
		SYS_PostTask(APL_TASK_ID);
	}
}

static void initTransmitData(void) {
	dataReq.profileId = 1;
	dataReq.dstAddrMode = APS_SHORT_ADDRESS;
	dataReq.dstAddress.shortAddress = CPU_TO_LE16(0);
	dataReq.dstEndpoint = 1;
	dataReq.asdu = transmitData.data;
	dataReq.asduLength = sizeof(transmitData.data);
	dataReq.srcEndpoint = 1;
	dataReq.APS_DataConf = APS_DataConf; // callback nach dem senden der data wird die callback-funktion "APS_DataConf" aufgerufen
}

static void writeSensorTemp(bool TrueFalse){ //report 3
	if(TrueFalse == true){
		HAL_StartAppTimer(&timertemp);
	}
	else{
		appstate = APP_NOTHING_STATE;
	}
}

static void writeSensorHumidity(bool TrueFalse){
	if(TrueFalse == true){
		HAL_StartAppTimer(&timerhmd);
	}
	else{
		appstate = APP_NOTHING_STATE;
	}
}
static void readSensorTemp(bool TrueFalse1){  //report 3
	if(TrueFalse1 == true){
		appstate = APP_OUTPUT_TEMP_SENSOR_STATE;
		SYS_PostTask(APL_TASK_ID);
	}
	
	else{
		appstate = APP_NOTHING_STATE;
	}
}

static void readSensorHumidity(bool TrueFalse1){
	if(TrueFalse1 == true){
		appstate = APP_OUTPUT_HMD_SENSOR_STATE;
		SYS_PostTask(APL_TASK_ID);
	}
	
	else{
		appstate = APP_NOTHING_STATE;
	}
}



static void calculateHumidity(void){
	float real_hmd;
	uint16_t hmd=0;

	hmd = sht21Data[0]<<8;
	hmd =hmd|sht21Data[1];
	hmd= hmd>>2;
	hmd=hmd<<2;
	
	real_hmd = -6+125*((float)hmd/65536);
	
	uint32_to_str(uart_output , sizeof(uart_output), real_hmd ,70,3);
	uint32_to_str(uart_output , sizeof(uart_output), ((real_hmd-(int16_t)real_hmd)*1000) ,74,3);
	appWriteDataToUsart(uart_output, sizeof(uart_output));
}

static void calculateTemp(void){  //report 3
	float real_temp;
	uint16_t temp;
	
	temp = sht21Data[0] <<8;
	temp= temp|sht21Data[1];
	temp= temp>>2;
	temp=temp<<2;
	
	real_temp =-46.85+175.72*( ((float)temp/65536));
	
	uart_output[55] = real_temp < 0 ? '-' : '+';
	
	uint32_to_str(uart_output , sizeof(uart_output), real_temp ,56,3);
	uint32_to_str(uart_output , sizeof(uart_output), ((real_temp-(int16_t)real_temp)*1000) ,60,3);

}



static void readtemp(void){			//report 3
	appstate = APP_READ_TEMP_SENSOR_STATE;
	SYS_PostTask(APL_TASK_ID);
}
static void readhmd(void){			//report 3
	appstate = APP_READ_HMD_SENSOR_STATE;
	SYS_PostTask(APL_TASK_ID);
}

static void startMessung(void){			
	appstate = APP_START_TEMP_SENSOR_STATE;
	SYS_PostTask(APL_TASK_ID);
}

static void initTimer(void){
	transmitTimerLed.interval = 500;
	transmitTimerLed.mode = TIMER_ONE_SHOT_MODE;
	transmitTimerLed.callback = transmitTimerLedFired;
		
	receiveTimerLed.interval = 500;
	receiveTimerLed.mode = TIMER_ONE_SHOT_MODE;
	receiveTimerLed.callback = receiveTimerLedFired;
	
	timertemp.interval = 85;
	timertemp.mode	 = TIMER_ONE_SHOT_MODE;
	timertemp.callback = readtemp;

	timerhmd.interval = 29;
	timerhmd.mode	 = TIMER_ONE_SHOT_MODE;
	timerhmd.callback = readhmd;
	
}


static void initTimerstart(void){
	timerstart.interval = 10000;
	timerstart.mode	 = TIMER_REPEAT_MODE;
	timerstart.callback = startMessung;
	HAL_StartAppTimer(&timerstart);
}


static void configure( void(*callback)(_Bool) , uint8_t data [], uint32_t length){
	i2c.f = callback;
	i2c.data = data;
	i2c.length = length;
}

static void initNetwork(){			//die callbak-funktion "ZDO_StartNetworkConf" wird aufgerufen sobald ein 							
									 // Netzwerkstart oder -eintritt mit einer Erfolgs- oder Fehlermeldung beendet wurde
	networkParams.ZDO_StartNetworkConf = ZDO_StartNetworkConf;
	ZDO_StartNetworkReq(&networkParams); //Durch den Aufruf der Funktion ZDO_StartNetworkReq(), weisen wir das funkmodul an, 
	initEndPoint();						// zu versuchen ein netzwerk zu starten
	initTransmitData();
}

void APL_TaskHandler(void){
	
	switch(appstate){  //report 3
		case APP_INIT_STATE:
		appInitUsartManager();
		initNetwork();
		BSP_OpenLeds ();
		initTimer();
		
		#if(CS_DEVICE_TYPE != DEV_TYPE_COORDINATOR)
		initTimerstart();
		#else
		appstate = APP_NOTHING_STATE ;
		#endif
		break;
		
		case APP_START_TEMP_SENSOR_STATE: //report 3
		if (-1 == HAL_OpenI2cPacket(&i2c)){ appWriteDataToUsart((uint8_t*)"open koordinator fail\n\r", sizeof("open koordinator fail\n\r")-1);}
		configure(writeSensorTemp,tempData,1);
		if (-1 == HAL_WriteI2cPacket(&i2c)){appWriteDataToUsart((uint8_t*)"Write fail\n\r", sizeof("write fail\n\r")-1);}
		appstate = APP_NOTHING_STATE;
		break;
		
		case APP_READ_TEMP_SENSOR_STATE: //report 3
		configure(readSensorTemp,sht21Data,3);
		if (-1 == HAL_ReadI2cPacket(&i2c)){appWriteDataToUsart((uint8_t*)"Read fail\n\r", sizeof("read fail\n\r")-1);}
		appstate = APP_NOTHING_STATE;
		break;
		
		case APP_OUTPUT_TEMP_SENSOR_STATE: //report 3
		calculateTemp();
		appstate = APP_START_HMD_SENSOR_STATE;
		SYS_PostTask(APL_TASK_ID);
		break;
		
		case APP_START_HMD_SENSOR_STATE:
		configure(writeSensorHumidity,hmdData,1);
		if (-1 == HAL_WriteI2cPacket(&i2c)){appWriteDataToUsart((uint8_t*)"Write fail\n\r", sizeof("write fail\n\r")-1);}
		appstate = APP_NOTHING_STATE;
		break;
		
		case APP_READ_HMD_SENSOR_STATE:
		configure(readSensorHumidity,sht21Data,3);
		if (-1 == HAL_ReadI2cPacket(&i2c)){appWriteDataToUsart((uint8_t*)"Read fail\n\r", sizeof("read fail\n\r")-1);}
		appstate = APP_NOTHING_STATE;
		break;
		
		case APP_OUTPUT_HMD_SENSOR_STATE:
		if(-1 == HAL_CloseI2cPacket(&i2c)){appWriteDataToUsart((uint8_t*)"Close fail\n\r", sizeof("Close fail\n\r")-1);}
		calculateHumidity();
		appstate = APP_TRANSMIT_STATE;
		SYS_PostTask(APL_TASK_ID);
		break;
		
		case APP_TRANSMIT_STATE:
		
		for(int i=0;i<sizeof(uart_output);i++){
			transmitData.data[i] =uart_output[i];
		}
		
		APS_DataReq(&dataReq); // durch den aufruf der APS_DataReq wir der Versand eingeleitet
		appstate = APP_NOTHING_STATE;
		break;
		
		case APP_NOTHING_STATE:
		break;
	}
}

/*******************************************************************************
  \brief The function is called by the stack to notify the application about 
  various network-related events. See detailed description in API Reference.
  
  Mandatory function: must be present in any application.

  \param[in] nwkParams - contains notification type and additional data varying
             an event
  \return none
*******************************************************************************/
void ZDO_MgmtNwkUpdateNotf(ZDO_MgmtNwkUpdateNotf_t *nwkParams)
{
  nwkParams = nwkParams;  // Unused parameter warning prevention
}

/*******************************************************************************
  \brief The function is called by the stack when the node wakes up by timer.
  
  When the device starts after hardware reset the stack posts an application
  task (via SYS_PostTask()) once, giving control to the application, while
  upon wake up the stack only calls this indication function. So, to provide 
  control to the application on wake up, change the application state and post
  an application task via SYS_PostTask(APL_TASK_ID) from this function.

  Mandatory function: must be present in any application.
  
  \return none
*******************************************************************************/
void ZDO_WakeUpInd(void)
{
}

#ifdef _BINDING_
/***********************************************************************************
  \brief The function is called by the stack to notify the application that a 
  binding request has been received from a remote node.
  
  Mandatory function: must be present in any application.

  \param[in] bindInd - information about the bound device
  \return none
 ***********************************************************************************/
void ZDO_BindIndication(ZDO_BindInd_t *bindInd)
{
  (void)bindInd;
}

/***********************************************************************************
  \brief The function is called by the stack to notify the application that a 
  binding request has been received from a remote node.

  Mandatory function: must be present in any application.
  
  \param[in] unbindInd - information about the unbound device
  \return none
 ***********************************************************************************/
void ZDO_UnbindIndication(ZDO_UnbindInd_t *unbindInd)
{
  (void)unbindInd;
}
#endif //_BINDING_

/**********************************************************************//**
  \brief The entry point of the program. This function should not be
  changed by the user without necessity and must always include an
  invocation of the SYS_SysInit() function and an infinite loop with
  SYS_RunTask() function called on each step.

  \return none
**************************************************************************/
int main(void)
{
  //Initialization of the System Environment
  SYS_SysInit();

  //The infinite loop maintaing task management
  for(;;)
  {
    //Each time this function is called, the task
    //scheduler processes the next task posted by one
    //of the BitCloud components or the application
    SYS_RunTask();
  }
}

//eof app.c
