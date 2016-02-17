//###########################################################################################
// RF3Sens (RoboForum triangulation range sensor)
// http://roboforum.ru/forum107/topic15929.html (official language for this forum - russian)
// MIT License
// 2015-2016
// Дмитрий Лилик (aka Dmitry__ @ RoboForum.ru)
// Андрей Пожогин (aka dccharacter  @ RoboForum.ru)
// Сергей Тараненко (aka setar @ RoboForum.ru)
//###########################################################################################

/* Config.h */
//###########################################################################################
//        Ручные настройки
//###########################################################################################
/*
При отсутствии флага debug_type, программа быстро обрабатывает соотв. регистр и выдает данные на led (штатная работа принтера).
При установленном флаге debug_type, по serial port передаются данные:
debug_type = 1  Передача картинки для фокусировки обьектива, колич. байт = ARRAY_WIDTH * ARRAY_HEIGHT
debug_type = 2  Для теста точности позиционирования (идея dccharacter-а). 
                Поиск порога как в штатном режиме (важна скорость), как только сработает порог - выдача картинки в serial port
debug_type = 3  Передача в текстовом виде на терминал данных: Max_Pix, Min_Pix, Pix_Sum, Shutter
debug_type = 4  Как 3-й режим, но по разрешению сигнала pin_TRIG (лог точно ограничен сигналом z_probe)
debug_type = 5  Данные перемещения мышки.
*/
#define debug_type 1
#define NUM_SAMPLES_PER_MEASURE 5 //количество данных для 4-го режима

/*
Актуально только если есть флаг debug_type
Если определено то применяется программный Serial порт (только одна нога для передачи данных)
Для платы Digispark это единственный вариант и автоматически включается на 1 ногу
Если не определено, для дебага используется Hardware Serial
*/
//#define software_serial 1 // одновременно и признак софтового serial и номер ноги для передачи данных (TX PIN)
#define SERIAL_SPEED 230400

/*
Тип сенсора, выбрать один нужный
*/
//#define sens_type_ADNS_5020
//#define sens_type_ADNS_2610
#define sens_type_ADNS_2620

/*
Используем ли питание с контроллера
*/
//#define laser_power_via_mcu // режим питания лазера с ног микроконтроллера (подаем питание на нужные ноги)
//#define laser_power_fast_pwm // используем ли управление мощьностью через pwm модуляцию (лезер включен через диод и шутирован конденсатором)
//#define laser_power_maxpix_target 100// значение MaxPix к которому стараемся отрегулировать мощность лазера (рационально ADNS_CONST_MAX или немного меньше)
//#define sens_power_via_mcu // режим питания сенсора с ног микроконтроллера (подаем питание на нужные ноги)

/*
Тип контроллера, выбрать один нужный
*/
//#define DIGI_SPARK
//#define ARDUINO_NANO // базовая распайка arduino nano
#define ARDUINO_NANO_wPOWER  // распайка сенсора на arduino nano для питания с ног микроконтроллера

/*
Алгоритм детектирования, выбрать один нужный
*/
#define Algo_MaxPix         // простейший режим - срабатывание когда в поле зрения начинает появляться краешек пятна лазера.
                            // самый стабильный, но дистанция срабатывания разная при разном цвете поверхности
//#define Algo_MaxSqualMA   // режим обнаружения максимума Squal, т.к. параметр не стабилен введено сглаживание и анализ через среднескользящие
                            // экспериментальный

#define Algo_TimeBased        // режим обнаружения минимума регулирования мощности лазера основаный на ожидаемом времени попадания пятна в центр датчика
                              // экспериментальный, базируется на геометрических размерах датчика и скорости Z 3D принтера
#define TimeBased_wait_center 250 // время в милисекундах за которое пятно перемещается от срабатывания по алгоритму Algo_MaxPix до центра датчика
//###########################################################################################
//        Конец ручных настроек
//###########################################################################################


#if (defined(DIGI_SPARK) && !defined(software_serial)) 
  #error "This board can be used only with software_serial"
#endif

#if (defined(Algo_TimeBased)&&!defined(laser_power_maxpix_target))
  #error "Time based Algoritm need to use laser_power_maxpix_target"
#endif

#if debug_type == 5
  #if defined(sens_type_ADNS_2610) || defined(sens_type_ADNS_2620)
    #error "This sensor doesn't have a motion register; debug_type 5 is unavailable"
  #endif
#endif


//###########################################################################################
//        Подключение библиотек 
//###########################################################################################
#if defined(sens_type_ADNS_5020)
  #include "sens/ADNS_5020.h"
#endif
#if defined(sens_type_ADNS_2610)
  #include "sens/ADNS_2610.h"
#endif
#if defined(sens_type_ADNS_2620)
  #include "sens/ADNS_2620.h"
#endif

#if defined(DIGI_SPARK)
#include "boards/Digispark.h"
#endif
#if defined(ARDUINO_NANO)
#include "boards/ArduinoNano.h"
#endif
#if defined(ARDUINO_NANO_wPOWER)
#include "boards/ArduinoNano_wPower.h"
#endif

#if defined(debug_type) && defined(software_serial)
  #include <SendOnlySoftwareSerial.h>
  SendOnlySoftwareSerial MyDbgSerial(software_serial, false); //true allows to connect to a regular RS232 without RS232 line driver
  #define SERIAL_OUT MyDbgSerial
#else
  #define SERIAL_OUT Serial
#endif

//###########################################################################################
//        Определения
// за пример красивого определения спасибо Денису Константинову aka linvinus  roboforum.ru
//###########################################################################################
#define GET_PIN(x) x ## _PIN
#define GET_DDR(x) x ## _DDR
#define GET_PORT(x) x ## _PORT
#define GET_IN(x) x ## _IN
  
#define PIN_OUTPUT(PIN)   GET_DDR(PIN) |= (1<<GET_PIN(PIN))
#define PIN_INPUT(PIN)   GET_DDR(PIN) &=~(1<<GET_PIN(PIN))
#define PIN_LOW(PIN)   GET_PORT(PIN) &=~(1<<GET_PIN(PIN))
#define PIN_HIGH(PIN)   GET_PORT(PIN) |= (1<<GET_PIN(PIN))
#define PIN_TOGGLE(PIN)   GET_PORT(PIN) ^= (1<<GET_PIN(PIN))
#define PIN_READ(PIN)   GET_IN(PIN) & (1<<GET_PIN(PIN))

#define NUM_PIXS (ARRAY_WIDTH * ARRAY_HEIGHT)
