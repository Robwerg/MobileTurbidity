/*
  Firmware for Turbidibuoy floating mobile pumped turbidity and water level sensor system
  Adapted from Paulo Silva mobile sediment sensor code 
  Forked by Axel Baylon axelbaylon@hotmail.com based off
  Kai James kaicjames@outlook.com code for Melt Sensors
*/
#include <SDI12.h>
#include <RTClib.h>
#include <Wire.h>
#include <SD.h>
#include <DFRobot_ADS1115.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTCZero.h>
#include <RH_RF95.h>
#include <QuickMedianLib.h>

#include "MobileTurbidityHelperFunctions.h"


#define TEST_LOOP             (false)     //Run test loop instead of actual loop
#define SLEEP_ENABLED         (true)    //Disable to keep serial coms alive for testing
#define FIRMWARE_VERSION      (001)
#define BV_OFFSET             (0.01)
#define OTT_OFFSET            (0.15)
#define ALS_POWER_WAIT        (1000) //Depends on probe. 1000 is safe
#define RTC_OFFSET_S          (12)
#define WRAP_AROUND_S_LOWER   (11)
#define WRAP_AROUND_S_UPPER   (49)
#define MAX_LOG_SIZE_BYTES    (1800000000)  //1.8GB. Max for some versions of FAT16
#define WRAP_AROUND_BUFF      (1)
#define ALS_AVE_COUNT         (11)  //Number of ADC reads for ALS averaging
#define ALS_AVE_DELAY         (0) //Period (mS) between ADC reads for ALS averaging. ALS takes around 10mS to poll regardless. 
#define TEMP_INIT_TIME        (100)
#define MAX_ALS_CAL_POINTS    (12)
#define MAX_REPEATED_NODES    (10)
#define RAIN_CUMULATIVE       (true)  //true is much better for telemetry monitoring
//Hardware pins
#define ONE_WIRE_BUS          (5)        //One wire temp sensors
#define ONE_WIRE_POWER        (2)        //One wire temp sensors
#define RAIN_GAUGE_INT        (9)
#define DATA_PIN              (12)          // The pin of the SDI-12 data bus
#define SD_SPI_CS             (A4)
#define USS_ECHO              (A2)
#define USS_TRIG              (A3)
#define USS_INIT_TIME         (50)
#define LED                   (13)
#define FET_POWER             (4)
#define BATT                  (A5)
//I2C Addressing
#define ADC_ADDR              (0x48)
#define RTC_ADDR              (0x68)  //Set automatically, here as a reminder
//Other
#define WATCHDOG_TIMER_MS     (11)   //Valid values: 0-11. 11 gives 16s timeout. 10 gives 8s timeout and so on  //resetWDT(); as necessary
#define LORA_FREQUENCY        (919.9)
#define LORA_BANDWIDTH        (125000)  //Increase to 250000 or 500000 for increased speed, less range. Linear change.

#define TURBIDITY_MOTOR_FORWARD (A0)
#define TURBIDITY_MOTOR_REVERSE_PIN (A1)
#define pumpspeed             (3) //D3 D4 D5 D9 are pwm output pins 8bit
#define TURB_DRY_READ    "TURBDRY.csv" // limited to 8 characters plus extension
#define TURB_WET_READ   "TURBRAW.csv" // limited to 8 characters plus extension
#define DEFAULT_PUMP_TIME 10 //forward pump time in seconds before wet read starts, to draw water into the sensor
#define DEFAULT_BACKFLUSH 10 //reverse pump time in seconds after wet read ends, to backflush the turbidity sensor
#define DEFAULT_READ_COUNT 10 //number of turbidity readings to take for median calculation
#define DEFAULT_MIN_H2O 20 // minimum water level in mm to allow turbidity wet read to occur
#define DEFAULT_TURB_READ_PERIOD 100 //period in ms between turbidity readings for median calculation

//CONFIG.txt variables
String NodeID;
double ALSslope = 1;
double ALSoffset = 0;
int LoRaPollOffset = 23;
int pollPerLoRa = 6;
String project = "TST";
String siteID = "AA";
float raingaugeTip_mm = 0.2;
bool LoRaEnabled = true;
bool SDEnabled = true;
int logFileSizeLast = 10;
int tpumptme = DEFAULT_PUMP_TIME;
int tbflshtm = DEFAULT_BACKFLUSH;
int dryReadTimes = DEFAULT_READ_COUNT;
int wetReadTimes = DEFAULT_READ_COUNT;
int minh2o = DEFAULT_MIN_H2O;
int turbPeriod = DEFAULT_TURB_READ_PERIOD;


const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

typedef struct {
  uint8_t sensorCount = 0;
  double measure[10];
  double measure_2[10];
  uint8_t pollPeriod = 240;
  uint8_t cyclesPerPoll;
  uint8_t cycleCount;
  DeviceAddress addr[4];
} value_t;
value_t value;

typedef struct {
  float temp[MAX_ALS_CAL_POINTS] = {15};
  float slope[MAX_ALS_CAL_POINTS] = {1.28783};
  float offset[MAX_ALS_CAL_POINTS] = { -1290.16};
} ALSCal_t;
ALSCal_t ALSCal;

typedef struct {
  value_t rainGauge;
  value_t temp;
  value_t RTCTemp;
  value_t ALS;
  value_t OTT;
  value_t USS;
} sensor_t;
sensor_t sensor;

//Initialise libraries
RH_RF95 rf95(12, 6);
uint8_t rfbuf[RH_RF95_MAX_MESSAGE_LEN];//
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);
DeviceAddress Thermometer;
DeviceAddress hello;

RTC_DS3231 rtc;
DFRobot_ADS1115 ads(&Wire);
SDI12 mySDI12(DATA_PIN);
RTCZero internalrtc;

int pollPeriod;
File logFile;
File configFile;
String UNIXtimestamp;
String normTimestamp;
String fileNameStr;
String dataString;
int sleep_now_time;
int sleep_remaining_s = 0;
uint8_t tx_count = 0;
char addr[5];
char hex_chars[] = "0123456789ABCDEF";
int Year;
bool setupLoop = true;
int logFileSize = 0;
// float nodeDesyncRatio;
int tarSec;
int loraPollCount = pollPerLoRa;
String CSVHeader;
int logIncrement = 97;  //ASCII lowercase a
int lastUpTime = 0;
int WakeTime = 0;
int SensWakeTime = 0;
bool dailyReset = false;
float rainfall_mm = 0;
volatile uint16_t tip_count = 0;
volatile bool rain_interrupt = false;

float voltageMeasurementArray[10];
float tempHousingMeasurementArray[10]; // lower number temp sensor
float tempWaterMeasurementArray[10]; // higher number temp sensor
float voltageMedian;
float tempHousingMedian;
float turbidity;
float turbidity_air_avg;

int adc2 = 0;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////    SETUP           ///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup () {
  setupWDT( WATCHDOG_TIMER_MS );
  analogReference(AR_INTERNAL2V23); //For internal battery level calculation
  pinMode(TURBIDITY_MOTOR_FORWARD, OUTPUT); // set pump pin to output
  pinMode(TURBIDITY_MOTOR_REVERSE_PIN, OUTPUT); // set reverse pump pin to output
  digitalWrite(TURBIDITY_MOTOR_FORWARD, LOW); // Turn off forward pump
  digitalWrite(TURBIDITY_MOTOR_REVERSE_PIN, LOW); // Turn off reverse pump

  pinMode(pumpspeed, OUTPUT); // set pump speed pin to output

    //SD card
  if (!SD.begin(SD_SPI_CS)) {
    crashNflash(1);  //10*10ms high period
  }

  if (battVoltUpdate() < 3.5) { // If battery less that 3.5v, sleep and reset via wdt
    rf95.init();
    rf95.sleep();
    debugWrite("Startup failed. Battery voltage = " + String(battVoltUpdate()) + " system will restart \n");
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __WFI();
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
  };

  
  
  sensor.RTCTemp.sensorCount = 1;
  hexstr(getChipId(), addr, sizeof(addr));  //Establish default node ID. Overwrite if specified in config
  NodeID = String(addr);
  pinMode(FET_POWER, OUTPUT);

  if (rf95.init() == false) {
    while (1) {
      delay(50);
      if (rf95.init()) {
        break;
      }
    }  //Watchdog will reset
  } else {
    rf95.setTxPower(23, false);
    rf95.setFrequency(LORA_FREQUENCY);
    rf95.setSignalBandwidth(LORA_BANDWIDTH); //500kHz
  }
  sendLoRaIgnore(String(NodeID) + ": sLoRa started");

  OneWireTempSetup(); //Must auto detect sensors BEFORE configRead()
  configRead();
  sendLoRaIgnore("Config updated");
  sendLoRaIgnore("forward pump time tpumptme = " + String(tpumptme) + " sec"); //ensuring tpumptme is pulled from config.
  sendLoRaIgnore("reverse pump time tbflshtm = " + String(tbflshtm) + " sec"); //ensuring tpumptme is pulled from config.
  sendLoRaIgnore("period between turbidity measures turbPeriod = " + String(turbPeriod) + "ms"); //ensuring turbPeriod is pulled from config.

  Wire.begin(); //Might not need this
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  //Ultra sonic
  pinMode(USS_TRIG, OUTPUT);
  pinMode(USS_ECHO, INPUT);
  digitalWrite(USS_TRIG, LOW);

  // //OTT probe
  // if (sensor.OTT.sensorCount > 0) {
  //   mySDI12.begin();
  // }
  ads.setAddr_ADS1115(ADS1115_IIC_ADDRESS0);   // 0x48
  ads.setGain(eGAIN_TWOTHIRDS);   // 2/3x gain
  ads.setMode(eMODE_SINGLE);       // single-shot mode
  ads.setRate(eRATE_860);          // 860 SPS
  ads.setOSMode(eOSMODE_SINGLE);   // Set to start a single-conversion
  ads.init();
  
  //Build CSV Header - optional sensors
  CSVHeader = "DATE_TIME,SITE_ID,NODE_ID,FW_VER,COUNT,UPTIME,FILE_SIZE,BATT_V,";
  for (int j = 0; j < sensor.temp.sensorCount ; j++) {
    CSVHeader +=  "Temp_" + String(j) + ",";
  }
  if (sensor.RTCTemp.sensorCount > 0) {
    CSVHeader += "RTCTemp,";
  }
  for (int j = 0; j < sensor.ALS.sensorCount ; j++) {
    CSVHeader += + "RawALS_" + String(j) + ",";
  }
  for (int j = 0; j < sensor.ALS.sensorCount ; j++) {
    CSVHeader += + "EstLevel_" + String(j) + ",";
  }
  if (sensor.USS.sensorCount > 0) {
    CSVHeader += "USS,";
  }

  // Dry Turbidity Median
  CSVHeader += "TURB_DRY,";

  // Wet Turbidity Median
  CSVHeader += "TURB_WET,";

  // Turbidity Housing Temp
  CSVHeader += "HOUSING_TEMP,";

  // Leak Sensor (ADC2)
  CSVHeader += "LEAK_SENSOR,";

  fileNameStr = fileNameGen(logIncrement);
  while (SD.exists((char*)fileNameStr.c_str()) && logIncrement < 122) {
    logIncrement++;
    fileNameStr = fileNameGen(logIncrement);
  }
  logIncrement--;
  fileNameStr = fileNameGen(logIncrement);
  logFile = SD.open((char*)fileNameStr.c_str());
  String currHeader = logFile.readStringUntil('\n');
  logFileSize = logFile.size();
  logFile.close();
  if (logFileSize == 0 && logFileSizeLast == 0) { //Quick check for SD driver crash
    systemReset();
  }
  logFileSizeLast = logFileSize;
  currHeader.trim(); //Remove whitespace. THIS IS NEEDED
  if ((currHeader != CSVHeader) || (logFileSize > MAX_LOG_SIZE_BYTES)) { //Reprint header if something has changed.
    logIncrement++;
    fileNameStr = fileNameGen(logIncrement);
    logFile = SD.open((char*)fileNameStr.c_str(), FILE_WRITE);
    logFile.println(CSVHeader);
    logFile.close();
  }

  sendLoRaIgnore("About to start RTC");
  //RTC
  if (! rtc.begin()) {
    crashNflash(2);
  }
  sendLoRaIgnore("RTC started");

  // testing callback for SD.h to write time metadata
  SdFile::dateTimeCallback(dateTime);

  DateTime now = rtc.now();
  int Hour;
  int Min;
  int Sec;
  int Day;
  char Month[12];
  uint8_t monthIndex;
  bool updateTime;
  sscanf(__TIME__, "%d:%d:%d", &Hour, &Min, &Sec);
  sscanf(__DATE__, "%s %d %d", Month, &Day, &Year);
  for (monthIndex = 0; monthIndex < 12; monthIndex++) {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  DateTime compileTime = DateTime(Year, monthIndex + 1, Day, Hour, Min, Sec);
  uint32_t uploadTime = compileTime.unixtime();
  uint32_t RTCTime = now.unixtime();
  if (RTCTime < uploadTime || now.year() < Year) {
    if (Sec >= 60 - RTC_OFFSET_S) {
      Min = Min + 1;
      Sec = Sec + RTC_OFFSET_S - 60;
    }
    else {
      Sec = Sec + RTC_OFFSET_S;
    }
    rtc.adjust(DateTime(Year, monthIndex + 1, Day, Hour, Min, Sec));
  }

  updatePollFreq(); //Calculate sensor poll frequencies
  tarSec = LoRaPollOffset % (pollPeriod * 60); //Gives us the target start second for polling


  internalrtc.begin(false);
  internalrtc.attachInterrupt(wake_from_sleep);



  resetWDT();
  sendLoRaIgnore("Going to sleep to sync time");
  if (!TEST_LOOP) {
    sleepTillSynced();
  }
  String tmpStr = String("Node ") + NodeID + " ready for work. Time offset " + String(LoRaPollOffset, DEC) + " seconds";
  sendLoRaIgnore(tmpStr); //Quick message to say we've woken up


  setupWDT( WATCHDOG_TIMER_MS ); // initialize and activate WDT with maximum period

  buildTimestamps();
  debugWrite(String("Node ") + NodeID + " successfuly started. Start time: "+normTimestamp+"\n");

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////      LOOP           ///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop () {
  while (TEST_LOOP) {
    disableWatchdog();
    digitalWrite(LED, HIGH);
  }

  resetWDT();
  
  if (battVoltUpdate() < 3.5) { // If battery less that 3.5v, sleep and reset via wdt
    debugWrite("Loop restart at: " + normTimestamp +". Battery voltage = " + String(battVoltUpdate()) + " system will restart \n");
    systemReset();};
  wake_system();

  tx_count++;

  DateTime now;
  buildTimestamps();

  if (sensor.temp.sensorCount > 0) {
    if (sensor.temp.cycleCount == 1) { //Only test if we have woken the sensor and reset counter
      if (!tempUpdate()) {  //This and subsequent blank if statements are used in problem solving. Can defs remove them
      }
    }
  }
  if (sensor.RTCTemp.sensorCount > 0) {
    if (sensor.RTCTemp.cycleCount == 1) { //Only test if we have woken the sensor and reset counter
      if (!RTCTempUpdate()) {  //This and subsequent blank if statements are used in problem solving. Can defs remove them
      }
    }
  }

  if (sensor.ALS.sensorCount > 0) {
    if (sensor.ALS.cycleCount == 1) { //Only test if we have woken the sensor and reset counter
      if (!ALSUpdate()) {  //This and subsequent blank if statements are used in problem solving. Can defs remove them
      }
    }
  }

  if (sensor.USS.sensorCount > 0) {
    if (sensor.USS.cycleCount == 1) { //Only test if we have woken the sensor and reset counter
      if (!USSUpdate()) {  //This and subsequent blank if statements are used in problem solving. Can defs remove them
      }
    }
  }


  adc2 = ads.readVoltage(2); 

  if (sensor.ALS.measure_2[0] > minh2o) {
    analogWrite(pumpspeed, 255); //max 255
    sendLoRaIgnore("begin dry read");
    //delay(1000);
    TurbMeasure(TURB_DRY_READ, dryReadTimes);  //Make dry read and wet read configurable
    turbidity_air_avg = voltageMedian; // use raw voltage, do not calculate NTU
    sendLoRaIgnore("median dry read voltage = " + (String(turbidity_air_avg)) + " V");
    sendLoRaIgnore("end of dry read & pump starts sampling");
    delay(10);
    digitalWrite(TURBIDITY_MOTOR_FORWARD, HIGH);// Run the pump forward for tpumptme seconds
    sendLoRaIgnore("turn on forward pump for " + (String(tpumptme)) + " sec");
    forwardPump();
    sendLoRaIgnore("begin wet read");
    //delay(1000);
    TurbMeasure(TURB_WET_READ, wetReadTimes);
    turbidity = voltageMedian; // use raw voltage, do not calculate NTU
    sendLoRaIgnore("median wet read voltage = " + (String(turbidity)) + " V");
    //turbidity = QuickMedian<float>::GetMedian(voltageMeasurementArray, sizeof(voltageMeasurementArray) / sizeof(float)); //RJ added 18/05/2026
    sendLoRaIgnore("turn off forward pump");
    //delay(1000);
    digitalWrite(TURBIDITY_MOTOR_FORWARD, LOW); // Turn off forward pump
    delay(500); // one second delay before running pump in reverse
    sendLoRaIgnore("turn on reverse pump for " + (String(tbflshtm)) + " sec");
    //delay(1000);
    digitalWrite(TURBIDITY_MOTOR_REVERSE_PIN, HIGH); // Turn on pump reverse for tbflshtm seconds
    reversePump();
    digitalWrite(TURBIDITY_MOTOR_REVERSE_PIN, LOW); // Turn off reverse pump
    sendLoRaIgnore("turn off reverse pump");
    delay(10);
  } else {
    debugWrite(normTimestamp+": Not enough water, skipping Turbidity measure \n");
    }


  sleep_system();
  buildCSVDataString();

  if (SDEnabled) {
    logDataToSD();
  }

  if (LoRaEnabled) {
    if (loraPollCount >= pollPerLoRa) {
      loraPollCount = 0;
      LoRaUpdate();
    }
    loraPollCount++;
  }

  rf95.sleep();
  delay(20);    //Delay 20ms to ensure the chips have gone to sleep before powering off the board
  disableWatchdog(); // disable watchdog
  rain_interrupt = false;
  bool first_loop = true;
  now = rtc.now();
  lastUpTime = millis() - WakeTime;
  while ((first_loop || rain_interrupt)) {
    if (first_loop) { //REMEMBER first loop is relative to this wake cycle considering rain interupt. NOT first_loop for the whole system.
      first_loop = false;
      if ((tarSec < WRAP_AROUND_S_LOWER) && (now.second() >= 30))  { //Wrap down case
        sleep_remaining_s = pollPeriod * 60 + tarSec - (-60 + now.second()) + 1 + WRAP_AROUND_BUFF;
      }
      else if ((tarSec > WRAP_AROUND_S_UPPER) && (now.second() <= 30)) { //Wrap up case
        sleep_remaining_s = pollPeriod * 60 + tarSec - (60 + now.second()) - 1 + WRAP_AROUND_BUFF;
      }
      else {  //Standard time correction
        sleep_remaining_s = pollPeriod * 60 + tarSec - now.second() + WRAP_AROUND_BUFF;
      }
    }
    rain_interrupt = false;
    sleep();
  }
  WakeTime = millis();  //For wake period calculation
  setupWDT( WATCHDOG_TIMER_MS ); // initialize and activate WDT with maximum period
  sendLoRaIgnore("time at end of loop: " + String(millis()) + " ms");
}

