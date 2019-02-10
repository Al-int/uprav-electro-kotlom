/*

Термостат для теплого пола с имзерением температуры в подвале и отправкой их по UART
и преёмом нового порога термостатирования

 >DS18B20 thermal sensor 2шт
 */
 
#include <avr/wdt.h>
#include <Wire.h> // i2c (для RTC)
#include <EEPROMex.h> // EE
#include <TimerOne.h> // прерывания по таймеру1
 
#include <OneWire.h> // 1wire для DS18B20
#include <DallasTemperature.h> // DS18B20
 
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);



// ***************************  DeviceAddress DS18B20Address;  ???????????????

DeviceAddress ThermometerL = {
  0x28, 0xFF, 0xD7, 0x3F, 0x82, 0x16, 0x03, 0x1C };  // Прописываем все датчики, адрес в обратном порядке 
    
DeviceAddress ThermometerP = {
  0x28, 0x48, 0x33, 0x4B, 0x03, 0x00, 0x00, 0x90 };  // Прописываем все датчики, адрес в обратном порядке 



 
#define Relay  13 // нога, к которой подключено реле
#define RelayOn HIGH // полярность сигнала включения реде (HIGH/LOW)
 

 
#define serialenabled // раскомментировать для выдачи в порт отладочной инфы
 
#define TstatTimerMax 60 //минимальная пауза между включениями горелки, сек
unsigned int TstatTimer = 20; //таймер паузы между включениями/выключениями, начальная установка 20 сек для устаканивания системы после сброса
 
float DS18B20TemperatureL = 0; //сырая температура от датчика котла
float TemperatureL = 0; //вычисленная температура котла с коррекцией
float DS18B20TempTmpL; //времянка
float DS18B20TemperatureP = 0; //сырая температура от датчика бойлера
float TemperatureP = 0; //вычисленная температура бойлера с коррекцией
float DS18B20TempTmpP; //времянка
byte DS18B20iteration = 0; //счётчик измерений температуры для усреднения
 
float TstatTemp = 30; //температура термостатирования, может изменяться настройками
float TemperatureCorr = 0; //коррекция температуры, может изменяться настройками
float Hysteresis = 1; // гистерезис термостата, может изменяться настройками

boolean IzmerenoYesNo = false; // показывать ли после времени Yes/No (косвенно - указание на режим установка/отображение)
 
boolean blink500ms = false; // мигающий бит, инвертируется каждые 500мс
boolean plus1sec = false; // ежесекундно взводится
 
byte MenuTimeoutTimer;
 
float AlarmTemp = 5; // температура для замерзательного орала
 
 
// EEPROM
EEMEM float TstatTempEE; //EE температура термостатирования
EEMEM float TemperatureCorrEE; // EE коррекция температуры
EEMEM float HysteresisEE; // EE гистерезис
EEMEM float AlarmTempEE; // EE значение недопустимого снижения температуры


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
int unit=2;            //метка для приемника

// Объявляем переменные строки для приема данных
float unitRead;
float t1;







// ===== SETUP ========================================================================
void setup() {
  wdt_disable(); // отключение сторожевого таймера
  Serial.begin(9600);
  sp_SetUp();                                       // Инициализируем протокол.

  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, !RelayOn);

  Timer1.initialize(500000); // Timer0 interrupt - set a timer of length 500000 microseconds
  Timer1.attachInterrupt( timerIsr ); // attach the service routine here
  EEPROM.setMaxAllowedWrites(32767);
   
/*     // раскоментировать если первая запись устройства 

    EEPROM.updateFloat(int(&TstatTempEE), TstatTemp);
    EEPROM.updateFloat(int(&TemperatureCorrEE), TemperatureCorr);
    EEPROM.updateFloat(int(&HysteresisEE), Hysteresis);
    EEPROM.updateFloat(int(&AlarmTempEE), AlarmTemp);
*/
  
  
 
  TstatTemp = EEPROM.readFloat(int(&TstatTempEE));
  TemperatureCorr = EEPROM.readFloat(int(&TemperatureCorrEE));
  Hysteresis = EEPROM.readFloat(int(&HysteresisEE));
  AlarmTemp = EEPROM.readFloat(int(&AlarmTempEE));
 
  DS18B20.begin();
  DS18B20.setResolution(ThermometerL, 12);
  DS18B20.setResolution(ThermometerP, 12);
  DS18B20.setWaitForConversion(false);
  DS18B20.requestTemperatures();

  stringData = String();
  stringSeparator = String(";");
  
  wdt_enable (WDTO_8S); // включение сторожевого таймера
  Serial.println("Watchdog enabled.");

}
 
// ===== MAIN CYCLE ===================================================================
void loop() {
 
  if ( TstatTimer == 0 && IzmerenoYesNo != false)
  {
// термостатирование теплый пол
    if ( TemperatureL > TstatTemp ) // гистерезис
    {
      if ( digitalRead(Relay) == RelayOn ) // если горелка включена -
      {
        digitalWrite(Relay, !RelayOn); // выключить горелку
      }
    }
    if (TemperatureL < TstatTemp - Hysteresis  )
    {
      if ( digitalRead(Relay) == !RelayOn ) // если горелка выключена -
      {
        digitalWrite(Relay, RelayOn); // включить горелку
      }
    }
    
     //собираем данные для отправки
     stringData = unit + stringSeparator +  TemperatureL + stringSeparator + TemperatureP + stringSeparator + TstatTemp;
     sp_Send(stringData);  //отсылаем

    TstatTimer = TstatTimerMax;
  }
 
 
  // если прошла 1 секунда - делаем ежесекундные дела
  if (plus1sec) {
    plus1sec = false; // сбрасываем до следующей секунды

 
    // измеряем температуру 
    DS18B20TempTmpL = DS18B20.getTempC(ThermometerL); // получить температуру от датчика
    DS18B20TempTmpP = DS18B20.getTempC(ThermometerP); // получить температуру от датчика
    DS18B20.requestTemperatures();  // запустить новое измерение
    if (DS18B20TempTmpL != -127 && DS18B20TempTmpP != -127)
    {
      DS18B20TemperatureL += DS18B20TempTmpL; // суммируем для усреднения
      DS18B20TemperatureP += DS18B20TempTmpP; // суммируем для усреднения
      DS18B20iteration ++;
      if (DS18B20iteration == 10)
      {
        DS18B20iteration = 0;
        IzmerenoYesNo = true;
        TemperatureL = (DS18B20TemperatureL / 10) + TemperatureCorr; //усреднённая + коррекция
        TemperatureP = (DS18B20TemperatureP / 10) + TemperatureCorr; //усреднённая + коррекция
        DS18B20TemperatureL = 0;
        DS18B20TemperatureP = 0;
      }
    }
    else
    {
     IzmerenoYesNo = false; 
    }
    delay(500); //ибо часто проверять температуру это зло 
   /* if (TemperatureP < AlarmTemp) {
      // ченить придумать или не нада
    }*/
  }
   sp_Read();
   if(sp_packetAvailable == true)
    {
      String data_0 = getValue(sp_dataString, ';', 0 );  //присваиваем переменной первый набор символов разделённые ";" т.е 12.34
      String data_1 = getValue(sp_dataString, ';', 1 );  //присваиваем переменной второй набор символов разделённые ";" т.е 12.34

      //преобразовываем в числа
      unitRead = data_0.toFloat();
      t1 = data_1.toFloat();
      sp_packetAvailable = false;
    
    if( t1 != TstatTemp)
    {
      TstatTemp = t1;
       EEPROM.updateFloat(int(&TstatTempEE), TstatTemp);
    }
  }

    wdt_reset();    // сброс отсчета сторожевого таймера

}

// ============================ Timer0 interrupt =============================
// run every 500ms
void timerIsr()
{
  blink500ms = !blink500ms; // инверсия мерцающего бита
  if(blink500ms) {
    plus1sec = true; // ежесекундно взводится
    if (TstatTimer != 0) {
      TstatTimer --; // ежесекундный декремент этого таймера
    }
    if (MenuTimeoutTimer != 0) {
      MenuTimeoutTimer --; // ежесекундный декремент этого таймера
    }
  }
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
  while(Serial.available() && !sp_packetAvailable)            // Пока в буфере есть что читать и пакет не является принятым
  {
    int bufferChar = Serial.read();                           // Читаем очередной байт из буфера
    if(sp_startMarkerStatus < sp_startMarker.length())        // Если стартовый маркер не сформирован (его длинна меньше той, которая должна быть) 
    {  
     if(sp_startMarker[sp_startMarkerStatus] == bufferChar)   // Если очередной байт из буфера совпадает с очередным байтом в маркере
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
       if(sp_dataLength <= 0)                                 // Если длинна пакета на установлена
       {
         sp_dataLength = bufferChar;                          // Значит этот байт содержит длину пакета данных
       }
      else                                                    // Если прочитанная из буфера длинна пакета больше нуля
      {
        if(sp_dataLength > sp_dataString.length())            // Если длинна пакета данных меньше той, которая должна быть
        {
          sp_dataString += (char)bufferChar;                  // прибавляем полученный байт к строке пакета
        }
        else                                                  // Если с длинной пакета данных все нормально
        {
          if(sp_stopMarkerStatus < sp_stopMarker.length())    // Если принятая длинна маркера конца пакета меньше фактической
          {
            if(sp_stopMarker[sp_stopMarkerStatus] == bufferChar)  // Если очередной байт из буфера совпадает с очередным байтом маркера
            {
              sp_stopMarkerStatus++;                              // Увеличиваем счетчик удачно найденных байт маркера
              if(sp_stopMarkerStatus == sp_stopMarker.length())
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
 int maxIndex = data.length()-1;
 for(int i=0; i<=maxIndex && found<=index; i++){
 if(data.charAt(i)==separator || i==maxIndex){ found++;
                                               strIndex[0] = strIndex[1]+1;
                                               strIndex[1] = (i == maxIndex) ? i+1 : i;}
                                                }
return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//Отправка данных на UART
void sp_Send(String data)
{
  Serial.print(sp_startMarker);         // Отправляем маркер начала пакета
  Serial.write(data.length());          // Отправляем длину передаваемых данных
  Serial.print(data);                   // Отправляем сами данные
  Serial.print(sp_stopMarker);          // Отправляем маркер конца пакета
}
