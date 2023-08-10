//<-датчик температры/////////////////////////////////////////////

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 0 // Пин подключения OneWire шины, 0 (D3)
//#define ONE_WIRE_BUS 15 // Пин подключения OneWire шины, 0 (D3)

OneWire oneWire(ONE_WIRE_BUS); // Подключаем бибилотеку OneWire
DallasTemperature sensors(&oneWire); // Подключаем бибилотеку DallasTemperature

DeviceAddress temperatureSensors[5]; // Размер массива определяем исходя из количества установленных датчиков
uint8_t deviceCount = 0;

//датчик температры->/////////////////////////////////////////////


#define DATA_PIN    12 // пин данных
#define LATCH_PIN   13 // пин защелки
#define CLOCK_PIN   15 // пин тактов синхронизации


//<-дисплэй
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define WIRE Wire

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &WIRE);
//дисплэй->



#include <GyverPortal.h>
#include <EEPROM.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

GyverPortal portal;

struct SettingsESP { // создаём ярлык myStruct
  char ssid[20];
  char pass[20];
  char mdns[20];
  char loginweb[20];
  char password[20];
  bool MQTT_Enable;
  char MQTTServer[25];
  char MQTTLogin[25];
  char MQTTPwd[10];
  char MQTTid[20];
  byte registerStates[16];
};

SettingsESP Settings;


#include "Arduino.h"
#include "EspMQTTClient.h" /* https://github.com/plapointe6/EspMQTTClient */
                           /* https://github.com/knolleary/pubsubclient */
//#define PUB_DELAY (5 * 1000) /* 5 seconds */
#define PUB_DELAY (100 * 1000) /* 5 seconds */

EspMQTTClient client(
  Settings.ssid,
  Settings.pass,
  Settings.MQTTServer,
  Settings.MQTTLogin,
  Settings.MQTTPwd,
  Settings.MQTTid
);

bool SettingMode = false;

byte SettingsPin = 14;//D5 пин настроек

unsigned long lastTime = 0;
unsigned long timerDelay = 1000;
unsigned long timerDelay2000 = 2000;

String WiFiOptionForSelect="";

//RX 433
#define RadioModuleData 2 //На D4 вешаем приёмник

#include <Gyver433.h>

Gyver433_RX<RadioModuleData, 20> rx;


// тикер вызывается в прерывании
void ICACHE_RAM_ATTR isr() {
  rx.tickISR();
}
//RX 433


void setup() {

// взводим прерывания по CHANGE для радио точки
  attachInterrupt(RadioModuleData, isr, CHANGE);


  EEPROM.begin(300);
  EEPROM.get(0, Settings);
  
  Serial.begin(115200);


////<-74CH595
  pinMode(DATA_PIN, OUTPUT);    // инициализация пинов
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  digitalWrite(LATCH_PIN, HIGH);


  byte CurrentRegister=0;
  //Заполняем состояние регистра
  for (byte i = 0; i <= 7; i++) bitWrite(CurrentRegister, i, Settings.registerStates[i]);

  byte CurrentRegister2=0;
  //Заполняем состояние регистра
  for (byte i = 0; i <= 7; i++) bitWrite(CurrentRegister2, i, Settings.registerStates[i+8]);

  
  Serial.println("расчёт CurrentRegister:");
  printBinaryByte(CurrentRegister);
  Serial.println();

  Serial.println("расчёт CurrentRegister2:");
  printBinaryByte(CurrentRegister2);
  Serial.println();
  
  digitalWrite(LATCH_PIN, LOW);               // "открываем защелку"
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, CurrentRegister2); // отправляем данные
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, CurrentRegister); // отправляем данные
  digitalWrite(LATCH_PIN, HIGH);              // "закрываем защелку", выходные ножки регистра установлены

////74CH595->

 
 //<-Датчик температуры DS18B20
  sensors.begin(); // Иницилизируем датчики
  deviceCount = sensors.getDeviceCount(); // Получаем количество обнаруженных датчиков

  pinMode(A0, INPUT);

  for (uint8_t index = 0; index < deviceCount; index++)
  {
    sensors.getAddress(temperatureSensors[index], index);
  }
  //Датчик температуры DS18B20 ->
  
  
  //<- Инициализаия дисплэя
  Serial.println("OLED FeatherWing test");
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  Serial.println("OLED begun");

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  //Инициализаия дисплэя ->




  pinMode(SettingsPin,INPUT_PULLUP);

  if(digitalRead(SettingsPin) == LOW) {
    
    //Если кнопка нажата запускаем страницу настроек
    SettingMode = true; 
    
    //Получаем список доступных сетей заполняем WiFiOptionForSelect
    GetVisibleWiFiSSID();

    //delay(1000);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MyPortal");

    Serial.println("Включаем точку доступа");

    //WiFi.softAPIP();

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("AP mode"); 
    display.println(WiFi.softAPIP()); 
    display.display();

    portal.attachBuild(buildSettings);
    portal.attach(actionSettings);
    portal.start();

    }
  else ConnectWiFiClient();
}

void ConnectWiFiClient(){

    EEPROM.get(0, Settings);
      
    WiFi.mode(WIFI_STA);
    WiFi.hostname(Settings.mdns);
    WiFi.begin(Settings.ssid, Settings.pass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println(WiFi.localIP());
   
    portal.attachBuild(build);  
    portal.attach(action);
    portal.start();

    Serial.println(Settings.ssid);
    Serial.println(Settings.pass);
    Serial.println(Settings.mdns);
    
    Serial.println("считаем символы для:"+String(Settings.loginweb));
    
    int count = 0;
    for (char* p = Settings.loginweb; *p; p++) if(*p != '\0') count++;

    Serial.print("Кол-во символов login web:");
    Serial.println(count);
    
    if (count>0) portal.enableAuth(Settings.loginweb, Settings.password);

    int count2 = CalculateCountChar(Settings.loginweb);

    Serial.print("Кол-во символов login web подстчёт 2: ");
    Serial.println(count2);
    

}

int CalculateCountChar(char *CurrentChar){

  int count = 0;
  for (char* p = CurrentChar; *p; p++) if(*p != '\0') count++;

  return count;

}


void loop() {

 // gotData() вернёт количество удачно принятых байт
  if (rx.gotData()) {   // если больше 0    
    
    switch (rx.buffer[0]) { // Получаем адрес модуля
      case 0xA1:            // Кнопка
        Serial.println(rx.buffer[1] ? "Отпустили кнопку" : "Нажали кнопку");
        
        //if (rx.buffer[1] == 1) changeState(6,1);
        //else changeState(6,0);

        if (rx.buffer[1] == 1) changeState(6, not Settings.registerStates[6]);
        //else changeState(6,0);
        
        break;
/*
      case 0xA2:            // Термистор
        Serial.print("NTC: ");
        Serial.print(ntc.computeTemp(rx.buffer[1] << 8 | rx.buffer[2]));
        Serial.println(" *C");
        break;
      case 0xA3:            // Фоторезистор
        Serial.print("Photo value: ");
        Serial.println(rx.buffer[1] << 8 | rx.buffer[2]);
        break;
      case 0xA4:            // PIR детектор
        Serial.println("Pir detected: ");
        break;
      case 0xA5:            // DS18B20
        Serial.print("DS18B20: ");
        Serial.print(ds.calcRaw(rx.buffer[1] << 8 | rx.buffer[2]));
        Serial.println(" *C");
        break;
*/
    
    }//switch
  }//if (rx.gotData())

 
  portal.tick();


  if ((millis() - lastTime) > timerDelay) {
    
    sensors.requestTemperatures();
    

   lastTime = millis();
  }

  if ((millis() - lastTime) > timerDelay2000) {
    
    for (int i = 0; i < deviceCount; i++)
    {
      printAddress(temperatureSensors[i]); // Выводим название датчика
      Serial.print(": ");
      
      Serial.println(sensors.getTempC(temperatureSensors[i])); // Выводим температуру с датчика
    }
    
    lastTime = millis();
  }


  //Если не включен режим настроек можно шевелить сеть и дисплэй обновлять
  if (SettingMode == false)
  {
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Connected...");
    display.println(WiFi.localIP());
   
    display.println(sensors.getTempC(temperatureSensors[0]));
    display.display();


    if (Settings.MQTT_Enable == true) {
      client.loop();
      publishTemperature();
    }
  }


}

void onConnectionEstablished() {
  client.subscribe("base/relay/led1", [] (const String &payload)  {
    Serial.println(payload);
  });
}

long last = 0;
void publishTemperature() {
  long now = millis();
  if (client.isConnected() && (now - last > PUB_DELAY)) {
    client.publish("base/state/temperature", String(sensors.getTempC(temperatureSensors[0])));
    client.publish("base/state/humidity", String(random(40, 90)));
    last = now;

    Serial.println("Отправил данные");
  }
}


void buildSettings() {

  GP.BUILD_BEGIN();               // запустить конструктор

  GP.TITLE("Введите настройки для подключения к Wi-Fi");
  //GP.LABEL("Введите настройки для подключения к Wi-Fi");

  GP.LABEL("Доступные WiFi:");
  
  ///////////////////////////////////// 
  //JavaScrypt обеспечивает изменение поля SsidWiFi при выборе WiFi сети из списка Select
  //WiFiOptionForSelect глобальная переменная типа String Заполняется функцией GetVisibleWiFiSSID она должна быть вызвана до построения страницы
  /////////////////////////////////////
  
  GP.SEND(F("<form name='ScanForm'>"));
  GP.SEND(F("<select name='WiFi'>"));
  GP.SEND(WiFiOptionForSelect);
  GP.SEND(F("  </select> </form>"));

  GP.SEND(F("<script>"));
  GP.SEND(F("var WiFiSelect = ScanForm.WiFi;"));
  GP.SEND(F("function changeOption(){"));
  GP.SEND(F("var selection = document.getElementById('SsidWiFi');"));
  GP.SEND(F("var selectedOption = WiFiSelect.options[WiFiSelect.selectedIndex];"));
  GP.SEND(F("selection.value = selectedOption.text;}WiFiSelect.addEventListener('change', changeOption);"));
  GP.SEND(F("</script>"));


  GP.FORM_BEGIN("/SettingsESP");

  GP.BLOCK_BEGIN();
  
    GP.TEXT("SsidWiFi", "SSID", ""); 
    GP.BREAK();                  

    GP.TEXT("PassWiFi", "Pass", ""); 
    GP.BREAK();                  
  
    GP.TEXT("mDNSWiFi", "mDNS", ""); 
    GP.BREAK();                  

    GP.TEXT("loginweb", "loginweb", ""); 
    GP.BREAK();                  

    GP.TEXT("password", "password", ""); 
    GP.BREAK();                  

  GP.BLOCK_END();
  
  GP.SUBMIT("Сохранить");
  
  GP.FORM_END(); 
  
  GP.BREAK();

  //GP.SELECT("SelectSSID",SelectsItemWiFi,0);

  //GP.BUTTON("btnScan", "Scan");

  GP.BUILD_END();
 
}

void actionSettings(GyverPortal &p){
  if (portal.form()) {
    if (portal.form("/SettingsESP")) {

        portal.copyStr("SsidWiFi", Settings.ssid);
        portal.copyStr("PassWiFi", Settings.pass);
        portal.copyStr("mDNSWiFi", Settings.mdns);
        portal.copyStr("loginweb", Settings.loginweb);
        portal.copyStr("password", Settings.password);


        
        Serial.println("Сохраняем в память:");
        Serial.println(Settings.ssid);
        Serial.println(Settings.pass);
        Serial.println(Settings.mdns);
        Serial.println(Settings.loginweb);
        Serial.println(Settings.password);


        EEPROM.put(0, Settings);
        EEPROM.commit();

        WiFi.softAPdisconnect();
        
        SettingMode = false;
        
        ConnectWiFiClient();
    }//(portal.form("/SettingsESP"))

  
  }//portal.form()
 
}


void build() {
  GP.BUILD_BEGIN();               // запустить конструктор
  GP.THEME(GP_DARK);          // применить тему

  //GP.AJAX_UPDATE("pos0,pos1,pos2,pos3,pos4,pos5,pos6,pos7");

 
  GP.NAV_TABS_M("navA", "Home,Settings");

  GP.NAV_BLOCK_BEGIN("navA", 0);

  GP.LABEL("Led 0: "); 
  GP.SWITCH("pos0", Settings.registerStates[0]);
  GP.BREAK();

  GP.LABEL("Led 1: "); 
  GP.SWITCH("pos1", Settings.registerStates[1]);
  GP.BREAK();
  
  GP.LABEL("Led 2: "); 
  GP.SWITCH("pos2", Settings.registerStates[2]);
  GP.BREAK();
    
  GP.LABEL("Led 3: "); 
  GP.SWITCH("pos3", Settings.registerStates[3]);
  GP.BREAK();
  
  GP.LABEL("Led 4: "); 
  GP.SWITCH("pos4", Settings.registerStates[4]);
  GP.BREAK();
  
  GP.LABEL("Led 5: "); 
  GP.SWITCH("pos5", Settings.registerStates[5]);
  GP.BREAK();
  
  GP.LABEL("Led 6: "); 
  GP.SWITCH("pos6", Settings.registerStates[6]);
  GP.BREAK();
  
  GP.LABEL("Led 7: "); 
  GP.SWITCH("pos7", Settings.registerStates[7]);
  GP.BREAK();

  GP.LABEL("Led 8: "); 
  GP.SWITCH("pos8", Settings.registerStates[8]);
  GP.BREAK();
  
  GP.LABEL("Led 9: "); 
  GP.SWITCH("pos9", Settings.registerStates[9]);
  GP.BREAK();
  
  GP.LABEL("Led 10: "); 
  GP.SWITCH("pos10", Settings.registerStates[10]);
  GP.BREAK();
  
  GP.LABEL("Led 11: "); 
  GP.SWITCH("pos11", Settings.registerStates[11]);
  GP.BREAK();
  
  GP.LABEL("Led 12: "); 
  GP.SWITCH("pos12", Settings.registerStates[12]);
  GP.BREAK();
  
  GP.LABEL("Led 13: "); 
  GP.SWITCH("pos13", Settings.registerStates[13]);
  GP.BREAK();
  
  GP.LABEL("Led 14: "); 
  GP.SWITCH("pos14", Settings.registerStates[14]);
  GP.BREAK();
    
  GP.LABEL("Led 15: "); 
  GP.SWITCH("pos15", Settings.registerStates[15]);
  GP.BREAK();
  
  GP.NAV_BLOCK_END();

  GP.NAV_BLOCK_BEGIN("navA", 1);

  GP.FORM_BEGIN("/SettingsDevice");

  GP.BLOCK_BEGIN();
  GP.LABEL("MQTT"); 
  GP.SWITCH("MQTT_Enable", Settings.MQTT_Enable); 
  GP.BREAK();  
   
  GP.TEXT("MQTTServer", "MQTTServer", Settings.MQTTServer); 
  GP.BREAK();                  

  GP.TEXT("MQTTLogin", "MQTTLogin", Settings.MQTTLogin); 
  GP.BREAK();                  
  
  GP.TEXT("MQTTPwd", "MQTTPwd", Settings.MQTTPwd); 
  GP.BREAK();                  

  GP.TEXT("MQTTid", "MQTTideb", Settings.MQTTid); 
  GP.BREAK();                  
  
  GP.BLOCK_END();
  
  GP.SUBMIT("Сохранить");
  
  GP.FORM_END(); 
  
  GP.NAV_BLOCK_END();

  GP.BUILD_END();

}



void action(GyverPortal &p) {
  // опрос действий
  // здесь переменная p является ссылкой на portal (ниже)
  // можно обращаться к ней точно так же, if (p.click())

  if (portal.form()) {
    if (portal.form("/SettingsDevice")) {

        portal.copyStr("MQTT_Enable", Settings.MQTT_Enable);
        portal.copyStr("MQTTServer", Settings.MQTTServer);
        portal.copyStr("MQTTLogin", Settings.MQTTLogin);
        portal.copyStr("MQTTPwd", Settings.MQTTPwd);
        portal.copyStr("MQTTid", Settings.MQTTid);

        EEPROM.put(0, Settings);
        EEPROM.commit();
    }
  }


  if (portal.click("pos0")) changeState(0,portal.getCheck("pos0"));

  if (portal.click("pos1")) changeState(1,portal.getCheck("pos1"));

  if (portal.click("pos2")) changeState(2,portal.getCheck("pos2"));

  if (portal.click("pos3")) changeState(3,portal.getCheck("pos3"));

  if (portal.click("pos4")) changeState(4,portal.getCheck("pos4"));

  if (portal.click("pos5")) changeState(5,portal.getCheck("pos5"));

  if (portal.click("pos6")) changeState(6,portal.getCheck("pos6"));

  if (portal.click("pos7")) changeState(7,portal.getCheck("pos7"));

  if (portal.click("pos8")) changeState(8,portal.getCheck("pos8"));

  if (portal.click("pos9")) changeState(9,portal.getCheck("pos9"));

  if (portal.click("pos10")) changeState(10,portal.getCheck("pos10"));

  if (portal.click("pos11")) changeState(11,portal.getCheck("pos11"));

  if (portal.click("pos12")) changeState(12,portal.getCheck("pos12"));

  if (portal.click("pos13")) changeState(13,portal.getCheck("pos13"));

  if (portal.click("pos14")) changeState(14,portal.getCheck("pos14"));

  if (portal.click("pos15")) changeState(15,portal.getCheck("pos15"));
  
}


void changeState(byte bitPos, bool State){

  Settings.registerStates[bitPos]=State;

  EEPROM.put(0, Settings);
  EEPROM.commit();

  byte CurrentRegister=0;
  //Заполняем состояние регистра
  for (byte i = 0; i <= 7; i++) bitWrite(CurrentRegister, i, Settings.registerStates[i]);

  byte CurrentRegister2=0;
  //Заполняем состояние регистра
  for (byte i = 0; i <= 7; i++) bitWrite(CurrentRegister2, i, Settings.registerStates[i+8]);
  
  Serial.println("расчёт CurrentRegister:");
  printBinaryByte(CurrentRegister);
  Serial.println();

  Serial.println("расчёт CurrentRegister2:");
  printBinaryByte(CurrentRegister2);
  Serial.println();
  
  digitalWrite(LATCH_PIN, LOW);               // "открываем защелку"
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, CurrentRegister2); // отправляем данные
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, CurrentRegister); // отправляем данные
  digitalWrite(LATCH_PIN, HIGH);              // "закрываем защелку", выходные ножки регистра установлены

  
}

void printBinaryByte(byte value)  {
  //for (byte mask = 0x80; mask; mask >>= 1)  {
  for (byte mask = 0x80; mask; mask >>= 1)  {
    Serial.print((mask & value) ? '1' : '0');
  }
}

void GetVisibleWiFiSSID(){

    WiFi.mode(WIFI_AP);
    WiFi.softAP("Scan");

    //String StringSelectWiFi;
    
    int n = WiFi.scanNetworks();

    delay(1000);
    
    if (n>0) Serial.println("Количество найденых сетей "+String(n)+":");
    else Serial.println("Сети не найдены.");
  
    for (int i = 0; i < n; i++)
    {
      Serial.println(WiFi.SSID(i));
      //StringSelectWiFi+=WiFi.SSID(i);

      //Заполняем опции select доступными WiFi сетями
      WiFiOptionForSelect+="<option value='"+WiFi.SSID(i)+"'>"+WiFi.SSID(i)+"</option>";
      //if (i != n-1) {
      //  StringSelectWiFi+=",";
      //}
    }

//    Serial.println(StringSelectWiFi);

    Serial.println("опции:");
    Serial.println(WiFiOptionForSelect);

    //Эта комбинация в итоге создаёт чар массив SelectsItemWiFi и его можно использовать в GyverPortal
    //StringSelectWiFi.toCharArray(SelectsItemWiFi, StringSelectWiFi.length());    // копируется String в массив
    
    WiFi.softAPdisconnect();

}//GetVisibleWiFiSSID

// Функция вывода адреса датчика 
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX); // Выводим адрес датчика в HEX формате 
  }
}
