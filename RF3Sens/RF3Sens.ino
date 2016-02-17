//###########################################################################################
// RF3Sens (RoboForum triangulation range sensor)
// http://roboforum.ru/forum107/topic15929.html (official language for this forum - russian)
// MIT License
// 2015-2016
// Дмитрий Лилик (aka Dmitry__ @ RoboForum.ru)
// Андрей Пожогин (aka dccharacter  @ RoboForum.ru)
// Сергей Тараненко (aka setar @ RoboForum.ru)
//###########################################################################################

#include "Config.h"
#if defined(debug_type) && defined(software_serial)
  #include <SendOnlySoftwareSerial.h>
#endif

byte ADNS_read(uint8_t address);
void ADNS_reset(void);
void ADNS_write(uint8_t addr, uint8_t data);
inline void params_grab(uint8_t *buffer);
inline void pixel_grab(uint8_t *buffer, uint16_t nBytes); 
inline void pixel_and_params_grab(uint8_t *buffer);
void ByteToString(uint8_t a);
void Uint16ToString(uint16_t a);
unsigned long currentTime;

unsigned char Str[5];
uint8_t RegPowLaser = 200;

void setup(){
  //set pin I/O direction
  #if defined(use_NCS)
    PIN_OUTPUT(NCS);
    PIN_HIGH(NCS);
  #endif
  PIN_OUTPUT(LED);
  PIN_HIGH(LED);
  #if defined(debug_type) && defined(TRIG_PIN)
    PIN_INPUT(TRIG);
  #endif

  #if defined(sens_power_via_mcu)
    #if defined(SENS_GND_PIN)
      PIN_OUTPUT(SENS_GND);
      PIN_LOW(SENS_GND);
    #endif
    #if defined(SENS_VCC_PIN)
      PIN_OUTPUT(SENS_VCC);
      PIN_HIGH(SENS_VCC);
    #endif
  #endif // sens_power_via_mcu
  #if defined(laser_power_via_mcu)
    #if defined(LASER_GND_PIN)
      PIN_OUTPUT(LASER_GND);
      PIN_LOW(LASER_GND);
    #endif
    #if defined(LASER_VCC_PIN)
      PIN_OUTPUT(LASER_VCC);
      PIN_HIGH(LASER_VCC);
      #if defined (laser_power_fast_pwm)
        analogWrite(LASER_VCC_PIN, RegPowLaser);//255=включить лазер , 0=выключить
      #endif
    #endif
  #endif // laser_power_via_mcu

//initialize SPI
    PIN_OUTPUT(NCLOCK);
    PIN_HIGH(NCLOCK);
    PIN_OUTPUT(SDIO);
    PIN_LOW(SDIO);

#if defined(debug_type)
    SERIAL_OUT.begin(SERIAL_SPEED);
#endif

  delay(1000);
  ADNS_reset();
}

void loop(){
//###########################################################################################
// штатный режим датчика для 3D принтера
#ifndef debug_type
  byte dataMax;
#if defined(laser_power_maxpix_target)
  float RegStep;
  boolean LaserPowerDropDown=false;
#endif
#if defined(Algo_MaxSqualMA)
  #define MA_LONG 10 // глубина длинной среднескользящей
  #define MA_SHORT 3 // глубина короткой среднескользящей
  byte dataSqual[MA_LONG], LastSqual;
  float MALongSqual,MAShortSqual;
  boolean laser_in_sight=false, sensed = false, SqualGrow = false;

  for (byte x=0 ; x<MA_LONG ; x++){dataSqual[x]=0;};
  MALongSqual=0; MAShortSqual=0;
#endif
#if defined(Algo_TimeBased)
  unsigned long SensedTime;
  byte BasePowLaser;
  typedef enum {
    START,
    WAIT_FIRST,
    FIRST_TIMER,
    ERR_OUT,
    FIRST_SENSED,
    SECOND_SEARCH
  }state_machine;
  state_machine state=START;
#endif//Algo_TimeBased
#if defined(Algo_TimedMaxPix)
  unsigned long SensedTime;
  byte BasePowLaser;
  typedef enum {
    START,
    WAIT_FIRST,
    FIRST_TIMER,
    ERR_OUT,
    FIRST_SENSED,
    SECOND_SEARCH
  }state_machine;
  state_machine state=START;
#endif//Algo_TimedMaxPix
  
  while(1){
    //dataMax = ADNS_read(ADNS_MAX_PIX);
    //dataAVG = ADNS_read(ADNS_PIX_SUM);
    //dataSqual = ADNS_read(ADNS_SQUAL);
    //dataSU = ADNS_read(ADNS_SHUTTER_UPPER);

//-------------------------------------------------------------------------------------------
#if defined(Algo_MaxPix)
    dataMax = ADNS_read(ADNS_MAX_PIX);
    #if defined(laser_power_maxpix_target)
      RegStep=RegPowLaser;
      while(dataMax > laser_power_maxpix_target || (dataMax < laser_power_maxpix_target && RegPowLaser < 255)){
        RefrPowerLaser(dataMax);
        delay(1);
        dataMax = ADNS_read(ADNS_MAX_PIX);
      }
      RegStep=RegStep-RegPowLaser;
      if (RegStep>15){LaserPowerDropDown=true;}//приближается поверхность
      if ((RegStep <-2) && LaserPowerDropDown) {PIN_LOW(LED);}
      if (RegPowLaser==255){LaserPowerDropDown=false;PIN_HIGH(LED);}//удаляется поверхность
      //(dataMax == laser_power_maxpix_target) && (RegPowLaser<200) ? PIN_LOW(LED) : PIN_HIGH(LED);
    #else //!laser_power_maxpix_target
      dataMax > ADNS_CONST_MAX ? PIN_LOW(LED) : PIN_HIGH(LED);
    #endif //laser_power_maxpix_target
#endif //Algo_MaxPix
//-------------------------------------------------------------------------------------------
#if defined(Algo_MaxSqualMA)
    if (laser_in_sight){ // пятно лазера в поле зрения 
      if (sensed){// максимум качества поверхности пройден
        PIN_LOW(LED);
        dataMax = ADNS_read(ADNS_MAX_PIX);
        dataMax>ADNS_CONST_MAX ? laser_in_sight=true : laser_in_sight=false;
      } else { // поиск максимума качества
        LastSqual = ADNS_read(ADNS_SQUAL);
        MALongSqual=GetSMA(dataSqual,MA_LONG,LastSqual);// для вычисления среднескользящей добавим свежие замеры
        MAShortSqual=GetSMA(dataSqual,MA_SHORT,LastSqual);// для вычисления среднескользящей добавим свежие замеры
        if ( MALongSqual > MAShortSqual){// качество уменьшается, лучше не станет - срабатываем
          PIN_LOW(LED); sensed=true; SqualGrow = false;
          MALongSqual=0; MAShortSqual=0;
          for (byte x=0 ; x<MA_LONG ; x++){dataSqual[x]=0;};
        }
      }
    } else { // пятна лазера в поле зрения нету
      PIN_HIGH(LED); sensed=false;
      dataMax = ADNS_read(ADNS_MAX_PIX);
      dataMax>ADNS_CONST_MAX ? laser_in_sight=true : laser_in_sight=false;
    }
#endif // Algo_MaxSqualMA
//-------------------------------------------------------------------------------------------
#if defined(Algo_TimeBased)
    //порегулируем мощность лазера
    while( (dataMax = ADNS_read(ADNS_MAX_PIX)) &&
           ( dataMax > laser_power_maxpix_target ||
           ( dataMax < laser_power_maxpix_target && RegPowLaser < 255) ) ){
      RefrPowerLaser(dataMax);
      delay(1);
    }
    if (RegPowLaser < 255) {                // лазер в поле зрения
      switch(state){
        case START:
          PIN_LOW(LED);                     // защитный сигнал срабатывания
          // помигаем в знак ожидания работы 255=включить лазер , 0=выключить
          analogWrite(LASER_VCC_PIN, 0);
          delay(50);
          analogWrite(LASER_VCC_PIN, RegPowLaser);
          delay(50);
         break;
        case WAIT_FIRST:
          SensedTime=millis();              // начинаем замер времени
          state=FIRST_TIMER;                // работает таймер
         break;
        case FIRST_TIMER:
          currentTime=millis();             // текущее время
          if (currentTime >= (SensedTime + TimeBased_wait_center)){ // время вышло, останавливаемся
            PIN_LOW(LED);                   // сигнал срабатывания
            BasePowLaser=RegPowLaser;       // запоминаем значение регулировки лазера
            SensedTime=currentTime;         // начинаем замер времени
            state=FIRST_SENSED;             // первый подход сработал
          }
         break;
        case FIRST_SENSED:
          if (RegPowLaser >= (BasePowLaser + 50)){ //  отошли от базового уровня на достаточное расстояние
            PIN_HIGH(LED);                   // отключаем сигнал срабатывания
            SensedTime=millis();             // начинаем замер времени
            state=SECOND_SEARCH;             // второй этап
          }
          currentTime=millis();             // текущее время
          if (currentTime >= (SensedTime + TimeBased_wait_second)){ // время вышло, повторно не тестировали
            state=START;                    // начинаем цикл по новой
          }
         break;
         case SECOND_SEARCH:
           if (RegPowLaser <= (BasePowLaser+0)){//  подошли близко к базовому уровню : срабатываем
             PIN_LOW(LED);                  // включаем сигнал срабатывания
             state=START;           // второй подход сработал
           }
           currentTime=millis();             // текущее время
           if (currentTime >= (SensedTime + TimeBased_wait_second)){ // время вышло, порог не найден
             state=START;                    // начинаем цикл по новой
           }
          break;
      } // end switch
    }else{                                  // лазер вне поля зрения 
      switch(state){
        case START:
          PIN_HIGH(LED);                    // сбросим сигнал срабатывания
          state=WAIT_FIRST;                 // переходим к ожиданию первого вхождения
         break;
        case FIRST_TIMER:
          PIN_LOW(LED);                     // сигнал срабатывания
          state=START;                      // пролетели поле зрения, авария!
         break;
         case FIRST_SENSED:
           PIN_HIGH(LED);                   // отключаем сигнал срабатывания
           SensedTime=millis();             // начинаем замер времени
           state=SECOND_SEARCH;             // было удаление после первого срабатывания
          break;
          /*
         case SECOND_SEARCH:
           PIN_LOW(LED);                    // сигнал срабатывания
           state=START;                     // пролетели поле зрения, авария!
          break;
          */
      } // end switch
    }
#endif //Algo_TimeBased
//-------------------------------------------------------------------------------------------
#if defined(Algo_TimedMaxPix)
    switch(state){
      case START:
        PIN_HIGH(LED);                    // сбросим сигнал срабатывания
      //PIN_LOW(LED);                     // защитный сигнал срабатывания
        analogWrite(LASER_VCC_PIN, 255);  // лазер на полную мощь
        delay(1);
        if (ADNS_read(ADNS_MAX_PIX) <= ADNS_CONST_MAX){ // выход пятна из поля зрения
          delay(10);
          state=WAIT_FIRST;               // переходим к ожиданию первого вхождения
        }else{                            // лазер в поле зрения
          // помигаем в знак ожидания работы 255=включить лазер , 0=выключить
          analogWrite(LASER_VCC_PIN, 0);
          delay(50);
          analogWrite(LASER_VCC_PIN, 255);
          delay(50);
        }
       break;
      case WAIT_FIRST:
        if (ADNS_read(ADNS_MAX_PIX) > ADNS_CONST_MAX){ // лазер вошел в поле зрения
          SensedTime=millis();             // начинаем замер времени
          state=FIRST_TIMER;               // переходим к ожиданию первого вхождения
        }
        PIN_HIGH(LED);                    // сбросим сигнал срабатывания
       break;
      case FIRST_TIMER:
        //порегулируем мощность лазера
        while( (dataMax = ADNS_read(ADNS_MAX_PIX)) &&
               ( dataMax > laser_power_maxpix_target ||
               ( dataMax < laser_power_maxpix_target && RegPowLaser < 255) ) ){
          RefrPowerLaser(dataMax);
          delay(1);
        }
        currentTime=millis();             // текущее время
        if (currentTime >= (SensedTime + TimeBased_wait_center)){ // время вышло, останавливаемся
          PIN_LOW(LED);                   // сигнал срабатывания
          BasePowLaser=RegPowLaser;       // запоминаем значение регулировки лазера
          SensedTime=currentTime;         // начинаем замер времени
          state=FIRST_SENSED;             // первый подход сработал
          analogWrite(LASER_VCC_PIN, BasePowLaser+1);
          delay(1);
        }
       break;
       case FIRST_SENSED:
         if ((ADNS_read(ADNS_MAX_PIX)+20) < laser_power_maxpix_target){ // выход пятна из поля зрения
           PIN_HIGH(LED);                  // сбросим сигнал срабатывания
           SensedTime=currentTime;         // начинаем замер времени
           state=SECOND_SEARCH;            // переходим к ожиданию второго вхождения
         }else{                              // пятно после первого срабатывания не должно быть в поле долго
           currentTime=millis();             // текущее время
           if (currentTime >= (SensedTime + TimeBased_wait_second)){ // время вышло
             state=START;                    // сброс по таймауту
           }
         }
        break;
        case SECOND_SEARCH:
          if (ADNS_read(ADNS_MAX_PIX) >= laser_power_maxpix_target){ // лазер вошел в поле зрения
            PIN_LOW(LED);                   // сигнал срабатывания
            state=START;                    // цикл успешно завершен
            delay(500);
          }else{                            // пятно не должно быть вне поля долго
            currentTime=millis();             // текущее время
            if (currentTime >= (SensedTime + TimeBased_wait_second)){ // время вышло
              state=START;                    // сброс по таймауту
            }
          }
         break;
    } // end switch
#endif //Algo_TimedMaxPix
//-------------------------------------------------------------------------------------------
  }
//###########################################################################################
// отладочные режимы
#else
//-------------------------------------------------------------------------------------------
#if debug_type ==1
//-------------------------------------------------------------------------------------------
  byte Frame[NUM_PIXS + 7],dataMax, dataSU;

  while(1){
    #if defined(laser_power_maxpix_target)
      dataMax = ADNS_read(ADNS_MAX_PIX);
      while(dataMax > laser_power_maxpix_target || (dataMax < laser_power_maxpix_target && RegPowLaser < 255)){
        RefrPowerLaser(dataMax);
        delay(1);
        dataMax = ADNS_read(ADNS_MAX_PIX);
      }
    #endif //laser_power_maxpix_target
    pixel_and_params_grab(Frame);
    SERIAL_OUT.write(Frame, NUM_PIXS + 7); // send frame in raw format
    delay(2);
  }
//-------------------------------------------------------------------------------------------
#elif debug_type ==2
//-------------------------------------------------------------------------------------------
  byte Frame[NUM_PIXS + 7];

  byte data;
  while(1){
    //data = ADNS_read(ADNS_SQUAL);
    data = ADNS_read(ADNS_MAX_PIX);
    //data = ADNS_read(ADNS_PIX_SUM);

    if(data > ADNS_CONST_MAX){
      PIN_LOW(LED);
    }
    else{
      PIN_HIGH(LED);
      pixel_and_params_grab(Frame);
      SERIAL_OUT.write(Frame, NUM_PIXS+7); // send frame in raw format
    }
  }
//-------------------------------------------------------------------------------------------
#elif debug_type ==3
//-------------------------------------------------------------------------------------------
  //листинг для электронных таблиц: В шапке названия, дальше только данные разделенные "tab".
  byte Frame[7],dataMax;
#if defined(Algo_MaxSqualMA)
  #define MA_LONG 10 // глубина длинной среднескользящей
  #define MA_SHORT 3 // глубина короткой среднескользящей
  byte dataSqual[MA_LONG], LastSqual;
  float MALongSqual,MAShortSqual;
  for (byte x=0 ; x<MA_LONG ; x++){dataSqual[x]=0;};
  MALongSqual=0; MAShortSqual=0;
  //заголовок
  SERIAL_OUT.println  (F  ("Squal:\tSqualMA_l:\tMax:\tMin:\tSum:\tShutter:\tLaserPower:"));
#else
  //заголовок
  SERIAL_OUT.println  (F  ("Squal:\tMax:\tMin:\tSum:\tShutter:\tLaserPower:"));
#endif //Algo_MaxSqualMA
  while(1){
    #if defined(laser_power_maxpix_target)
      dataMax = ADNS_read(ADNS_MAX_PIX);
      while(dataMax > laser_power_maxpix_target || (dataMax < laser_power_maxpix_target && RegPowLaser < 255)){
        RefrPowerLaser(dataMax);
        delay(1);
        dataMax = ADNS_read(ADNS_MAX_PIX);
      }
    #endif //laser_power_maxpix_target
    params_grab(Frame);
#if defined(Algo_MaxSqualMA)
    LastSqual = Frame[0];
    MALongSqual=GetSMA(dataSqual,MA_LONG,LastSqual);// для вычисления среднескользящей добавим свежие замеры
    //MAShortSqual=GetSMA(dataSqual,MA_SHORT,LastSqual);// для вычисления среднескользящей добавим свежие замеры
    //dtostrf(MALongSqual, 5, 3, (char *)Str);
    ByteToString(Frame[0]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
    SERIAL_OUT.print(MALongSqual,2);
    SERIAL_OUT.write(0x09);
#else 
    ByteToString(Frame[0]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
#endif //Algo_MaxSqualMA
    ByteToString(Frame[1]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
    ByteToString(Frame[2]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
    ByteToString(Frame[3]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
    Uint16ToString(Frame[4] *256 + Frame[5]);
    SERIAL_OUT.write(Str[4]);
    SERIAL_OUT.write(Str[3]);
    SERIAL_OUT.write(Str[2]);
    SERIAL_OUT.write(Str[1]);
    SERIAL_OUT.write(Str[0]);
    SERIAL_OUT.write(0x09);
    ByteToString(Frame[6]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
    SERIAL_OUT.write(0x0d);
    SERIAL_OUT.write(0x0a);

#if SERIAL_SPEED > 115200
      delay(20);
#else
      delay(60);  //задержка больше из-за низкой скорости Serial
    #endif
  }
//-------------------------------------------------------------------------------------------
#elif debug_type ==4
//-------------------------------------------------------------------------------------------
  //Как 3-й режим, но по разрешению сигнала pin_TRIG (лог точно ограничен сигналом z_probe)
  byte Frame[7];
  while(1){
    if(PIN_READ(TRIG)){
      //заголовок
      SERIAL_OUT.println (F  ("Squal:\tMax:\tMin:\tSum:\tShutter:\tLaserPower:"));

      params_grab(Frame);

      ByteToString(Frame[0]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
      ByteToString(Frame[1]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
      ByteToString(Frame[2]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
      ByteToString(Frame[3]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
      Uint16ToString(Frame[4] *256 + Frame[5]);
      SERIAL_OUT.write(Str[4]);
      SERIAL_OUT.write(Str[3]);
      SERIAL_OUT.write(Str[2]);
      SERIAL_OUT.write(Str[1]);
      SERIAL_OUT.write(Str[0]);
      SERIAL_OUT.write(0x09);
      ByteToString(Frame[6]); SERIAL_OUT.write(Str[2]); SERIAL_OUT.write(Str[1]); SERIAL_OUT.write(Str[0]); SERIAL_OUT.write(0x09);
      SERIAL_OUT.write(0x0a);
      //SERIAL_OUT.write(0x0d);

#if SERIAL_SPEED > 115200
      delay(20);
#else
      delay(60);  //задержка больше из-за низкой скорости Serial
#endif
    }
  }
//-------------------------------------------------------------------------------------------
#elif debug_type ==5
//-------------------------------------------------------------------------------------------
  while(1){
    unsigned int motion = 0;
    //check for movement
    motion = ADNS_read(ADNS_MOTION);
    if (motion > 127){
      //there has been a motion! Print the x and y movements
      SERIAL_OUT.println("X:" + String(ADNS_read(ADNS_DELTA_X)) + " Y:" + String(ADNS_read(ADNS_DELTA_Y)));
    }
  }
//-------------------------------------------------------------------------------------------
#endif
#endif
}

//###########################################################################################
// процедуры
//-------------------------------------------------------------------------------------------
#if defined(laser_power_maxpix_target)
void RefrPowerLaser(uint8_t power){
  if (power > laser_power_maxpix_target && RegPowLaser > 1){
    RegPowLaser--;
    analogWrite(LASER_VCC_PIN,RegPowLaser);
  }
  if (power < laser_power_maxpix_target && RegPowLaser < 255){
    RegPowLaser++;
    analogWrite(LASER_VCC_PIN,RegPowLaser);
  }
}
#endif
//-------------------------------------------------------------------------------------------
#if defined(Algo_MaxSqualMA)
float GetSMA(uint8_t *buffer,byte depth,byte LastValue)
{
  float result=0;
  byte temp;
  for (byte x=depth-1 ; x>0 ; x--){
    temp = buffer[x-1];
    *(buffer + x) = temp;//сдвигаем массив результатов
    result=result + *(buffer + x);
  } 
  result = (result + LastValue)/depth;
  return result;
}
#endif
//-------------------------------------------------------------------------------------------
void ADNS_reset(void){
#ifdef sens_type_ADNS_5020
  ADNS_write(0x3a,0x5a);
  delay(1000);
  ADNS_write(0x0d, 0x01);  // Set 1000cpi resolution
  delay(1000);
#endif
#if  defined(sens_type_ADNS_2610) || defined(sens_type_ADNS_2620)
  ADNS_write(ADNS_CONF,0x80);
  delay(1000);
  ADNS_write(ADNS_CONF,0x01); //Always awake
#endif
}

//-------------------------------------------------------------------------------------------
void ADNS_write(byte address, byte data){
  #if defined(use_NCS)
    PIN_LOW(NCS);
  #endif
  // send in the address and value via SPI:
  address |= 0x80;  //признак записи адреса
  for (byte i = 0x80; i; i >>= 1){
    PIN_LOW(NCLOCK);
    address & i ? PIN_HIGH(SDIO) : PIN_LOW(SDIO);
    asm volatile ("nop");
    PIN_HIGH(NCLOCK);
  }

  //delayMicroseconds(1);

  for (byte i = 0x80; i; i >>= 1){
    PIN_LOW(NCLOCK);
    data & i ? PIN_HIGH(SDIO) : PIN_LOW(SDIO);
    asm volatile ("nop");
    PIN_HIGH(NCLOCK);
  }
  //t SWW. SPI Time between Write Commands
  delayMicroseconds(ADNS_DELAY_TSWW);

  #if defined(use_NCS)
    PIN_HIGH(NCS);
  #endif
}

//-------------------------------------------------------------------------------------------
byte ADNS_read(byte address){
  #if defined(use_NCS)
    PIN_LOW(NCS);
  #endif

  address &= ~0x80;  //признак записи данных
  for (byte i = 0x80; i; i >>= 1){
    PIN_LOW(NCLOCK);
    address & i ? PIN_HIGH(SDIO) : PIN_LOW(SDIO);
    asm volatile ("nop");
    PIN_HIGH(NCLOCK);
  }

  // prepare io pin for reading
  PIN_INPUT(SDIO);

  // t SRAD. SPI Read Address-Data Delay
  delayMicroseconds(ADNS_DELAY_TSRAD);

  // read the data
  byte data = 0;
  for (byte i = 8; i; i--){
    // tick, tock, read
    PIN_LOW(NCLOCK);
    asm volatile ("nop");
    PIN_HIGH(NCLOCK);
    data <<= 1;
    if (PIN_READ(SDIO)) data |= 0x01;
  }

  #if defined(use_NCS)
    PIN_HIGH(NCS);
  #endif
  PIN_OUTPUT(SDIO);

  // t SRW & t SRR = 1μs.
  delayMicroseconds(2);
  return data;
}
//-------------------------------------------------------------------------------------------
inline void pixel_grab(uint8_t *buffer, uint16_t nBytes) {
  uint8_t temp_byte;

  //reset the pixel grab counter
  ADNS_write(ADNS_PIX_GRAB, 0x00);

  for (uint16_t count = 0; count < nBytes; count++) {
    while (1) {
      temp_byte = ADNS_read(ADNS_PIX_GRAB);
      if (temp_byte & ADNS_PIX_DATA_VALID) {
        break;
      }
    }
    *(buffer + count) = temp_byte & ADNS_MASK_PIX;  // only n bits are used for data
  }
}
//-------------------------------------------------------------------------------------------
inline void params_grab(uint8_t *buffer) {
	*(buffer + 0) = ADNS_read(ADNS_SQUAL);
	*(buffer + 1) = ADNS_read(ADNS_MAX_PIX);
	*(buffer + 2) = ADNS_read(ADNS_MIN_PIX);
	*(buffer + 3) = ADNS_read(ADNS_PIX_SUM);
	*(buffer + 4) = ADNS_read(ADNS_SHUTTER_UPPER);
	*(buffer + 5) = ADNS_read(ADNS_SHUTTER_LOWER);
  *(buffer + 6) = RegPowLaser;
}
//-------------------------------------------------------------------------------------------
inline void pixel_and_params_grab(uint8_t *buffer) {
	params_grab((buffer + NUM_PIXS));
	pixel_grab(buffer, NUM_PIXS);
}
//-------------------------------------------------------------------------------------------
void ByteToString(uint8_t a){
  Str[0] = '0';
  Str[1] = '0';
  Str[2] = '0';
  if (!a) return;
  Str[0] = a % 10 + '0'; a /= 10;
  if (!a) return;
  Str[1] = a % 10 + '0'; a /= 10;
  if (!a) return;
  Str[2] = a % 10 + '0';
}
//-------------------------------------------------------------------------------------------
void Uint16ToString(uint16_t a){
  Str[0] = '0';
  Str[1] = '0';
  Str[2] = '0';
  Str[3] = '0';
  Str[4] = '0';
  if (!a) return;
  Str[0] = a % 10 + '0'; a /= 10;
  if (!a) return;
  Str[1] = a % 10 + '0'; a /= 10;
  if (!a) return;
  Str[2] = a % 10 + '0'; a /= 10;
  if (!a) return;
  Str[3] = a % 10 + '0'; a /= 10;
  if (!a) return;
  Str[4] = a % 10 + '0';
}
//-------------------------------------------------------------------------------------------
uint8_t ByteToAscii_h(uint8_t x){
  uint8_t tmpByte;

  tmpByte = (x >> 4) + 0x30;
  if (tmpByte > 0x39) { tmpByte += 0x07; }
  return tmpByte;
}
//-------------------------------------------------------------------------------------------
uint8_t ByteToAscii_l(uint8_t x){
  uint8_t tmpByte;
  tmpByte = (x & 0x0f) + 0x30;
  if (tmpByte > 0x39) { tmpByte += 0x07; }
  return tmpByte;
}
