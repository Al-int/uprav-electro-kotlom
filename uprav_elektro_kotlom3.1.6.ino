/*
  Термостат для Электрокотла с бойлером и приемом данных от люка теплого пола
  с возможностью изменение термостатирования последнего. 
  Добавлен датчик темперауры на улице.
  Добавлена возможностью отключения части нагрузки

  >Encoder control
  >DS18B20 thermal sensor 
  >LCD2004 i2c
  >DS1307 RTC
*/

#include <avr/wdt.h>
#include <Wire.h> // i2c (для RTC и экрана)
#include <RealTimeClockDS1307.h> // RTC
#include <EEPROMex.h> // EE
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h> // прерывания по таймеру1

#include <OneWire.h> // 1wire для DS18B20
#include <DallasTemperature.h> // DS18B20

#define ONE_WIRE_BUS A1
#define ONE_WIRE_BUS_U A2
OneWire oneWire(ONE_WIRE_BUS);
OneWire oneWire_U(ONE_WIRE_BUS_U);
DallasTemperature DS18B20(&oneWire);
DallasTemperature sensors(&oneWire_U);



// ***************************  DeviceAddress DS18B20Address;  ???????????????

// Прописываем все датчики, адрес в обратном порядке
DeviceAddress ThermometerK = {
  0x28, 0xFF, 0x6D, 0x97, 0x83, 0x16, 0x03, 0xF0
};  //датчик в котле
DeviceAddress ThermometerB = {
  0x28, 0xFF, 0x2C, 0xCD, 0x81, 0x16, 0x03, 0x58
}; //датчик в бойлере

DeviceAddress ThermRadiatorDaln = {
  0x28, 0xFF, 0x4F, 0xD1, 0x81, 0x16, 0x03, 0x2D
}; //датчик радиатора  (два тена )
DeviceAddress ThermRadiatorBlign = {
  0x28, 0xFF, 0x9C, 0x03, 0x85, 0x16, 0x05, 0x78
}; //датчик радиатора (тен и бойлер)

DeviceAddress ThermometerU = {
  0x28, 0x19, 0x40, 0x4B, 0x03, 0x00, 0x00, 0xAA
};  //датчик на улице

#define encoderA    2 // энкодер - поворот вправо (об землю)
#define encoderB    3 // энкодер - поворот влево (об землю)
#define encoderK    4 // энкодер - кнопка (об землю)
#define PowerONpin 5 // нога, подключения выключателя нагрева 
#define BeepPin     6 // пищалка
#define BeepToneNo  2000 // тон звука "No", герц
#define BeepToneYes 4000 // тон звука "Yes", герц
#define BeepToneNoDuration 200 // длительность звука "No", мс
#define BeepToneYesDuration 200 // длительность звука "Yes", мс
#define Relay  7 // нога, к которой подключено реле
#define Relay2  8 // нога, к которой подключено реле
#define Relay3  9 // нога, к которой подключено реле
#define RelayB  10 // нога, к которой подключено реле ,бойлера
#define RelayOn HIGH // полярность сигнала включения реде (HIGH/LOW)
#define BlockONpin 11 // нога, подключения выключателя нагрева 


LiquidCrystal_I2C lcd(0x27, 20, 4); // ПРОВЕРИТЬ the LCD address to 0x20 for a 16 chars and 2 line display

byte block1[8] = {
  0x06, 0x09, 0x09, 0x06, 0x00, 0x04, 0x0E, 0x1F
}; // значок градуса с пламенем снизу
byte block2[8] = {
  0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00
}; // значок градуса

#define serialenabled // раскомментировать для выдачи в порт отладочной инфы

#define TstatTimerMax 3 //минимальная пауза между включениями горелки, сек
unsigned int TstatTimer = 10; //таймер паузы между включениями/выключениями, начальная установка 20 сек для устаканивания системы после сброса

float DS18B20TemperatureK = 0; //сырая температура от датчика котла
float TemperatureK = 0; //вычисленная температура котла с коррекцией
float DS18B20TempTmpK; //времянка
float DS18B20TemperatureB = 0; //сырая температура от датчика бойлера
float TemperatureB = 0; //вычисленная температура бойлера с коррекцией
float DS18B20TempTmpB; //времянка
byte DS18B20iteration = 0; //счётчик измерений температуры для усреднения

float TemperatureRadiatorDaln = 0; //температура крацйнего датчика (радиатор 1 и 2 тена)
float TemperatureRadiatorBlign = 0; //температура второго датчика (радиатор 3 тена и бойлера)
float TemperatureU = 0; //температура на улице


float TstatTemp = 50; //температура термостатирования, может изменяться настройками
float Hysteresis = 2; // гистерезис термостата, может изменяться настройками
float HysteresisOld;
float TbojlerTemp = 50; //температура болера, может изменяться настройками

boolean BojlerEnabled = true; // признак разрешения работі бойлера
boolean Ten1Enabled = true; // признак разрешения работы тена 1
boolean Ten2Enabled = true; // признак разрешения работы тена 2
boolean Ten3Enabled = true; // признак разрешения работы тена 3
boolean TenEnabledOld; // временная переменная используется при настройке


int Hours = 0; // времянка часов RTC для отображения и установки
int Minutes = 0; // времянка минут RTC для отображения и установки
int Seconds;
int StartTime1=600; //переменная задержки включения тена 1, в теле программы жестко задано значение
int StartTime2=300;//переменная задержки включения тена 2, в теле программы жестко задано значение
boolean PrintYesNo = false; // показывать ли после времени Yes/No (косвенно - указание на режим установка/отображение)
boolean SetH = false; // выделение часов при отображении
boolean SetM = false; // выделение минут при отображении
boolean SetYesNo = false; // выделение Yes/No при установке часов

boolean blink500ms = false; // мигающий бит, инвертируется каждые 500мс
boolean plus1sec = false; // ежесекундно взводится
boolean Peregrev = false; // проверяем на перегрев
boolean PowerON = false; // включить/выключить нагревательные приборы 
boolean BlockON = false; // включить/выключить ЧАСТЬ нагревательных приборов   // з а к л а д к а   в ы к л ю ч а т е л я  ч а с т и   н а г р у з к и


boolean LCDON = false; // включить/выключить отображение данных нагревательных приборов

boolean BeepEnabled = true;

byte MenuTimeoutTimer;

float AlarmTemp = 10; // температура для замерзательного орала

// encoder vars
static boolean rotating = false;    // debounce management
boolean A_set = false;
boolean B_set = false;
boolean encoderR = false;
boolean encoderL = false;

// EEPROM
EEMEM float TstatTempEE; //EE температура термостатирования
EEMEM boolean BeepEnabledEE; // EE признак разрешения звука
EEMEM float HysteresisEE; // EE гистерезис
EEMEM float AlarmTempEE; // EE значение недопустимого снижения температуры
EEMEM float TbojlerTempEE; //EE температура термостатирования бойера
EEMEM boolean BojlerEnabledEE; // EE признак разрешения работі бойлера
EEMEM boolean Ten1EnabledEE; // EE признак разрешения работы тена 1
EEMEM boolean Ten2EnabledEE; // EE признак разрешения работы тена 2
EEMEM boolean Ten3EnabledEE; // EE признак разрешения работы тена 3

// для передачи данных
String sp_startMarker;           // Переменная, содержащая маркер начала пакета
String sp_stopMarker;            // Переменная, содержащая маркер конца пакета
String sp_dataString;            // Здесь будут храниться принимаемые данные
int sp_startMarkerStatus;        // Флаг состояния маркера начала пакета
int sp_stopMarkerStatus;         // Флаг состояния маркера конца пакета
int sp_dataLength;               // Флаг состояния принимаемых данных
boolean sp_packetAvailable;      // Флаг завершения приема пакета

// Объявляем переменные строки для передачи данных
String stringData, stringSeparator;
int unit = 1;          //метка для приемника

// Объявляем переменные строки для приема данных
float unitRead;
float TemperatureL = 0;
float TemperatureP = 0;
float TstatLUK = 0;






// ===== SETUP ========================================================================
void setup() {
  wdt_disable(); // отключение сторожевого таймера

  Serial.begin(9600);
  sp_SetUp();                                       // Инициализируем протокол.

  stringData = String();
  stringSeparator = String(";");

  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, !RelayOn);
  pinMode(Relay2, OUTPUT);
  digitalWrite(Relay2, !RelayOn);
  pinMode(Relay3, OUTPUT);
  digitalWrite(Relay3, !RelayOn);
  pinMode(RelayB, OUTPUT);
  digitalWrite(RelayB, !RelayOn);

  lcd.init();                      // initialize the lcd
  lcd.backlight();
  lcd.createChar(1, block1);
  lcd.createChar(2, block2);
  pinMode(PowerONpin, INPUT);
  pinMode(BlockONpin, INPUT);
  digitalWrite(BlockONpin, HIGH);
  pinMode(encoderA, INPUT);
  digitalWrite(encoderA, HIGH);
  pinMode(encoderB, INPUT);
  digitalWrite(encoderB, HIGH);
  pinMode(encoderK, INPUT);
  digitalWrite(encoderK, HIGH);
  attachInterrupt(0, doEncoderA, CHANGE);   // encoder pin on interrupt 0 (pin 2)
  attachInterrupt(1, doEncoderB, CHANGE);  // encoder pin on interrupt 1 (pin 3)
  Timer1.initialize(500000); // Timer0 interrupt - set a timer of length 500000 microseconds
  Timer1.attachInterrupt( timerIsr ); // attach the service routine here
  EEPROM.setMaxAllowedWrites(32767);
  if ((digitalRead(encoderK)) == 0)
  { // если первая запись однокристалки (зажата кнопка при включении питания)- записать начальные значения в EE
    lcd.setCursor(0, 0); //инфо на LCD
    lcd.print(F("Cold start..."));

    EEPROM.updateFloat(int(&TstatTempEE), TstatTemp);
    EEPROM.updateByte(int(&BeepEnabledEE), BeepEnabled);
    EEPROM.updateFloat(int(&HysteresisEE), Hysteresis);
    EEPROM.updateFloat(int(&AlarmTempEE), AlarmTemp);
    EEPROM.updateFloat(int(&TbojlerTempEE), TbojlerTemp);
    EEPROM.updateByte(int(&BojlerEnabledEE), BojlerEnabled);
    EEPROM.updateByte(int(&Ten1EnabledEE), Ten1Enabled);
    EEPROM.updateByte(int(&Ten2EnabledEE), Ten2Enabled);
    EEPROM.updateByte(int(&Ten3EnabledEE), Ten3Enabled);

    tone(BeepPin, 2000, 50);
    delay(50);
    tone(BeepPin, 3000, 50);
    delay(50);
    tone(BeepPin, 4000, 50);
    delay(1000);
  }
  lcd.clear();
  lcd.setCursor(0, 0); //инфо на LCD
  lcd.print(F("Read settings..."));
  TstatTemp = EEPROM.readFloat(int(&TstatTempEE));
  BeepEnabled = EEPROM.readByte(int(&BeepEnabledEE));
  Hysteresis = EEPROM.readFloat(int(&HysteresisEE));
  AlarmTemp = EEPROM.readFloat(int(&AlarmTempEE));
  TbojlerTemp = EEPROM.readFloat(int(&TbojlerTempEE));
  BojlerEnabled = EEPROM.readByte(int(&BojlerEnabledEE));
  Ten1Enabled = EEPROM.readByte(int(&Ten1EnabledEE));
  Ten2Enabled = EEPROM.readByte(int(&Ten2EnabledEE));
  Ten3Enabled = EEPROM.readByte(int(&Ten3EnabledEE));


  DS18B20.begin();                            //для первой линии датчиков
  DS18B20.setResolution(ThermometerK, 12);
  DS18B20.setResolution(ThermometerB, 12);
  DS18B20.setResolution(ThermRadiatorDaln, 9);
  DS18B20.setResolution(ThermRadiatorBlign, 9);
  DS18B20.setWaitForConversion(false);
  DS18B20.requestTemperatures();

  sensors.begin();                          //для второй линии датчиков
  sensors.setResolution(ThermometerU, 12);
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  tone(BeepPin, 4000, 50);
  delay(100);
  tone(BeepPin, 4000, 50);
  delay(1000);
  lcd.clear();
  RTC.start();
  wdt_enable (WDTO_8S); // включение сторожевого таймера

}

// ===== MAIN CYCLE ===================================================================
void loop() {
//вывод  нформации о нагреве на экран ЕСЛИ разрешен
  if ( LCDON == true ) {
    lcd.setCursor(9, 3); //инфо на LCD
    if ((TemperatureK < AlarmTemp) & (blink500ms)) {
      lcd.print(F("*"));
    }
    else {
      lcd.print(F(" "));
    }
    lcd.print(F("t="));
    if (TemperatureK < 10) {
      lcd.print(F(" "));
    }
    lcd.print(TemperatureK, 1);
    if ( digitalRead(Relay) == RelayOn ) {
      lcd.write(0x01); // значок градуса с пламенем
    }
    else {
      lcd.write(0x02); // значок градуса
    }
    //////////////////////////////////////////////// отрабатываем ещё два тена на экране
    if ( digitalRead(Relay2) == RelayOn ) {
      lcd.write(0x01); // значок градуса с пламенем
    }
    else {
      lcd.write(0x02); // значок градуса
    }

    if ( digitalRead(Relay3) == RelayOn ) {
      lcd.write(0x01); // значок градуса с пламенем
    }
    else {
      lcd.write(0x02); // значок градуса
    }
    //////////////////////////////////////////////////// информация о бойлере
    lcd.setCursor(0, 3); //инфо на LCD
    lcd.print(F("b="));
    lcd.print(TemperatureB, 1);
    if ( digitalRead(RelayB) == RelayOn ) {
      lcd.write(0x01); // значок градуса с пламенем
    }
    else {
      lcd.write(0x02); // значок градуса
    }
  }

// выводим на экран температуру улицы
  lcd.setCursor(0, 2); //инфо на LCD
  lcd.print(F("ULICA="));
  lcd.print(TemperatureU, 1);
  lcd.write(0x02); // значок градуса

// выводим на экран температуру подвала
  lcd.setCursor(0, 1); //инфо на LCD
  lcd.print(F("PODVAL="));
  lcd.print(TemperatureP, 1);
  lcd.write(0x02); // значок градуса

// печатаем текущее время
  PrintYesNo = false;
  PrintRTC(0, 0);

// основная программа термостатирования НАЧАЛО!!
  if ( TstatTimer == 0 )
    {
  if ( Peregrev == false && PowerON == true ) // если нет перегрев и разрешен нагрев
  {
// вывод на экран информации о включеных приборах
          lcd.setCursor(9, 0);
          lcd.print (PowerON);
          lcd.print (BlockON);
          lcd.print (Ten1Enabled);
          lcd.print (Ten2Enabled);
          lcd.print (Ten3Enabled);
          lcd.print (BojlerEnabled);
//разрешаем работу бойлера и третего тена при не высокой нагрузки на сеть
    if (BlockON == true)
    {
//следим за состоянием бойлера    
  bool dataReaded=TemperatureB > TbojlerTemp && digitalRead(RelayB) == RelayOn;
      if ( dataReaded == true || BojlerEnabled == false )
      {
          digitalWrite(RelayB, !RelayOn); // выключить бойлер
      }
      if ( TemperatureB < TbojlerTemp - (Hysteresis * 2) && digitalRead(RelayB) == !RelayOn && BojlerEnabled == true)
      {
          digitalWrite(Relay3, !RelayOn); // выключить третий тэн
          digitalWrite(RelayB, RelayOn); // включить бойлер
      }
// термостатирование тэн 3 если бойлер нагрет

      if ( (digitalRead(RelayB) == RelayOn /* если бойлер включен */ && TemperatureK > TstatTemp && digitalRead(Relay3) == RelayOn) || Ten3Enabled == false ) // гистерезис
      {
          digitalWrite(Relay3, !RelayOn); // выключить тен 3
      }
      if (digitalRead(RelayB) == !RelayOn /* если бойлер выключен */ && TemperatureK < ( TstatTemp - Hysteresis ) && digitalRead(Relay3) == !RelayOn && Ten3Enabled == true )
      {
          digitalWrite(Relay3, RelayOn); // включить тен 3
      }
     }
// вырубаем бойлер и третий тен при высокой нагрузке на сеть    
     else { 
          digitalWrite(RelayB, !RelayOn);
          digitalWrite(Relay3, !RelayOn);
     }
// первые два тена работают не зависимо от нагрузки на сеть

//задаем задержку на включение первых двух тенов
// термостатирование тэн 2
     if (StartTime2 == 0)
     {
      if (TemperatureK < TstatTemp - (Hysteresis / 2 ) && digitalRead(Relay2) == !RelayOn && Ten2Enabled == true)
      {
          digitalWrite(Relay2, RelayOn); // включить горелку 2
      }
     }
      if (( TemperatureK > ( TstatTemp + Hysteresis ) && digitalRead(Relay2) == RelayOn) || Ten2Enabled == false ) // гистерезис
      {
          digitalWrite(Relay2, !RelayOn); // выключить горелку 2
      }
// термостатирование тэн 1
     if (StartTime1 == 0)
     {
      if (TemperatureK < TstatTemp && digitalRead(Relay) == !RelayOn && Ten1Enabled == true)
      {
          digitalWrite(Relay, RelayOn); // включить горелку 1
      }
     }
      if ( (TemperatureK > ( TstatTemp + ( 2 * Hysteresis ) ) && digitalRead(Relay) == RelayOn) || Ten1Enabled == false) // гистерезис
      {
          digitalWrite(Relay, !RelayOn); // выключить горелку 1
      }  
    }
  else {
    digitalWrite(Relay, !RelayOn); // выключить тєн1
    digitalWrite(Relay2, !RelayOn); // выключить тєн2
    digitalWrite(Relay3, !RelayOn); // выключить тєн3
    digitalWrite(RelayB, !RelayOn); // выключить бойлер
    lcd.setCursor(9, 0);
    lcd.print (PowerON);
}
      TstatTimer = TstatTimerMax;
        }
//конец основной программы термостатирования

  /* if ( Seconds == 0 )
    {

    }
  */
  // если прошла 1 секунда - делаем ежесекундные дела
  if (plus1sec) {
    plus1sec = false; // сбрасываем до следующей секунды
    // обновляем часы
    RTC.readClock();
    Hours = RTC.getHours();
    Minutes = RTC.getMinutes();
    Seconds = RTC.getSeconds();
//таймер задержки включения первых двух тенов
    if (StartTime1 != 0)
    {
      StartTime1 = StartTime1-1;
    }
    if (StartTime2 != 0)
    {
      StartTime2 = StartTime2-1;
    }

    // проверка входа разрешения на нагрев
    PowerON = digitalRead(PowerONpin);    
    BlockON = digitalRead(BlockONpin);    
//перезапуск таймера вклчения первых двух тенов после выключения нагрева с панели управления
  if (PowerON == false)
  {
    StartTime1 = 600;
    StartTime2 = 300;
  }
  

    // измеряем температуру теплоносителя в котле
    DS18B20TempTmpK = DS18B20.getTempC(ThermometerK); // получить температуру от датчика
    DS18B20TempTmpB = DS18B20.getTempC(ThermometerB); // получить температуру от датчика
    DS18B20.requestTemperatures();  // запустить новое измерение
    if (DS18B20TempTmpK != -127 && DS18B20TempTmpB != -127)
    {
      DS18B20TemperatureK += DS18B20TempTmpK; // суммируем для усреднения
      DS18B20TemperatureB += DS18B20TempTmpB; // суммируем для усреднения
      DS18B20iteration ++;
      if (DS18B20iteration == 10)
      {
        DS18B20iteration = 0;
        TemperatureK = (DS18B20TemperatureK / 10); //усреднённая 
        TemperatureB = (DS18B20TemperatureB / 10); //усреднённая 
        DS18B20TemperatureK = 0;
        DS18B20TemperatureB = 0;
      }
    }
    TemperatureU = sensors.getTempC(ThermometerU);
    sensors.requestTemperatures();  // запустить новое измерение


    //получение и индикация температуры датчиков радиатора
    TemperatureRadiatorDaln = DS18B20.getTempC(ThermRadiatorDaln);
    TemperatureRadiatorBlign = DS18B20.getTempC(ThermRadiatorBlign);

    /*  использовалось при отладке
      lcd.setCursor(0, 3); //инфо на LCD
      lcd.print(F("t1-2="));
      lcd.print(TemperatureRadiatorDaln,1);
      lcd.setCursor(10, 3); //инфо на LCD
      lcd.print(F("t3-b="));
      lcd.print(TemperatureRadiatorBlign,1);
    */
    //проверяем на перегрев
    if (TemperatureRadiatorDaln > 60 || TemperatureRadiatorBlign > 60 )
    {
      Peregrev = true;
      lcd.setCursor(19, 0);
      lcd.print ("e");
    }
    else
    {
      Peregrev = false;
      lcd.setCursor(19, 0);
      lcd.print (" ");
    }
    //конец обработки температуры датчиков радиатора



    sp_Read();
    if (sp_packetAvailable == true)
    {
      String data_0 = getValue(sp_dataString, ';', 0 );  //присваиваем переменной первый набор символов разделённые ";" т.е 12.34
      String data_1 = getValue(sp_dataString, ';', 1 );  //присваиваем переменной второй набор символов разделённые ";" т.е 12.34
      String data_2 = getValue(sp_dataString, ';', 2 );  //присваиваем переменной второй набор символов разделённые ";" т.е 12.34
      String data_3 = getValue(sp_dataString, ';', 3 );  //присваиваем переменной второй набор символов разделённые ";" т.е 12.34

      //преобразовываем в числа
      unitRead = data_0.toFloat();
      TemperatureL = data_1.toFloat();
      TemperatureP = data_2.toFloat();
      TstatLUK = data_3.toFloat();
      sp_packetAvailable = false;
    }


    if (TemperatureK < AlarmTemp) {
      tone(BeepPin, 4000, 5);
    }
  }


  // ================ по нажатию кнопки энкодера - меню настроек ====================
  if (digitalRead(encoderK) == 0) {
    MenuTimeoutTimer = 10; //таймер таймаута, секунд
    lcd.clear();
    lcd.setCursor(0, 0); //инфо на LCD
    lcd.print(F("< SETUP >"));
    if (BeepEnabled) {
      tone(BeepPin, 4000, 50);
    }
    delay(200);
    int menuitem = 0;

    do {
      rotating = true;  // reset the debouncer
      if ((encoderR) | (encoderL)) {
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        if (encoderR) {
          menuitem += 1;
        }
        else  {
          menuitem -= 1;
        }
        if ( menuitem > 10 ) {
          menuitem = 0;
        } // границы пунктов меню
        if ( menuitem < 0 ) {
          menuitem = 10;
        }
        encoderR = false;
        encoderL = false;
      }

      // индикация пункта меню (номер пункта - в menuitem)
      lcd.setCursor(0, 1); //инфо на LCD
      switch (menuitem)
      {
        case 0:
          lcd.print(F("0.BACK             "));
          break;
        case 1:
          lcd.print(F("1.TEN SET  "));
          break;
        case 2:
          lcd.print(F("2.LCD         "));
          break;
        case 3:
          lcd.print(F("3.LUK V PADVAL      "));
          // выводим на экран температуру люка
          lcd.print(TemperatureL, 1);
          break;
        case 4:
          lcd.print(F("4.TEPLONOSYTEL SET"));
          break;
        case 5:
          lcd.print(F("5.CLOCK SET       "));
          break;
        case 6:
          lcd.print(F("6.HYSTERESIS SET"));
          break;
        case 7:
          lcd.print(F("7.off           "));
          break;
        case 8:
          lcd.print(F("8.SOUND SET  "));
          break;
        case 9:
          lcd.print(F("9.T-ALARM SET  "));
          lcd.setCursor(0, 2);
          lcd.print(F("t1-2="));
          lcd.print(TemperatureRadiatorDaln, 1);
          lcd.print(F(" "));
          lcd.print(F("t3-b="));
          lcd.print(TemperatureRadiatorBlign, 1);
          break;
        case 10:
          lcd.print(F("10.T-BOJLER SET"));
          break;
      }
      if (MenuTimeoutTimer == 0) {
        menuitem = 0;
      }

    }
    while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
    // если нажата кнопка энкодера или таймаут - обработка пункта меню (номер пункта - в menuitem)
    if (BeepEnabled) {
      tone(BeepPin, 4000, 50);
    }
    switch (menuitem)
    {
      // ====== пункт 0 - выход
      case 0:
        if (BeepEnabled) {
          tone(BeepPin, BeepToneNo, BeepToneNoDuration);
        } //звук "NO"
        break; // case 0 out

// ====== пункт 1 - установка работы тэнов
      case 1:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SETUP Ten"));
        lcd.setCursor(0, 1); //инфо на LCD
        lcd.print(F("123B"));
    
//разрешение на работу первого тена        
        delay(200);
        do {
          lcd.setCursor(0, 2);          
          TenEnabledOld = Ten1Enabled;        
          if (Ten1Enabled ) {
            lcd.print(F("1"));
          }
          else {
            lcd.print(F("0"));
          }
          rotating = true;  // reset the debouncer
          if ((encoderR) | (encoderL)) {
            Ten1Enabled = !Ten1Enabled;
            encoderR = false;
            encoderL = false;
          }
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
          if (TenEnabledOld !=Ten1Enabled)
          {
          EEPROM.updateByte(int(&Ten1EnabledEE), Ten1Enabled);
          }
        }
        else Ten1Enabled=TenEnabledOld;

//разрешение на работу второго тена
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        delay(200);
          do {
          lcd.setCursor(1, 2);
          TenEnabledOld = Ten2Enabled;        
          if (Ten2Enabled ) {
            lcd.print(F("1"));
          }
          else {
            lcd.print(F("0"));
          }
          rotating = true;  // reset the debouncer
          if ((encoderR) | (encoderL)) {
            Ten2Enabled = !Ten2Enabled;
            encoderR = false;
            encoderL = false;
          }
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
          if (TenEnabledOld != Ten2Enabled)
          {
          EEPROM.updateByte(int(&Ten2EnabledEE), Ten2Enabled);
          }
        }
        else Ten2Enabled=TenEnabledOld;

//разрешение на работу третего тена
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        delay(200);

          do {
          lcd.setCursor(2, 2);
          TenEnabledOld = Ten3Enabled;        
          if (Ten3Enabled ) {
            lcd.print(F("1"));
          }
          else {
            lcd.print(F("0"));
          }
          rotating = true;  // reset the debouncer
          if ((encoderR) | (encoderL)) {
            Ten3Enabled = !Ten3Enabled;
            encoderR = false;
            encoderL = false;
          }
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
          if (TenEnabledOld != Ten3Enabled)
          {
          EEPROM.updateByte(int(&Ten3EnabledEE), Ten3Enabled);
          }
        }
        else Ten3Enabled=TenEnabledOld;

//разрешение на работу бойлера
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        delay(200);

          do {
          lcd.setCursor(3, 2);
          TenEnabledOld = BojlerEnabled;        
          if (BojlerEnabled ) {
            lcd.print(F("1"));
          }
          else {
            lcd.print(F("0"));
          }
          rotating = true;  // reset the debouncer
          if ((encoderR) | (encoderL)) {
            BojlerEnabled = !BojlerEnabled;
            encoderR = false;
            encoderL = false;
          }
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
          if (TenEnabledOld != BojlerEnabled)
          {
          EEPROM.updateByte(int(&BojlerEnabledEE), BojlerEnabled);
          }
        }
        else BojlerEnabled=TenEnabledOld;        
    
        break; // case 1 out

// ====== пункт 2 - установка Timer2
      case 2:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("LCD SET       "));
        delay(200);
        do {
          lcd.setCursor(0, 1);
          if (LCDON) {
            lcd.print(F("LCD ON         "));
          }
          else {
            lcd.print(F("LCD OFF        "));
          }

          rotating = true;  // reset the debouncer
          if ((encoderR) | (encoderL)) {
            LCDON = !LCDON;
            encoderR = false;
            encoderL = false;
          }
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
        }
        if (MenuTimeoutTimer == 0) {
        }
        break; // case 2 out

// ====== пункт 3 - установка тимпературы теплого люка
      case 3:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SETUP T-teploLUK"));
        delay(200);
        float TstatLUKtmp;
        TstatLUKtmp = TstatLUK;
        do {
          lcd.setCursor(0, 1);
          if (blink500ms) {
            lcd.print(F("    "));
          }
          else {

            lcd.print(TstatLUK, 1);
            lcd.write(0x02); // значок градуса
          }
          rotating = true;  // reset the debouncer
          if (encoderR) {
            TstatLUK += 0.5;
            encoderR = false;
          }
          if (encoderL)
          {
            TstatLUK -= 0.5;
            encoderL = false;
          }
          TstatLUK = constrain(TstatLUK, 15, 40);
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
        if (MenuTimeoutTimer != 0) {
          //отправка новой температуры для люка
          if ( TstatLUKtmp != TstatLUK)
          {
          stringData = unit + stringSeparator +  TstatLUK;
          sp_Send(stringData);  //отсылаем
          }
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
        }
        else {
          TstatLUK = TstatLUKtmp;
          if (BeepEnabled) {
            tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
          }
        }
        break; // case 3 out

// ====== пункт 4 - установка тимпературы теплоносителя
      case 4:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SETUP T-otoplenya"));
        delay(200);
        do {
          lcd.setCursor(0, 1);
          if (blink500ms) {
            lcd.print(F("    "));
          }
          else {

            lcd.print(TstatTemp, 1);
            lcd.write(0x02); // значок градуса
          }
          rotating = true;  // reset the debouncer
          if (encoderR) {
            TstatTemp += 0.5;
            encoderR = false;
          }
          if (encoderL)
          {
            TstatTemp -= 0.5;
            encoderL = false;
          }
          TstatTemp = constrain(TstatTemp, 5, 85);
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
        if (MenuTimeoutTimer != 0) {
          EEPROM.updateFloat(int(&TstatTempEE), TstatTemp); // запись в ЕЕПРОМ
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
        }
        else {
          TstatTemp = EEPROM.readFloat(int(&TstatTempEE));
          if (BeepEnabled) {
            tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
          }
        }
        break; // case 4 out

// ====== пункт 5 - установка RTC
      case 5:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SETUP CLOCK"));
        delay(200);
        RTC.readClock();
        Hours = RTC.getHours();
        Minutes = RTC.getMinutes();
        SetYesNo = false;
        PrintYesNo = true;
        SetTime(0, 1); // в позиции 0,1 - запрос ввода времени
        if (MenuTimeoutTimer != 0) {
          if (SetYesNo)
          {
            if (BeepEnabled) {
              tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
            }
            RTC.setHours(Hours);
            RTC.setMinutes(Minutes);
            RTC.setSeconds(0);
            RTC.setClock();
            RTC.start();
          }
          else
          {
            if (BeepEnabled) {
              tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
            }
          }
        }
        else {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
          }
        }
        break; // case 5 out

// ====== пункт 6 - установка гистерезиса
      case 6:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        HysteresisOld = Hysteresis;
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SETUP HYSTERESIS"));
        delay(200);
        do {
          lcd.setCursor(0, 1);
          if (blink500ms) {
            lcd.print("   ");
          }
          else {
            lcd.print(Hysteresis, 1);
            lcd.write(0x02); // значок градуса
          }
          rotating = true;  // reset the debouncer
          if (encoderR) {
            Hysteresis += 0.5;
            encoderR = false;
          }
          if (encoderL) {
            Hysteresis -= 0.5;
            encoderL = false;
          }
          Hysteresis = constrain(Hysteresis, 1, 10); // крайние значения
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
        if (MenuTimeoutTimer != 0) {
          EEPROM.updateFloat(int(&HysteresisEE), Hysteresis); // запись в ЕЕПРОМ
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
        }
        else {
          Hysteresis = HysteresisOld;
          if (BeepEnabled) {
            tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
          }
        }
        break; // case 6 out

// ====== пункт 7 - не используется
      case 7:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("OFF"));
        delay(500);

        break; // case 7 out

// ====== пункт 8 - вкл/выкл звука
      case 8:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SOUND SET"));
        delay(200);
        do {
          lcd.setCursor(0, 1);
          if (BeepEnabled) {
            lcd.print(F("BEEP ON "));
          }
          else {
            lcd.print(F("BEEP OFF"));
          }
          rotating = true;  // reset the debouncer
          if ((encoderR) | (encoderL)) {
            BeepEnabled = !BeepEnabled;
            encoderR = false;
            encoderL = false;
          }
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
          EEPROM.updateByte(int(&BeepEnabledEE), BeepEnabled);
        }
        if (MenuTimeoutTimer == 0) {
          BeepEnabled = EEPROM.readByte(int(&BeepEnabledEE));
        }
        break; // case 8 out

// ====== пункт 9 - установка предупреждалки о холоде
      case 9:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("ALARM-TEMP SET"));
        delay(200);
        do {
          lcd.setCursor(0, 1);
          if (blink500ms) {
            lcd.print(F("    "));
          }
          else {
            if (AlarmTemp >= 0) {
              lcd.print(F("+"));
            }

            lcd.print(AlarmTemp, 0);
            lcd.write(0x02); // значок градуса
          }
          rotating = true;  // reset the debouncer
          if (encoderR) {
            AlarmTemp += 1;
            encoderR = false;
          }
          if (encoderL) {
            AlarmTemp -= 1;
            encoderL = false;
          }
          AlarmTemp = constrain(AlarmTemp, 15, 90); // крайние значения
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));

        if (MenuTimeoutTimer != 0) {
          EEPROM.updateFloat(int(&AlarmTempEE), AlarmTemp); // запись в ЕЕПРОМ
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
        }
        else {
          AlarmTemp = EEPROM.readFloat(int(&AlarmTempEE));
          if (BeepEnabled) {
            tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
          }
        }
        break; // case 9 out

// ====== пункт 10 - установка тимпературы бойлера
      case 10:
        MenuTimeoutTimer = 10; //таймер таймаута, секунд
        lcd.clear();
        lcd.setCursor(0, 0); //инфо на LCD
        lcd.print(F("SETUP T-bojlera"));
        delay(200);
        do {
          lcd.setCursor(0, 1);
          if (blink500ms) {
            lcd.print(F("    "));
          }
          else {

            lcd.print(TbojlerTemp, 1);
            lcd.write(0x02); // значок градуса
          }
          rotating = true;  // reset the debouncer
          if (encoderR) {
            TbojlerTemp += 0.5;
            encoderR = false;
          }
          if (encoderL)
          {
            TbojlerTemp -= 0.5;
            encoderL = false;
          }
          TbojlerTemp = constrain(TbojlerTemp, 10, 85);
        }
        while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
        if (MenuTimeoutTimer != 0) {
          EEPROM.updateFloat(int(&TbojlerTempEE), TbojlerTemp); // запись в ЕЕПРОМ
          if (BeepEnabled) {
            tone(BeepPin, BeepToneYes, BeepToneYesDuration); //звук "YES"
          }
        }
        else {
          TbojlerTemp = EEPROM.readFloat(int(&TbojlerTempEE));
          if (BeepEnabled) {
            tone(BeepPin, BeepToneNo, BeepToneNoDuration); //звук "NO"
          }
        }
        break; // case 10 out


    }
    delay(200);
    lcd.clear();
  }
  wdt_reset();    // сброс отсчета сторожевого таймера

}



// Первичная инициализация протокола:
void sp_SetUp()
{
  sp_startMarker = "<bspm>";     // Так будет выглядеть маркер начала пакета
  sp_stopMarker = "<espm>";      // Так будет выглядеть маркер конца пакета
  sp_dataString.reserve(200);     // Резервируем место под прием строки данных
  sp_ResetAll();                 // Полный сброс протокола
}

// Полный сброс протокола:
void sp_ResetAll()
{
  sp_dataString = "";           // Обнуляем буфер приема данных
  sp_Reset();                   // Частичный сброс протокола
}


// Частичный сброс протокола:
void sp_Reset()
{
  sp_startMarkerStatus = 0;     // Сброс флага маркера начала пакета
  sp_stopMarkerStatus = 0;      // Сброс флага маркера конца пакета
  sp_dataLength = 0;            // Сброс флага принимаемых данных
  sp_packetAvailable = false;   // Сброс флага завершения приема пакета
}

void sp_Read()
{
  while (Serial.available() && !sp_packetAvailable)           // Пока в буфере есть что читать и пакет не является принятым
  {
    int bufferChar = Serial.read();                           // Читаем очередной байт из буфера
    if (sp_startMarkerStatus < sp_startMarker.length())       // Если стартовый маркер не сформирован (его длинна меньше той, которая должна быть)
    {
      if (sp_startMarker[sp_startMarkerStatus] == bufferChar)  // Если очередной байт из буфера совпадает с очередным байтом в маркере
      {
        sp_startMarkerStatus++;                                // Увеличиваем счетчик совпавших байт маркера
      }
      else
      {
        sp_ResetAll();                                         // Если байты не совпали, то это не маркер. Нас нае****, расходимся.
      }
    }
    else
    {
      // Стартовый маркер прочитан полностью
      if (sp_dataLength <= 0)                                // Если длинна пакета на установлена
      {
        sp_dataLength = bufferChar;                          // Значит этот байт содержит длину пакета данных
      }
      else                                                    // Если прочитанная из буфера длинна пакета больше нуля
      {
        if (sp_dataLength > sp_dataString.length())           // Если длинна пакета данных меньше той, которая должна быть
        {
          sp_dataString += (char)bufferChar;                  // прибавляем полученный байт к строке пакета
        }
        else                                                  // Если с длинной пакета данных все нормально
        {
          if (sp_stopMarkerStatus < sp_stopMarker.length())   // Если принятая длинна маркера конца пакета меньше фактической
          {
            if (sp_stopMarker[sp_stopMarkerStatus] == bufferChar) // Если очередной байт из буфера совпадает с очередным байтом маркера
            {
              sp_stopMarkerStatus++;                              // Увеличиваем счетчик удачно найденных байт маркера
              if (sp_stopMarkerStatus == sp_stopMarker.length())
              {
                // Если после прочтения очередного байта маркера, длинна маркера совпала, то сбрасываем все флаги (готовимся к приему нового пакета)
                sp_Reset();
                sp_packetAvailable = true;                        // и устанавливаем флаг готовности пакета
              }
            }
            else
            {
              sp_ResetAll();                                      // Иначе это не маркер, а х.з. что. Полный ресет.
            }
          }
          //
        }
      }
    }
  }
}

// Функция парсинга строки, которая вызывается в loop
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1  };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//Отправка данных на UART
void sp_Send(String data)
{
  Serial.print(sp_startMarker);         // Отправляем маркер начала пакета
  Serial.write(data.length());          // Отправляем длину передаваемых данных
  Serial.print(data);                   // Отправляем сами данные
  Serial.print(sp_stopMarker);          // Отправляем маркер конца пакета
}

// ============================ Encoder interrupts =============================
// Interrupt on A changing state
void doEncoderA() {
  if ( rotating ) {
    delay (1) ;  // wait a little until the bouncing is done
  }
  // Test transition, did things really change?
  if ( digitalRead(encoderA) != A_set ) { // debounce once more
    A_set = !A_set;
    // adjust counter + if A leads B
    if ( A_set && !B_set )
    {
      MenuTimeoutTimer = 10; //таймер таймаута, секунд
      if (BeepEnabled) {
        tone(BeepPin, 4000, 5);
      }
      encoderR = true;
      rotating = false;  // no more debouncing until loop() hits again
    }
  }
  wdt_reset();    // сброс отсчета сторожевого таймера
}
// Interrupt on B changing state, same as A above
void doEncoderB() {
  if ( rotating ) {
    delay (1);
  }
  if ( digitalRead(encoderB) != B_set ) {
    B_set = !B_set;
    //  adjust counter - 1 if B leads A
    if ( B_set && !A_set ) {
      MenuTimeoutTimer = 10; //таймер таймаута, секунд
      if (BeepEnabled) {
        tone(BeepPin, 4000, 5);
      }
      encoderL = true;
      rotating = false;
    }
  }
  wdt_reset();    // сброс отсчета сторожевого таймера
}

// ===== SUBROUTINES ==================================================================
// ========================================
void PrintRTC(char x, char y)
{
  lcd.setCursor(x, y);
  if (SetH && blink500ms) {
    lcd.print(F("  "));
  }
  else {
    if (Hours < 10) {
      lcd.print(F("0"));
    }
    lcd.print(Hours);
  }

  // мигающее двоеточие, если не в режиме установки времени
  if (!(SetH || SetM || PrintYesNo || blink500ms))
  {
    lcd.print(F(" "));
  }
  else {
    lcd.print(F(":"));
  }

  if (SetM && blink500ms) {
    lcd.print(F("  "));
  }
  else {
    if (Minutes < 10) {
      lcd.print(F("0"));
    }
    lcd.print(Minutes);
  }
  lcd.print(F(" "));

  if (PrintYesNo) {
    lcd.print(F("["));
    if (!(SetH || SetM || blink500ms))
    {
      lcd.print(F("   "));
    }
    else {
      if (SetYesNo)
      {
        lcd.print(F("YES"));
      }
      else {
        lcd.print(F("NO "));
      }
    }
    lcd.print(F("]"));
  }
}

// ========================================
void SetTime(char x, char y)
{
  // ========= set hours
  SetH = true;
  do {
    PrintRTC(x, y);
    rotating = true;  // reset the debouncer
    if (encoderR) {
      Hours += 1;
      if (Hours > 23) {
        Hours = 0;
      };
      encoderR = false;
    }
    if (encoderL) {
      Hours -= 1;
      if (Hours < 0) {
        Hours = 23;
      };
      encoderL = false;
    }
  }
  while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
  if (BeepEnabled) {
    tone(BeepPin, 4000, 50); //звук "YES"
  }
  SetH = false;
  delay(200);
  // ========= set minutes
  SetM = true;
  do {
    PrintRTC(0, 1);
    rotating = true;  // reset the debouncer
    if (encoderR) {
      Minutes += 1;
      if (Minutes > 59) {
        Minutes = 0;
      };
      encoderR = false;
    }
    if (encoderL) {
      Minutes -= 1;
      if (Minutes < 0) {
        Minutes = 59;
      };
      encoderL = false;
    }
  }
  while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
  if (BeepEnabled) {
    tone(BeepPin, 4000, 50); //звук "YES"
  }
  if (PrintYesNo) {
    SetM = false;
    delay(200);
    // ========= set yes-no
    SetYesNo = false;
    do {
      PrintRTC(0, 1);
      rotating = true;  // reset the debouncer
      if ((encoderR) || (encoderL)) {
        SetYesNo = !SetYesNo;
        encoderR = false;
        encoderL = false;
      }
    }
    while ((digitalRead(encoderK) == 1) | (MenuTimeoutTimer == 0));
    delay(200);
  }
}

// ============================ Timer0 interrupt =============================
// run every 500ms
void timerIsr()
{
  blink500ms = !blink500ms; // инверсия мерцающего бита
  if (blink500ms) {
    plus1sec = true; // ежесекундно взводится
    if (TstatTimer != 0) {
      TstatTimer --; // ежесекундный декремент этого таймера
    }
    if (MenuTimeoutTimer != 0) {
      MenuTimeoutTimer --; // ежесекундный декремент этого таймера
    }
  }
}
