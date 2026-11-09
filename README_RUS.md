# Драйвер INA226 для STM32 HAL

Изменения и совместимость: [CHANGELOG.md](CHANGELOG.md).

English documentation: [README.md](README.md)

## Назначение

Драйвер предназначен для работы с измерителем тока, напряжения и мощности INA226 в проектах STM32CubeIDE с использованием STM32 HAL.

Драйвер не привязан к конкретной серии STM32. Требуется реализация STM32 HAL с совместимыми блокирующими функциями I2C.

Основные свойства:

- поддержка нескольких INA226 одновременно;
- отдельный `INA226_Handle_t` для каждого физического устройства;
- возможность использовать разные аппаратные контроллеры I2C;
- нормальные 7-битные I2C-адреса в публичном API;
- проверка Manufacturer ID и Die ID при инициализации;
- автоматический расчёт Current LSB, Power LSB и Calibration Register;
- continuous, triggered и power-down режимы;
- настройка averaging и времени преобразования;
- поддержка ALERT, пороговых событий, полярности и latch;
- прямой доступ к 16-битным регистрам;
- отсутствие глобального состояния внутри драйвера.

Драйвер использует блокирующие функции STM32 HAL:

```c
HAL_I2C_Mem_Read()
HAL_I2C_Mem_Write()
HAL_I2C_IsDeviceReady()
```

## Файлы

Репозиторий содержит:

```text
ina226.h
ina226.c
example.c
```

Добавьте `ina226.c` и `ina226.h` в приложение и укажите их каталог в путях поиска компилятора. `example.c` является необязательным примером интеграции.

В `ina226.c` заголовки подключаются в следующем порядке:

```c
#include "main.h"
#include "ina226.h"
```

`ina226.h` намеренно не подключает `main.h` или заголовок STM32 HAL. Тип `I2C_HandleTypeDef` должен быть определён до подключения `ina226.h`. В обычном проекте STM32CubeIDE достаточно следующего:

```c
#include "main.h"
#include "ina226.h"
```

## Минимальный пример

После создания и инициализации I2C:

```c
INA226_Handle_t ina;
INA226_Data_t data;
INA226_Status_t status;

status = INA226_Init(&ina, &hi2c2, INA226_ADDR_GND_GND, 0.1f, 0.8f);

if (status != INA226_OK)
{
    Error_Handler();
}
```

Периодическое чтение:

```c
while (1)
{
    status = INA226_Read(&ina, &data);

    if (status == INA226_OK)
    {
        /* data.bus_voltage_mV */
        /* data.shunt_voltage_uV */
        /* data.current_mA */
        /* data.power_mW */
    }

    HAL_Delay(100U);
}
```

После `INA226_Init()` микросхема по умолчанию работает непрерывно. Запускать каждое преобразование из главного цикла не требуется.

## Аппаратные ограничения INA226

### Диапазон напряжения шунта

INA226 имеет фиксированный диапазон напряжения на шунте приблизительно:

```text
-81.92 mV ... +81.92 mV
```

В отличие от INA219, программируемого PGA у INA226 нет.

Для шунта `R100`, то есть `0.1 Ω`, максимальный симметричный измеряемый ток составляет приблизительно:

```text
81.9175 mV / 0.1 Ω = 0.819175 A
```

Для шунта `0.01 Ω`, который обычно маркируется `R010`, соответствующий диапазон составляет приблизительно `±8.19175 A`. В `INA226_Init()` необходимо передавать фактическое сопротивление по спецификации компонента: маркировка вроде `R10` может трактоваться неоднозначно.

Этот расчёт описывает только измерительный диапазон INA226. Мощность шунта, дорожки платы, разъёмы и проводка должны быть рассчитаны на фактический ток.

В драйвер передаются фактическое сопротивление шунта и максимальный ожидаемый ток по модулю:

```c
INA226_Init(&ina, &hi2c2, INA226_ADDR_GND_GND, 0.1f, 0.8f);
```

`max_current_A` задаёт масштаб Current Register и не должен превышать физический предел `81.9175 mV / Rshunt`. Меньшее значение даёт более мелкий Current LSB, но ток за пределами настроенного диапазона может установить OVF. Предел защиты при необходимости задаётся отдельно через ALERT.

### Дискретность измерений

```text
Bus Voltage LSB:    1.25 mV
Shunt Voltage LSB:  2.5 µV
Power LSB:          25 × Current LSB
```

Shunt Voltage и Current являются знаковыми величинами. Драйвер интерпретирует соответствующие регистры как `int16_t`.

Power Register является беззнаковым.

## I2C-адрес

Публичный API принимает обычный 7-битный адрес от `0x40` до `0x4F`. Внутри драйвера перед вызовом HAL он сдвигается влево:

```c
dev->address << 1
```

Не следует передавать в `INA226_Init()` уже сдвинутый адрес.

Стандартный адрес при подключении A1 и A0 к GND:

```c
INA226_ADDR_GND_GND /* 0x40 */
```

В заголовочном файле определены все 16 комбинаций A1 и A0:

| A1 | A0 | Макрос | Адрес |
| --- | --- | --- | ---: |
| GND | GND | `INA226_ADDR_GND_GND` | `0x40` |
| GND | VS | `INA226_ADDR_GND_VS` | `0x41` |
| GND | SDA | `INA226_ADDR_GND_SDA` | `0x42` |
| GND | SCL | `INA226_ADDR_GND_SCL` | `0x43` |
| VS | GND | `INA226_ADDR_VS_GND` | `0x44` |
| VS | VS | `INA226_ADDR_VS_VS` | `0x45` |
| VS | SDA | `INA226_ADDR_VS_SDA` | `0x46` |
| VS | SCL | `INA226_ADDR_VS_SCL` | `0x47` |
| SDA | GND | `INA226_ADDR_SDA_GND` | `0x48` |
| SDA | VS | `INA226_ADDR_SDA_VS` | `0x49` |
| SDA | SDA | `INA226_ADDR_SDA_SDA` | `0x4A` |
| SDA | SCL | `INA226_ADDR_SDA_SCL` | `0x4B` |
| SCL | GND | `INA226_ADDR_SCL_GND` | `0x4C` |
| SCL | VS | `INA226_ADDR_SCL_VS` | `0x4D` |
| SCL | SDA | `INA226_ADDR_SCL_SDA` | `0x4E` |
| SCL | SCL | `INA226_ADDR_SCL_SCL` | `0x4F` |

## Типы данных

### `INA226_Handle_t`

Содержит состояние одного физического INA226:

```c
INA226_Handle_t ina;
```

В handle хранятся:

- указатель на используемый `I2C_HandleTypeDef`;
- 7-битный адрес устройства;
- сопротивление шунта;
- максимальный ожидаемый ток;
- Current LSB и Power LSB;
- рассчитанное значение Calibration Register;
- текущая конфигурация;
- настройки ALERT;
- состояние инициализации.

После успешного `INA226_Init()` поля handle не следует изменять вручную.

### `INA226_Data_t`

Содержит преобразованные результаты измерений:

```c
typedef struct
{
    float bus_voltage_mV;
    float shunt_voltage_uV;
    float current_mA;
    float power_mW;
} INA226_Data_t;
```

Единицы измерения являются частью имени каждого поля.

`INA226_Read()` записывает структуру только после успешного чтения всех четырёх регистров. При ошибке I2C предыдущие данные в структуре сохраняются.

### `INA226_AlertStatus_t`

Содержит флаги Mask/Enable Register:

```c
typedef struct
{
    bool alert_function;
    bool conversion_ready;
    bool overflow;
} INA226_AlertStatus_t;
```

- `alert_function` — сработало настроенное пороговое условие;
- `conversion_ready` — завершено преобразование;
- `overflow` — внутренний расчёт Current или Power вышел за диапазон регистра.

### `INA226_Status_t`

| Значение | Описание |
|---|---|
| `INA226_OK` | Операция выполнена успешно |
| `INA226_ERROR_PARAM` | Некорректный указатель, адрес, режим или параметр |
| `INA226_ERROR_I2C` | Ошибка обмена по I2C |
| `INA226_ERROR_TIMEOUT` | Таймаут HAL или незавершившийся reset |
| `INA226_ERROR_NOT_FOUND` | Устройство не отвечает или не прошло проверку ID |
| `INA226_ERROR_OVERFLOW` | Установлен Math Overflow Flag |

`HAL_StatusTypeDef` за пределы драйвера не передаётся.

## Инициализация

### `INA226_Init()`

```c
INA226_Status_t INA226_Init(INA226_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t address, float shunt_ohm, float max_current_A);
```

Параметры:

- `dev` — handle конкретного INA226;
- `hi2c` — используемый контроллер I2C STM32;
- `address` — 7-битный адрес INA226;
- `shunt_ohm` — сопротивление шунта в омах;
- `max_current_A` — максимальный ожидаемый ток по модулю в амперах.

Функция выполняет:

1. Проверку указателей и числовых параметров.
2. Проверку адреса `0x40 ... 0x4F`.
3. Проверку `max_current_A × shunt_ohm` относительно положительного предела `81.9175 mV`.
4. Расчёт минимального Current LSB как `max_current_A / 32767`.
5. Расчёт Power LSB и Calibration Register с проверкой допустимого 15-битного значения.
6. Проверку присутствия устройства через `HAL_I2C_IsDeviceReady()`.
7. Проверку Manufacturer ID `0x5449`.
8. Проверку Die ID `0x2260` с маскированием revision bits.
9. Программный reset.
10. Запись Configuration Register.
11. Запись Calibration Register.

Конфигурация по умолчанию:

```text
Averaging:               16 samples
Bus conversion time:     1.1 ms
Shunt conversion time:   1.1 ms
Mode:                    Shunt + Bus Continuous
```

Полный период обновления при такой конфигурации составляет приблизительно:

```text
(1.1 ms + 1.1 ms) × 16 = 35.2 ms
```

Масштаб Current Register рассчитывается по формулам даташита:

```text
Minimum Current LSB = Maximum Expected Current / 32767
Calibration         = 0.00512 / (Current LSB × Rshunt)
Power LSB           = 25 × Current LSB
```

Для максимального разрешения драйвер использует минимальный Current LSB без округления вверх. Для шунта `R100` и `max_current_A = 0.8 A`:

```text
Current LSB = 24.4158 µA
Power LSB   = 610.396 µW
Calibration = 2097 = 0x0831
```

Заданный ток является диапазоном масштабирования Current Register, а не пределом защиты и не расширяет физический диапазон шунтового входа. Значения `shunt_ohm` и `max_current_A` должны быть конечными и положительными. Если падение на шунте превышает `81.9175 mV` либо рассчитанная калибровка не помещается в 15 бит, функция возвращает `INA226_ERROR_PARAM`.

## Чтение измерений

### `INA226_Read()`

```c
INA226_Status_t INA226_Read(INA226_Handle_t *dev, INA226_Data_t *data);
```

Функция последовательно и однократно читает:

1. Bus Voltage Register;
2. Shunt Voltage Register;
3. Current Register;
4. Power Register.

Затем значения преобразуются в физические единицы и записываются в `INA226_Data_t`.

Функция намеренно не читает Mask/Enable Register. Поэтому она:

- не сбрасывает Conversion Ready Flag;
- не подтверждает latched ALERT;
- не проверяет Math Overflow Flag;
- одинаково работает при `latch = false` и `latch = true`.

Если необходимо проверять OVF, это делается отдельно через `INA226_IsOverflow()` или `INA226_GetAlertStatus()`.

Четыре регистра читаются последовательно, поэтому при очень коротком времени преобразования теоретически часть значений может относиться к соседним циклам преобразования. Драйвер не останавливает continuous mode и не повторяет чтение, чтобы время выполнения было ограниченным и предсказуемым.

### Отдельные функции чтения

```c
INA226_Status_t INA226_GetBusVoltage_mV(INA226_Handle_t *dev, float *voltage_mV);
INA226_Status_t INA226_GetShuntVoltage_uV(INA226_Handle_t *dev, float *voltage_uV);
INA226_Status_t INA226_GetCurrent_mA(INA226_Handle_t *dev, float *current_mA);
INA226_Status_t INA226_GetPower_mW(INA226_Handle_t *dev, float *power_mW);
```

`INA226_GetBusVoltage_mV()` и `INA226_GetShuntVoltage_uV()` читают только соответствующий регистр.

Обе функции проверяют OVF при любом LEN. Аппаратная защёлка подтверждается; AFF и CVRF сохраняются программно до получения функциями статуса.

Если в программе нужны все четыре величины, предпочтительнее один вызов `INA226_Read()`, а не четыре отдельных вызова.

## Конфигурирование АЦП и режима

### `INA226_Configure()`

```c
INA226_Status_t INA226_Configure(INA226_Handle_t *dev, INA226_Averaging_t averaging, INA226_ConversionTime_t bus_time, INA226_ConversionTime_t shunt_time, INA226_Mode_t mode);
```

Функция одновременно изменяет:

- количество усреднений;
- время преобразования Bus Voltage;
- время преобразования Shunt Voltage;
- режим работы.

Пример:

```c
status = INA226_Configure(&ina, INA226_AVERAGES_64, INA226_CONVERSION_TIME_1_1_MS, INA226_CONVERSION_TIME_1_1_MS, INA226_MODE_SHUNT_BUS_CONTINUOUS);
```

### Усреднение

Доступные значения:

```c
INA226_AVERAGES_1
INA226_AVERAGES_4
INA226_AVERAGES_16
INA226_AVERAGES_64
INA226_AVERAGES_128
INA226_AVERAGES_256
INA226_AVERAGES_512
INA226_AVERAGES_1024
```

Большее усреднение уменьшает шум, но увеличивает период обновления результата.

### Время преобразования

Для Bus Voltage и Shunt Voltage время задаётся независимо:

```c
INA226_CONVERSION_TIME_140_US
INA226_CONVERSION_TIME_204_US
INA226_CONVERSION_TIME_332_US
INA226_CONVERSION_TIME_588_US
INA226_CONVERSION_TIME_1_1_MS
INA226_CONVERSION_TIME_2_116_MS
INA226_CONVERSION_TIME_4_156_MS
INA226_CONVERSION_TIME_8_244_MS
```

При измерении обоих каналов примерный период обновления рассчитывается так:

```text
(Bus conversion time + Shunt conversion time) × Averaging
```

### Режимы работы

```c
INA226_MODE_POWER_DOWN
INA226_MODE_SHUNT_TRIGGERED
INA226_MODE_BUS_TRIGGERED
INA226_MODE_SHUNT_BUS_TRIGGERED
INA226_MODE_POWER_DOWN_ALT
INA226_MODE_SHUNT_CONTINUOUS
INA226_MODE_BUS_CONTINUOUS
INA226_MODE_SHUNT_BUS_CONTINUOUS
```

`INA226_MODE_POWER_DOWN` и `INA226_MODE_POWER_DOWN_ALT` соответствуют двум кодам power-down, предусмотренным INA226.

В режимах измерения только одного канала регистры, зависящие от второго канала, могут содержать предыдущее или неактуальное значение. `INA226_Read()` всё равно читает все четыре измерительных регистра.

### `INA226_SetMode()`

```c
INA226_Status_t INA226_SetMode(INA226_Handle_t *dev, INA226_Mode_t mode);
```

Изменяет только mode bits, сохраняя averaging и времена преобразования.

Запись triggered mode сразу запускает первое одиночное преобразование.

### `INA226_Trigger()`

```c
INA226_Status_t INA226_Trigger(INA226_Handle_t *dev);
```

Повторно записывает текущую конфигурацию и запускает следующее одиночное преобразование. Функция допустима только для:

```c
INA226_MODE_SHUNT_TRIGGERED
INA226_MODE_BUS_TRIGGERED
INA226_MODE_SHUNT_BUS_TRIGGERED
```

Пример triggered mode:

```c
bool ready = false;

status = INA226_SetMode(&ina, INA226_MODE_SHUNT_BUS_TRIGGERED);

if (status != INA226_OK)
{
    Error_Handler();
}

do
{
    status = INA226_IsConversionReady(&ina, &ready);
}
while ((status == INA226_OK) && !ready);

if (status == INA226_OK)
{
    status = INA226_Read(&ina, &data);
}

/* Start the next one-shot conversion later. */
status = INA226_Trigger(&ina);
```

Для реального приложения в цикл ожидания следует добавить собственный программный таймаут.

## ALERT

### Электрическое подключение

ALERT — выход open-drain. Для получения высокого уровня требуется подтягивающий резистор. Можно использовать внешний резистор или внутреннюю подтяжку GPIO STM32, если её параметры подходят конкретной схеме.

Обычно GPIO STM32 настраивается как вход:

```c
GPIO_InitStruct.Pin = ALERT_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(ALERT_GPIO_Port, &GPIO_InitStruct);
```

ALERT также можно подключить к линии EXTI.

### `INA226_ConfigureAlert()`

```c
INA226_Status_t INA226_ConfigureAlert(INA226_Handle_t *dev, INA226_AlertFunction_t function, uint16_t limit_raw, bool conversion_ready, bool active_high, bool latch);
```

Параметры:

- `function` — пороговая функция;
- `limit_raw` — сырое значение Alert Limit Register;
- `conversion_ready` — выводить Conversion Ready на ALERT;
- `active_high` — активная полярность ALERT;
- `latch` — защёлкивать событие до чтения Mask/Enable.

Доступные пороговые функции:

| Значение | Условие |
|---|---|
| `INA226_ALERT_NONE` | Пороговая функция отключена |
| `INA226_ALERT_POWER_OVER` | Мощность выше порога |
| `INA226_ALERT_BUS_UNDER` | Bus Voltage ниже порога |
| `INA226_ALERT_BUS_OVER` | Bus Voltage выше порога |
| `INA226_ALERT_SHUNT_UNDER` | Shunt Voltage ниже порога |
| `INA226_ALERT_SHUNT_OVER` | Shunt Voltage выше порога |

Одновременно может быть выбрана только одна пороговая функция. Conversion Ready разрешается включить вместе с ней.

### Пример: VBUS выше 12 V

```c
uint16_t limit_raw = (uint16_t)(12000.0f / INA226_BUS_LSB_MV);

status = INA226_ConfigureAlert(&ina, INA226_ALERT_BUS_OVER, limit_raw, false, false, true);
```

Здесь:

```text
limit_raw       = 12000 mV / 1.25 mV = 9600 = 0x2580
conversion_ready = false
active_high      = false, активный LOW
latch            = true
```

### Расчёт Alert Limit

Для Bus Voltage:

```c
limit_raw = (uint16_t)(threshold_mV / INA226_BUS_LSB_MV);
```

Для Shunt Voltage порог является знаковым:

```c
int16_t shunt_limit = (int16_t)(threshold_uV / INA226_SHUNT_LSB_UV);
uint16_t limit_raw = (uint16_t)shunt_limit;
```

Для Power:

```c
limit_raw = (uint16_t)(threshold_mW / (ina.power_lsb_W * 1000.0f));
```

Приложение должно самостоятельно проверить, что рассчитанный порог помещается в соответствующий 16-битный регистр.

### Полярность

При `active_high = false`:

- нормальное состояние — HIGH за счёт подтяжки;
- активное состояние — LOW.

При `active_high = true` логика инвертируется.

### Transparent mode

При `latch = false`:

- условие выполняется — ALERT активен;
- условие исчезает — ALERT автоматически возвращается в неактивное состояние.

Читать Mask/Enable для отпускания ALERT не требуется. `INA226_GetAlertStatus()` можно использовать только для получения флагов.

Короткое событие может закончиться до того, как MCU успеет его опросить.

### Latched mode

При `latch = true` INA226 запоминает событие:

- после срабатывания ALERT остаётся активным;
- исчезновение порогового условия само по себе не снимает ALERT;
- чтение Mask/Enable подтверждает событие и сбрасывает аппаратную защёлку;
- если условие продолжает выполняться, ALERT может снова активироваться после следующего преобразования.

Для чтения причины и одновременного сброса защёлки:

```c
INA226_AlertStatus_t alert_status;

status = INA226_GetAlertStatus(&ina, &alert_status);
```

Пример опроса активного LOW:

```c
if (HAL_GPIO_ReadPin(ALERT_GPIO_Port, ALERT_Pin) == GPIO_PIN_RESET)
{
    status = INA226_GetAlertStatus(&ina, &alert_status);

    if (status != INA226_OK)
    {
        Error_Handler();
    }

    if (alert_status.alert_function)
    {
        /* Process the threshold event. */
    }
}
```

Следующие функции читают Mask/Enable и поэтому также подтверждают latched ALERT:

```c
INA226_GetAlertStatus()
INA226_IsConversionReady()
INA226_IsOverflow()
INA226_ReadRegister(..., INA226_REG_MASK_ENABLE, ...)
```

`INA226_Read()` Mask/Enable не читает и защёлку не сбрасывает.

### `INA226_DisableAlert()`

```c
INA226_Status_t INA226_DisableAlert(INA226_Handle_t *dev);
```

Отключает все источники ALERT, устанавливает активный LOW, выключает latch и записывает нулевой Alert Limit.

## Флаги состояния

### `INA226_GetAlertStatus()`

```c
INA226_Status_t INA226_GetAlertStatus(INA226_Handle_t *dev, INA226_AlertStatus_t *alert_status);
```

Одним чтением Mask/Enable получает:

- Alert Function Flag;
- Conversion Ready Flag;
- Math Overflow Flag.

Чтение очищает Conversion Ready Flag и подтверждает latched ALERT.

### `INA226_IsConversionReady()`

```c
bool ready;
status = INA226_IsConversionReady(&ina, &ready);
```

Читает Conversion Ready Flag. Само чтение Mask/Enable очищает CVRF и подтверждает latched ALERT.

В обычном continuous mode ожидать этот флаг обычно не требуется. Он наиболее полезен в triggered mode.

### `INA226_IsOverflow()`

```c
bool overflow;
status = INA226_IsOverflow(&ina, &overflow);
```

Читает Math Overflow Flag. Чтение того же регистра одновременно очищает CVRF и подтверждает latched ALERT.

При `overflow = true` значения Current и Power нельзя считать корректными.

## Reset и DeInit

### `INA226_Reset()`

```c
INA226_Status_t INA226_Reset(INA226_Handle_t *dev);
```

Функция:

1. Помечает handle как неинициализированный.
2. Записывает reset bit в Configuration Register.
3. Читает Configuration Register до самоочистки бита `RST`.
4. Прерывает опрос после `INA226_RESET_POLL_ATTEMPTS` неудачных проверок.
5. Обновляет кешированные значения handle.

Reset сбрасывает регистры INA226 к начальным значениям, включая Calibration Register. Если бит `RST` не очистился, возвращается `INA226_ERROR_TIMEOUT`. Фиксированная задержка и `HAL_Delay()` внутри драйвера не используются. После успешного reset необходимо повторно вызвать `INA226_Init()`.

### `INA226_DeInit()`

```c
INA226_Status_t INA226_DeInit(INA226_Handle_t *dev);
```

Функция выполняет две I2C-записи:

1. Переводит INA226 в Power Down.
2. Отключает все источники ALERT.

Calibration и Alert Limit не обнуляются, потому что в Power Down они не используются. После успешного завершения handle помечается как неинициализированный. Для возобновления работы следует вызвать `INA226_Init()`.

## Несколько INA226

Каждому устройству требуется отдельный handle и отдельная структура результатов:

```c
INA226_Handle_t ina1;
INA226_Handle_t ina2;

INA226_Data_t data1;
INA226_Data_t data2;
```

### Одна I2C-шина, разные адреса

```c
INA226_Init(&ina1, &hi2c1, INA226_ADDR_GND_GND, 0.1f, 0.8f);
INA226_Init(&ina2, &hi2c1, INA226_ADDR_GND_VS, 0.1f, 0.8f);
```

### Разные I2C-шины, одинаковые адреса

```c
INA226_Init(&ina1, &hi2c1, INA226_ADDR_GND_GND, 0.1f, 0.8f);
INA226_Init(&ina2, &hi2c2, INA226_ADDR_GND_GND, 0.1f, 0.8f);
```

Одинаковый адрес допустим, потому что физические I2C-шины независимы.

Общая временная переменная статуса разрешена:

```c
INA226_Status_t status;
```

Драйвер не содержит глобальных переменных состояния устройств.

## Низкоуровневый доступ к регистрам

### `INA226_ReadRegister()`

```c
uint16_t raw;
status = INA226_ReadRegister(&ina, INA226_REG_SHUNT_VOLTAGE, &raw);
```

Возвращает сырое 16-битное значение регистра.

Пример ручного преобразования Shunt Voltage:

```c
int32_t shunt_uV = (int32_t)((int16_t)raw) * 25 / 10;
```

Для вычислений с сохранением шага `2.5 µV` удобнее использовать `float`:

```c
float shunt_uV = (float)((int16_t)raw) * INA226_SHUNT_LSB_UV;
```

Чтение `INA226_REG_MASK_ENABLE` имеет побочный эффект: очищает CVRF и подтверждает latched ALERT.

### `INA226_WriteRegister()`

```c
status = INA226_WriteRegister(&ina, register_address, value);
```

Записывает сырое 16-битное значение MSB first.

При прямой записи Mask/Enable или Alert Limit соответствующее кешированное поле handle обновляется.

Прямая запись Configuration или Calibration может нарушить согласованность аппаратных регистров с параметрами, сохранёнными в handle. После такой записи рекомендуется повторно выполнить `INA226_Init()`.

Низкоуровневые функции предназначены для диагностики и специальных случаев. В обычной программе предпочтительнее использовать высокоуровневый API.

## Обработка ошибок

Простой вариант:

```c
status = INA226_Read(&ina, &data);

if (status != INA226_OK)
{
    Error_Handler();
}
```

В работающем устройстве обычно лучше различать временную ошибку I2C и ошибку конфигурации:

```c
status = INA226_Read(&ina, &data);

switch (status)
{
    case INA226_OK:
        break;

    case INA226_ERROR_I2C:
    case INA226_ERROR_TIMEOUT:
        /* Retry later or recover the I2C peripheral. */
        break;

    default:
        Error_Handler();
        break;
}
```

Все операции являются блокирующими. Вызывать драйвер из обработчика прерывания не рекомендуется.

Если несколько задач или обработчиков используют одну I2C-шину, синхронизацию доступа должно обеспечить приложение.

## Рекомендованный порядок использования

Для основного continuous mode:

1. Настроить GPIO и I2C средствами CubeMX.
2. Вызвать `MX_I2Cx_Init()`.
3. Вызвать `INA226_Init()` один раз.
4. При необходимости вызвать `INA226_Configure()`.
5. При необходимости настроить ALERT.
6. Периодически вызывать `INA226_Read()`.
7. Обрабатывать status flags отдельными функциями только тогда, когда они действительно нужны.

Пример полной последовательности:

```c
INA226_Handle_t ina;
INA226_Data_t data;
INA226_Status_t status;

MX_I2C2_Init();

status = INA226_Init(&ina, &hi2c2, INA226_ADDR_GND_GND, 0.1f, 0.8f);

if (status != INA226_OK)
{
    Error_Handler();
}

status = INA226_ConfigureAlert(&ina, INA226_ALERT_BUS_OVER, (uint16_t)(12000.0f / INA226_BUS_LSB_MV), false, false, false);

if (status != INA226_OK)
{
    Error_Handler();
}

while (1)
{
    status = INA226_Read(&ina, &data);

    if (status == INA226_OK)
    {
        /* Use the measurement values. */
    }

    if (HAL_GPIO_ReadPin(ALERT_GPIO_Port, ALERT_Pin) == GPIO_PIN_RESET)
    {
        /* VBUS is above 12 V in transparent active-low mode. */
    }

    HAL_Delay(100U);
}
```

## Ограничения текущей версии

- API использует `float`; стоимость вычислений зависит от выбранного STM32 и наличия аппаратного FPU.
- Все I2C-операции блокирующие.
- Внутренний таймаут одной HAL I2C-операции равен `100 ms`.
- `INA226_Read()` не проверяет Math Overflow Flag.
- При latched ALERT любое явное чтение Mask/Enable подтверждает событие.
- Автоматическое восстановление I2C после аппаратной ошибки не выполняется.
- Драйвер не управляет GPIO ALERT и не настраивает EXTI.
- Потокобезопасность для общей I2C-шины должна обеспечиваться приложением.

## Регистры INA226

| Адрес | Регистр | Доступ |
|---:|---|---|
| `0x00` | Configuration | Read/Write |
| `0x01` | Shunt Voltage | Read |
| `0x02` | Bus Voltage | Read |
| `0x03` | Power | Read |
| `0x04` | Current | Read |
| `0x05` | Calibration | Read/Write |
| `0x06` | Mask/Enable | Read/Write |
| `0x07` | Alert Limit | Read/Write |
| `0xFE` | Manufacturer ID | Read |
| `0xFF` | Die ID | Read |

## Автор и разработка

Автор и правообладатель: **Yuriy Yaremchuk**.

При разработке использовался OpenAI Codex в качестве ИИ-инструмента для проверки кода, подготовки документации и помощи в реализации. Все технические решения, испытания и окончательная ответственность за проект принадлежат автору.

Драйвер реализован для STM32 HAL на основе документации Texas Instruments INA226. Публичный проект [LibDriver INA226](https://github.com/libdriver/ina226) изучался как источник архитектурных идей и перечня возможностей. LibDriver INA226 также распространяется по лицензии MIT; данный драйвер не требует LibDriver и не содержит его hardware abstraction layer.

## Лицензия

Проект распространяется по лицензии MIT. Полный текст находится в файле [LICENSE](LICENSE).

## Калибровка и события (0.2.0)

После усечения CAL дополнительно ограничивается по консервативной целочисленной границе кода шунтового АЦП. Затем Current LSB пересчитывается как `(0.00512f / CAL) / Rshunt`, Power LSB — в 25 раз больше. Учитываются целый CAL и квантование АЦП; double не используется. Ошибка усиления, offset и шум требуют дополнительного запаса приложения.

Новый запуск преобразования сбрасывает старый программный CVRF. Power Down сохраняет готовность последнего цикла до получения статуса или нового запуска. AFF сохраняется через смену режима до GetAlertStatus или явного отключения ALERT. Reset, успешный DeInit и любая ошибка записи очищают сохранённые события. Неверные параметры Init оставляют прежний handle и аппаратуру без изменений; ошибка I2C в Init оставляет initialized=false. После прямой записи Configuration/Calibration и любой ошибки записи обязателен повторный Init.

Для выбора HAL-заголовка вместо main.h компилируйте ina226.c с `-DINA226_HAL_HEADER='"stm32f4xx_hal.h"'`. До подключения ina226.h приложение должно определить I2C_HandleTypeDef. Из-за добавления conversion_ready_pending в handle требуется пересборка всех потребителей.
