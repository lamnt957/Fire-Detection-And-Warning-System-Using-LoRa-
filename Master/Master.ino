
#include "Arduino.h"
#include "LoRa_E32.h"          
#include "UniversalTelegramBot.h"
#include <WiFiClientSecure.h>
#include "FirebaseESP32.h"
#include <HTTPClient.h>
#include "ESP_Mail_Client.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
const char* ssid = "Redmi9C"; // Tên WiFi của bạn
const char* password = "1234567890"; // Mật khẩu WiFi
#define telegramToken  "7628739655:AAFZGXdv_-630yTSjLVrTWys50AiWPl4m-k"// Token của bot Telegram
#define chat_id  "7610890544" // ID của người dùng hoặc nhóm Telegram
#define FIREBASE_HOST "https://doan-95b6b-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "T9oAILmcPqQZjGBBdgdeKR3OcHjLKoOGu8kzOmeW"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_587
#define AUTHOR_EMAIL "doansinhvienso23@gmail.com"
#define AUTHOR_PASSWORD "pjtx raub tytk oitn"
#define RECIPIENT_EMAIL  "doanchth111@gmail.com"
FirebaseData fbdo;
SMTPSession smtp;
WiFiClientSecure client;
UniversalTelegramBot bot(telegramToken, client);

LiquidCrystal_I2C lcd(0x27, 20, 4);
// Dữ liệu các node
String nodeData[3][5] = {
  {"Node1", "1", "1", "OK", "NO"},     // Node 1
  {"Node2", "2", "2", "Canh Bao", "YES"}, // Node 2
  {"Node3", "3", "3", "OK", "NO"},    // Node 3
 // {"Node4", "4", "4", "Canh Bao", "YES"}  // Node 4
 
};
// Dữ liệu cho nút nhấn
const int buttonPin = 2;  // Nút nhấn kết nối với chân 2
int buttonState = 0;      // Biến lưu trạng thái của nút nhấn
int lastButtonState = 0;  // Biến lưu trạng thái nút nhấn trước đó
int currentNode = 0;      // Biến lưu index node hiện tại
const int buzzerPin = 4; 

void handshaking();
void RevACK();
void RevMess();
boolean runEvery(unsigned long interval);
void sendEmail(String subject, String content,int canhBao);    
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
     
LoRa_E32 e32ttl(&Serial2, 5, 19,18); // M0 19, M1 18, Rx 17, Tx 16,Aux 5

struct Message 
{ 
  byte ID = 0;
  byte addl = 0x01;
  byte addh = 0x01;
  byte temp[2];
  byte humi[2];
  byte mq2Val[2];
  byte flameVal[2];
  byte connectingToMaster[4] = {0,0,0,0};
} message;

int isACK = 0;
String temp_,humi_,gas_,flame_;
int chek =1;

void setup()
{
  Serial.begin(115200);
  e32ttl.begin();
  lcd.init();               // Khởi tạo LCD
  lcd.backlight();          // Bật đèn nền LCD 
  ResponseStructContainer c;
  c = e32ttl.getConfiguration();
  Configuration configuration = *(Configuration *)c.data;

  configuration.ADDL = 0x1;
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
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Đang kết nối WiFi...");
  }
  Serial.println("Kết nối WiFi thành công!");
  client.setInsecure();
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(fbdo, 1000 * 60);
  Firebase.setwriteSizeLimit(fbdo, "tiny");


  pinMode(buttonPin, INPUT_PULLUP); // Cấu hình nút nhấn là đầu vào với điện trở kéo lên
  pinMode(buzzerPin, OUTPUT); // Thiết lập chân điều khiển Buzzer
  digitalWrite(buzzerPin, LOW); // Đảm bảo Buzzer tắt ban đầu
}


void loop()
{
  handshaking();
  while(1)
  { 
  
    for (int i = 1; i <= 4; i++) {
      Serial.print(message.connectingToMaster[i-1]); 
    }
    Serial.println();
    if (chek == 1){
      break;
    }
    for (int i = 1; i <= 4; i++) {
      if (message.connectingToMaster[i-1] == 1) {
        ResponseStatus rs = e32ttl.sendFixedMessage(0x00, i, 0x03, &message, sizeof(Message));  //send data request package 
        while((runEvery(120000) == 0))  {
          RevMess();
           // Kiểm tra trạng thái của nút nhấn
          buttonState = digitalRead(buttonPin);
        
          // Nếu nút nhấn đã được thả ra (có sự thay đổi trạng thái)
          if (buttonState == LOW && lastButtonState == HIGH) {
            currentNode = (currentNode + 1) % 4;  // Chuyển đến node kế tiếp, quay lại từ đầu nếu hết
            delay(200);  // Giới hạn tốc độ nhấn để tránh nhấn liên tục
            if (currentNode == 3){
              digitalWrite(buzzerPin, LOW); 
              currentNode = (currentNode + 1) % 4;
            }
          }
        
          lastButtonState = buttonState;  // Cập nhật trạng thái nút nhấn trước đó
        
          // Hiển thị và lưu dữ liệu từng dòng
          lcd.setCursor(0, 0);    // Dòng 1
          String line1 = "Ten: " + nodeData[currentNode][0];
          lcd.print(line1);
          lcd.setCursor(0, 1);    // Dòng 2
          lcd.print("                    "); 
          lcd.setCursor(0, 1);     
          String line2 = "Temp: " + nodeData[currentNode][1] + "C" + " Hum: " + nodeData[currentNode][2] + "%";
          lcd.print(line2); 
          lcd.setCursor(0, 2);    // Dòng 3
          lcd.print("                    "); 
          lcd.setCursor(0, 2);  
          String line3 = "Gas: " + nodeData[currentNode][3];
          lcd.print(line3);
          lcd.setCursor(0, 3);    // Dòng 4
          lcd.print("                    "); 
          lcd.setCursor(0, 3); 
          String line4 = "Flame: " + nodeData[currentNode][4];
          lcd.print(line4);
        }
      }
    }    
  } 
}


void handshaking() {
  for (int addr = 1; addr <= 4; addr++) {
    ResponseStatus rs = e32ttl.sendFixedMessage(0x00, addr, 0x03, &message, sizeof(Message));
    delay(1000);  
    rs = e32ttl.sendFixedMessage(0x00, addr, 0x03, &message, sizeof(Message)); 
    delay(200);   
    while(runEvery(5000) == 0) {
      RevACK();
      if (isACK) {
        isACK = 0;
        break;
      }
      delay(200);
    }     
  } 
}

void RevACK(){
  if (e32ttl.available() > 1) {
    ResponseStructContainer rsc = e32ttl.receiveMessage(sizeof(Message));
    struct Message rxACK = *(Message*)rsc.data;
    Serial.println(rxACK.ID);     
    if (rxACK.ID >= 1 && rxACK.ID <= 4) {
      message.connectingToMaster[rxACK.ID - 1] = 1;
      isACK = 1;
      chek =0;  
    } 
    free(rsc.data);
  }
}
void RevMess() {
  if (e32ttl.available()  > 1) {
    ResponseStructContainer rsc = e32ttl.receiveMessage(sizeof(Message));
    struct Message rxMess = *(Message*)rsc.data;
    Serial.println(rxMess.ID);
    Serial.println(*(short*)(rxMess.temp));
    Serial.println(*(short*)(rxMess.humi));
    Serial.println(*(short*)(rxMess.mq2Val));
    Serial.println(*(short*)(rxMess.flameVal));
    
    int temp  = *(short*)rxMess.temp;
    int humi  = *(short*)rxMess.humi;
    int gas   = *(short*)rxMess.mq2Val;
    int flame = *(short*)rxMess.flameVal;

    temp_ = String(temp);
    humi_ = String(humi);
    gas_  = String(gas);
    flame_= String(flame);
    
  String id="";
    if (rxMess.ID == 1)
    {      
       id = "Node 1";
         nodeData[0][1]=temp_;
         nodeData[0][2]=humi_;
         nodeData[0][3]=gas_;
         if (flame==1){
         nodeData[0][4]="Canh Bao chay";
           digitalWrite(buzzerPin, HIGH);         
         } else if (flame==0){
         nodeData[0][4]="Binh thuong";
         }  
        
    }
    else if (rxMess.ID == 2)
    {
       id = "Node 2";
       nodeData[1][1]=temp_;
         nodeData[1][2]=humi_;
         nodeData[1][3]=gas_;
         if (flame==1){
         nodeData[1][4]="Canh Bao chay";
           digitalWrite(buzzerPin, HIGH);
         } else if (flame==0){
         nodeData[1][4]="Binh thuong";
         }  
    }
    else if (rxMess.ID == 4)
    {
       id = "Node 3";
       nodeData[2][1]=temp_;
         nodeData[2][2]=humi_;
         nodeData[2][3]=gas_;
         if (flame==1){
         nodeData[2][4]="Canh Bao chay";
           digitalWrite(buzzerPin, HIGH);
         } else if (flame==0){
         nodeData[2][4]="Binh thuong";
         }  
    }

    Firebase.setString(fbdo, id + "/temp",temp_);
    Firebase.setString(fbdo, id + "/hum",humi_);
    Firebase.setString(fbdo, id + "/gas",gas_);
    Firebase.setString(fbdo, id + "/flame",flame_);

    
    if (flame == 1){
      if (WiFi.status() == WL_CONNECTED) {
        String message = "Có cháy ở: " + String(id);
        bot.sendMessage(chat_id, message, "Markdown");
        Serial.println("Đã gửi tin nhắn.");
        sendEmail("Cảnh Báo", " Có cháy ở ",rxMess.ID);
      }else {
          Serial.println("Mất kết nối WiFi!");
          // Thử kết nối lại WiFi
          WiFi.begin(ssid, password);
          while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.println("Đang cố gắng kết nối lại WiFi...");
          }
          Serial.println("Kết nối lại WiFi thành công!");
        }
    }

    delay(200);
    free(rsc.data);
    
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

void sendEmail(String subject, String content,int canhBao)
{
  Serial.println("Sending Email...");

  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = F("127.0.0.1");

  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  SMTP_Message message;

  message.sender.name = F("Me (canhbao)");
  message.sender.email = AUTHOR_EMAIL;

  message.subject = subject;

  message.addRecipient(F("Recipient Name"), RECIPIENT_EMAIL);

  message.text.content = content+ String(canhBao);
  message.text.transfer_encoding = "base64";
  message.text.charSet = F("utf-8");
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  message.addHeader(F("Message-ID: <doansinhvienso23@gmail.com>"));

  if (!smtp.connect(&config))
  {
    Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn())
  {
    Serial.println("Not yet logged in.");
  }
  else
  {
    if (smtp.isAuthenticated())
      Serial.println("Successfully logged in.");
    else
      Serial.println("Connected with no Auth.");
  }

  if (!MailClient.sendMail(&smtp, &message))
    Serial.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

  smtp.sendingResult.clear();

  Serial.println("Email Sent!");
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
