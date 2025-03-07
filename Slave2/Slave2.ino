
#include "Arduino.h"
#include "LoRa_E32.h"
#include "DHT.h"

//Prototype
void handshaking();
void RevRequest();
void readSensors();
void RevMess();
boolean runEvery(unsigned long interval);

#define DHTPIN 6         // Chân digital kết nối với cảm biến DHT11 (thay D2 bằng GPIO13)
#define DHTTYPE DHT11     // Loại cảm biến DHT (DHT11)
#define MQ2PIN  A1         // Chân analog kết nối với cảm biến MQ2 (thay A0 bằng GPIO34)
#define FIREPIN 3        // Chân digital kết nối với cảm biến cháy (thay D3 bằng GPIO23)
#define FIREPIN_1 5
DHT dht(DHTPIN, DHTTYPE);
LoRa_E32 e32ttl(7,8,13,2,4);
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
struct Message 
{
  byte ID = 2;
  byte addl = 0x02;
  byte addh = 0x00;
  byte temp[2];
  byte humi[2];
  byte mq2Val[2];
  byte flameVal[2];
  byte connectingToMaster[4] = {0,0,0,0};
} message;

int idOfFather = 0;
byte addlOfFather =1;
byte addhOfFather =1;

int firstRequest = 1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  e32ttl.begin();
 dht.begin(); 
  ResponseStructContainer c;
  c = e32ttl.getConfiguration();
  Configuration configuration = *(Configuration *)c.data;

  configuration.ADDL = 0x2;
  configuration.ADDH = 0x0;
  configuration.CHAN = 0x3;

  configuration.OPTION.fec = FEC_1_ON;
  configuration.OPTION.fixedTransmission = FT_FIXED_TRANSMISSION;
  configuration.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;
  configuration.OPTION.transmissionPower = POWER_20;      // 2'b10 24dBm
  configuration.OPTION.wirelessWakeupTime = WAKE_UP_1000; // 3'b011 1000ms

  configuration.SPED.airDataRate = AIR_DATA_RATE_011_48;  // 3'b011 4.8kbps
  configuration.SPED.uartBaudRate = UART_BPS_9600;
  configuration.SPED.uartParity = MODE_00_8N1;

  // Set configuration changed and set to not hold the configuration
  ResponseStatus rs = e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
  Serial.print("Lora Configuration: ");
 Serial.println(rs.getResponseDescription());

  c = e32ttl.getConfiguration();
  configuration = *(Configuration *)c.data;
  printParameters(configuration);
  c.close();
}


void loop() {
 
  handshaking(); 
  while(1)
  {
    RevRequest();
    int flame1 = digitalRead(FIREPIN);     // Cảm biến lửa cũ
    int flame2 = digitalRead(FIREPIN_1);   // Cảm biến lửa mới
    
    // Kiểm tra nếu có lửa (nếu một trong hai cảm biến phát hiện lửa)
    if (flame1 == LOW || flame2 == LOW) {
      *(short*)message.flameVal = 1; // Có lửa 
      ResponseStatus rs = e32ttl.sendFixedMessage(addhOfFather, addlOfFather, 0x03, &message, sizeof(Message));
      
    delay(500);
    }
  }
}



void handshaking() {
  while(runEvery(20000) == 0) {
    if (e32ttl.available() > 1) {
      ResponseStructContainer rsc = e32ttl.receiveMessage(sizeof(Message));
      struct Message rxACK = *(Message*)rsc.data;
      Serial.println(rxACK.ID);
      if (rxACK.ID == 0) {
        ResponseStatus rs = e32ttl.sendFixedMessage(0x01, 0x01, 0x03, &message, sizeof(Message));
        firstRequest = 0;
        idOfFather = 0;
        addlOfFather = 1;
        addhOfFather = 1;
      }    
      free(rsc.data);
    }
  }      
}

void RevRequest(){
  if (e32ttl.available() > 1) {
    ResponseStructContainer rsc = e32ttl.receiveMessage(sizeof(Message));
    struct Message rxMess = *(Message*)rsc.data;
    Serial.print("receive request from ");
    Serial.println(rxMess.ID);
    if (firstRequest == 1) {
      firstRequest = 0;
      idOfFather = rxMess.ID;
      addlOfFather = rxMess.addl;
      addhOfFather = rxMess.addh;
    }    
    if (rxMess.ID == idOfFather) {
      for (int i = 0; i < 4; i++)
        message.connectingToMaster[i] = rxMess.connectingToMaster[i];
      for (int i = 1; i <= 4; i++) {
        Serial.print(message.connectingToMaster[i-1]); 
      }
      readSensors();
      delay(500);
      ResponseStatus rs = e32ttl.sendFixedMessage(addhOfFather, addlOfFather, 0x03, &message, sizeof(Message));
      Serial.println("--");
      for (int i = 1; i <= 4; i++) {
        if ( (message.connectingToMaster[i-1] == 0) && (message.ID != i) && (idOfFather != i) ) {
          ResponseStatus rs1 = e32ttl.sendFixedMessage(0x00, i+1, 0x03, &message, sizeof(Message));
          int cnt=0;
          while (cnt != 10000) {
            cnt++;
            RevMess();
            delay(10);  
          }
        }
      } 
    }
 
    free(rsc.data);
  }
}



void readSensors() { 

  *(short*)message.temp     = dht.readTemperature(); 
  *(short*)message.humi     =   dht.readHumidity(); 
  *(short*)message.mq2Val   =   analogRead(MQ2PIN);  
  // Đọc giá trị từ hai cảm biến lửa và so sánh
  int flame1 = digitalRead(FIREPIN);     // Cảm biến lửa cũ
  int flame2 = digitalRead(FIREPIN_1); // Cảm biến lửa mới

  // Kiểm tra nếu có lửa (nếu một trong hai cảm biến phát hiện lửa)
  if (flame1 == LOW || flame2 == LOW) {
    // Có lửa, gán giá trị 1 vào flameVal
    *(short*)message.flameVal = 1; 
  } else {
    // Không có lửa, gán giá trị 0 vào flameVal
    *(short*)message.flameVal = 0; 
  }
 // *(short*)message.flameVal =   digitalRead(FIREPIN);
  Serial.print("Temp: ");
  Serial.println(*(short*)(message.temp));               
  Serial.print("Humi: ");
  Serial.println(*(short*)message.humi);
  Serial.print("MQ-2: ");
  Serial.println(*(short*)message.mq2Val);
  Serial.print("Digital Value FL: ");
  Serial.println( *(short*)message.flameVal);
}



void RevMess(){
  if (e32ttl.available() > 1) {
    ResponseStructContainer rsc = e32ttl.receiveMessage(sizeof(Message));
    struct Message rxMess = *(Message*)rsc.data;
    if (rxMess.ID != message.ID && rxMess.ID != idOfFather)
    {
    //nhan duoc tu slave va in ra
      Serial.print("receive mess from ");
      Serial.println(rxMess.ID);
      Serial.print("Temp: ");
      Serial.println(*(short*)(rxMess.temp));               
      Serial.print("Humi: ");
      Serial.println(*(short*)rxMess.humi);
      Serial.print("MQ-2: ");
      Serial.println(*(short*)rxMess.mq2Val);
      Serial.print("Digital Value FL: ");
      Serial.println( *(short*)rxMess.flameVal);
      //if (rxMess.ID >= 1 && rxMess.ID <= 4)
      delay(100);
      Serial.println( addhOfFather);
      Serial.println( addlOfFather);
      ResponseStatus rs = e32ttl.sendFixedMessage(addhOfFather, addlOfFather, 0x03, &rxMess, sizeof(Message)); 
      delay(200);
      rs = e32ttl.sendFixedMessage(addhOfFather, addlOfFather, 0x03, &rxMess, sizeof(Message)); 
      delay(300);
      free(rsc.data);
    }
  }
}



boolean runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
void printParameters(struct Configuration configuration) {
  Serial.println("----------------------------------------");

  Serial.print(F("HEAD : "));  Serial.print(configuration.HEAD, BIN);Serial.print(" ");Serial.print(configuration.HEAD, DEC);Serial.print(" ");Serial.println(configuration.HEAD, HEX);
  Serial.println(F(" "));
  Serial.print(F("AddH : "));  Serial.println(configuration.ADDH, DEC);
  Serial.print(F("AddL : "));  Serial.println(configuration.ADDL, DEC);
  Serial.print(F("Chan : "));  Serial.print(configuration.CHAN, DEC); Serial.print(" -> "); Serial.println(configuration.getChannelDescription());
  Serial.println(F(" "));
  Serial.print(F("SpeedParityBit     : "));  Serial.print(configuration.SPED.uartParity, BIN);Serial.print(" -> "); Serial.println(configuration.SPED.getUARTParityDescription());
  Serial.print(F("SpeedUARTDatte  : "));  Serial.print(configuration.SPED.uartBaudRate, BIN);Serial.print(" -> "); Serial.println(configuration.SPED.getUARTBaudRate());
  Serial.print(F("SpeedAirDataRate   : "));  Serial.print(configuration.SPED.airDataRate, BIN);Serial.print(" -> "); Serial.println(configuration.SPED.getAirDataRate());

  Serial.print(F("OptionTrans        : "));  Serial.print(configuration.OPTION.fixedTransmission, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getFixedTransmissionDescription());
  Serial.print(F("OptionPullup       : "));  Serial.print(configuration.OPTION.ioDriveMode, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getIODroveModeDescription());
  Serial.print(F("OptionWakeup       : "));  Serial.print(configuration.OPTION.wirelessWakeupTime, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
  Serial.print(F("OptionFEC          : "));  Serial.print(configuration.OPTION.fec, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getFECDescription());
  Serial.print(F("OptionPower        : "));  Serial.print(configuration.OPTION.transmissionPower, BIN);Serial.print(" -> "); Serial.println(configuration.OPTION.getTransmissionPowerDescription());

  Serial.println("----------------------------------------");

}
void printModuleInformation(struct ModuleInformation moduleInformation) {
  Serial.println("----------------------------------------");
  Serial.print(F("HEAD BIN: "));  Serial.print(moduleInformation.HEAD, BIN);Serial.print(" ");Serial.print(moduleInformation.HEAD, DEC);Serial.print(" ");Serial.println(moduleInformation.HEAD, HEX);

  Serial.print(F("Freq.: "));  Serial.println(moduleInformation.frequency, HEX);
  Serial.print(F("Version  : "));  Serial.println(moduleInformation.version, HEX);
  Serial.print(F("Features : "));  Serial.println(moduleInformation.features, HEX);
  Serial.println("----------------------------------------");

}
