# INA226 Driver for STM32 HAL

Russian documentation: [README_RUS.md](README_RUS.md)

## Overview

This is a small, self-contained INA226 current, voltage, and power monitor driver for STM32CubeIDE projects using STM32 HAL.

The driver is not tied to a specific STM32 family. It requires a compatible STM32 HAL implementation with blocking I2C functions.

Main features:

- multiple INA226 devices with independent handles;
- devices on the same or different hardware I2C peripherals;
- normal 7-bit I2C addresses in the public API;
- Manufacturer ID and Die ID verification during initialization;
- automatic Current LSB, Power LSB, and Calibration Register calculation;
- continuous, triggered, and power-down modes;
- configurable averaging and conversion times;
- ALERT threshold, polarity, Conversion Ready, and latch support;
- raw 16-bit register access;
- no global device state inside the driver.

The driver uses the following blocking STM32 HAL functions:

```c
HAL_I2C_Mem_Read()
HAL_I2C_Mem_Write()
HAL_I2C_IsDeviceReady()
```

The device behavior and register definitions are based on the [Texas Instruments INA226 datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf).

## Files

The repository contains:

```text
ina226.h
ina226.c
example.c
```

Add `ina226.c` and `ina226.h` to the application and include their directory in the compiler search paths. `example.c` is an optional integration example.

The source file includes the headers in this order:

```c
#include "main.h"
#include "ina226.h"
```

`ina226.h` intentionally does not include `main.h` or an STM32 HAL header. `I2C_HandleTypeDef` must already be defined before including `ina226.h`. In a normal STM32CubeIDE application, use:

```c
#include "main.h"
#include "ina226.h"
```

## Minimal Example

Create the objects and initialize the device after the I2C peripheral:

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

Read the latest conversion results periodically:

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

After `INA226_Init()`, the device runs continuously by default. The MCU does not need to start every conversion.

## INA226 Hardware Limits

### Shunt Voltage Range

The INA226 has a fixed shunt voltage range of approximately:

```text
-81.92 mV to +81.92 mV
```

Unlike the INA219, the INA226 does not have a programmable gain amplifier (PGA).

With an `R100` shunt (`0.1 Ω`), the maximum symmetrical current range is approximately:

```text
81.9175 mV / 0.1 Ω = 0.819175 A
```

With a `0.01 Ω` shunt, commonly marked `R010`, the corresponding range is approximately `±8.19175 A`. Always use the actual resistance value from the component specification; markings such as `R10` can be ambiguous.

This calculation describes the INA226 measurement range only. The shunt power rating, PCB traces, connectors, and wiring must be designed for the actual current.

Pass both the physical shunt resistance and the maximum expected current magnitude to the driver:

```c
INA226_Init(&ina, &hi2c2, INA226_ADDR_GND_GND, 0.1f, 0.8f);
```

`max_current_A` sets the Current Register scale and must not exceed the physical limit of `81.9175 mV / Rshunt`. A lower value provides a finer Current LSB, but current beyond the configured range can set OVF. Configure a protection threshold separately through ALERT when required.

### Measurement Resolution

```text
Bus Voltage LSB:    1.25 mV
Shunt Voltage LSB:  2.5 µV
Power LSB:          25 × Current LSB
```

Shunt Voltage and Current are signed values. The driver interprets both registers as `int16_t`.

The Power Register is unsigned.

## I2C Addressing

The public API accepts a normal 7-bit address from `0x40` through `0x4F`. The driver shifts it before passing it to STM32 HAL:

```c
dev->address << 1
```

Do not pass an already shifted address to `INA226_Init()`.

The default address with A1 and A0 connected to GND is:

```c
INA226_ADDR_GND_GND /* 0x40 */
```

All 16 A1/A0 combinations are defined:

| A1 | A0 | Macro | Address |
|---|---|---|---:|
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

## Data Types

### `INA226_Handle_t`

One handle represents one physical INA226:

```c
INA226_Handle_t ina;
```

It stores the selected I2C peripheral, 7-bit address, shunt resistance, maximum expected current, calculated scaling values, cached register values, ALERT state, and initialization state.

Do not modify handle fields manually after successful initialization.

### `INA226_Data_t`

Converted measurement results:

```c
typedef struct
{
    float bus_voltage_mV;
    float shunt_voltage_uV;
    float current_mA;
    float power_mW;
} INA226_Data_t;
```

The unit is part of each field name. Shunt voltage and current are signed.

`INA226_Read()` updates this structure only after all four registers have been read successfully. Existing values remain unchanged after an I2C failure.

### `INA226_AlertStatus_t`

Status flags read from the Mask/Enable Register:

```c
typedef struct
{
    bool alert_function;
    bool conversion_ready;
    bool overflow;
} INA226_AlertStatus_t;
```

- `alert_function`: the configured alert limit was reached;
- `conversion_ready`: a conversion result is ready;
- `overflow`: the internal Current or Power calculation overflowed.

### `INA226_Status_t`

| Value | Meaning |
|---|---|
| `INA226_OK` | Operation completed successfully |
| `INA226_ERROR_PARAM` | Invalid pointer, address, mode, or parameter |
| `INA226_ERROR_I2C` | I2C communication error |
| `INA226_ERROR_TIMEOUT` | HAL timeout or reset did not complete |
| `INA226_ERROR_NOT_FOUND` | Device did not respond or failed its ID check |
| `INA226_ERROR_OVERFLOW` | Math Overflow Flag is set |

`HAL_StatusTypeDef` is not exposed by the public API.

## Initialization

### `INA226_Init()`

```c
INA226_Status_t INA226_Init(INA226_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t address, float shunt_ohm, float max_current_A);
```

Parameters:

- `dev`: handle for this INA226;
- `hi2c`: STM32 HAL I2C handle;
- `address`: 7-bit INA226 address;
- `shunt_ohm`: shunt resistance in ohms;
- `max_current_A`: maximum expected current magnitude in amperes.

The function performs the following operations:

1. Validates pointers and numeric parameters.
2. Validates the `0x40 ... 0x4F` address range.
3. Validates `max_current_A × shunt_ohm` against the positive `81.9175 mV` limit.
4. Calculates the minimum Current LSB as `max_current_A / 32768`.
5. Calculates Power LSB and the Calibration Register, validating its 15-bit range.
6. Probes the device with `HAL_I2C_IsDeviceReady()`.
7. Verifies Manufacturer ID `0x5449`.
8. Verifies Die ID `0x2260`, ignoring revision bits.
9. Performs a software reset.
10. Writes the Configuration Register.
11. Writes the Calibration Register.

Default configuration:

```text
Averaging:               16 samples
Bus conversion time:     1.1 ms
Shunt conversion time:   1.1 ms
Mode:                    Shunt + Bus Continuous
```

The approximate update period is:

```text
(1.1 ms + 1.1 ms) × 16 = 35.2 ms
```

Current scaling follows the datasheet equations:

```text
Minimum Current LSB = Maximum Expected Current / 2^15
Calibration         = 0.00512 / (Current LSB × Rshunt)
Power LSB           = 25 × Current LSB
```

For maximum resolution, the driver uses the minimum Current LSB without upward rounding. For an `R100` shunt and `max_current_A = 0.8 A`:

```text
Current LSB = 24.4140625 µA
Power LSB   = 610.3515625 µW
Calibration = 2097 = 0x0831
```

The requested current is a Current Register scaling range, not a protection threshold, and it does not extend the physical shunt-input range. Both `shunt_ohm` and `max_current_A` must be finite and positive. If the shunt drop exceeds `81.9175 mV` or the calculated calibration does not fit in 15 bits, the function returns `INA226_ERROR_PARAM`.

## Reading Measurements

### `INA226_Read()`

```c
INA226_Status_t INA226_Read(INA226_Handle_t *dev, INA226_Data_t *data);
```

The function reads each of these registers once:

1. Bus Voltage;
2. Shunt Voltage;
3. Current;
4. Power.

It then converts the raw values and updates `INA226_Data_t`.

`INA226_Read()` intentionally does not read the Mask/Enable Register. Therefore it:

- does not clear the Conversion Ready Flag;
- does not acknowledge a latched ALERT;
- does not check the Math Overflow Flag;
- behaves identically with latch enabled or disabled.

Use `INA226_IsOverflow()` or `INA226_GetAlertStatus()` when OVF must be checked.

Because the four registers are read sequentially, with extremely short conversion times some values can theoretically belong to adjacent conversion cycles. The driver does not stop continuous mode or retry the sequence, keeping execution time bounded and predictable.

### Individual Measurement Functions

```c
INA226_Status_t INA226_GetBusVoltage_mV(INA226_Handle_t *dev, float *voltage_mV);
INA226_Status_t INA226_GetShuntVoltage_uV(INA226_Handle_t *dev, float *voltage_uV);
INA226_Status_t INA226_GetCurrent_mA(INA226_Handle_t *dev, float *current_mA);
INA226_Status_t INA226_GetPower_mW(INA226_Handle_t *dev, float *power_mW);
```

`INA226_GetBusVoltage_mV()` and `INA226_GetShuntVoltage_uV()` read only the corresponding measurement register.

With latch disabled, `INA226_GetCurrent_mA()` and `INA226_GetPower_mW()` first read Mask/Enable and reject an active OVF condition. With latch enabled, they skip Mask/Enable to avoid acknowledging the hardware latch. Current and Power are then returned without checking OVF.

When all four values are required, prefer one `INA226_Read()` call instead of four individual calls.

## ADC and Operating Mode Configuration

### `INA226_Configure()`

```c
INA226_Status_t INA226_Configure(INA226_Handle_t *dev, INA226_Averaging_t averaging, INA226_ConversionTime_t bus_time, INA226_ConversionTime_t shunt_time, INA226_Mode_t mode);
```

This function changes averaging, Bus Voltage conversion time, Shunt Voltage conversion time, and operating mode in one write.

Example:

```c
status = INA226_Configure(&ina, INA226_AVERAGES_64, INA226_CONVERSION_TIME_1_1_MS, INA226_CONVERSION_TIME_1_1_MS, INA226_MODE_SHUNT_BUS_CONTINUOUS);
```

### Averaging

Available settings:

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

More averaging reduces noise but increases the result update period.

### Conversion Time

Bus and shunt conversion times are configured independently:

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

When both channels are enabled, the approximate update period is:

```text
(Bus conversion time + Shunt conversion time) × Averaging
```

### Operating Modes

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

`INA226_MODE_POWER_DOWN` and `INA226_MODE_POWER_DOWN_ALT` are the two power-down codes defined by the INA226.

In single-channel modes, registers that depend on the other channel can contain an old or otherwise non-current value. `INA226_Read()` still reads all four measurement registers.

### `INA226_SetMode()`

```c
INA226_Status_t INA226_SetMode(INA226_Handle_t *dev, INA226_Mode_t mode);
```

Changes only the mode bits while preserving averaging and conversion times. Writing a triggered mode starts the first one-shot conversion immediately.

### `INA226_Trigger()`

```c
INA226_Status_t INA226_Trigger(INA226_Handle_t *dev);
```

Rewrites the current configuration to start another one-shot conversion. It is accepted only when the configured mode is one of:

```c
INA226_MODE_SHUNT_TRIGGERED
INA226_MODE_BUS_TRIGGERED
INA226_MODE_SHUNT_BUS_TRIGGERED
```

Example:

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

/* Start another one-shot conversion later. */
status = INA226_Trigger(&ina);
```

Add an application-level timeout to the polling loop in production code.

## ALERT

### Electrical Connection

ALERT is an open-drain output and requires a pull-up resistor. An external resistor is preferred; an STM32 internal pull-up may be used if its characteristics are suitable for the hardware.

A typical GPIO input configuration is:

```c
GPIO_InitStruct.Pin = ALERT_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(ALERT_GPIO_Port, &GPIO_InitStruct);
```

ALERT can also be connected to an EXTI input.

### `INA226_ConfigureAlert()`

```c
INA226_Status_t INA226_ConfigureAlert(INA226_Handle_t *dev, INA226_AlertFunction_t function, uint16_t limit_raw, bool conversion_ready, bool active_high, bool latch);
```

Parameters:

- `function`: threshold function;
- `limit_raw`: raw Alert Limit Register value;
- `conversion_ready`: route Conversion Ready to ALERT;
- `active_high`: select active polarity;
- `latch`: retain the event until Mask/Enable is read.

Available threshold functions:

| Value | Condition |
|---|---|
| `INA226_ALERT_NONE` | Threshold function disabled |
| `INA226_ALERT_POWER_OVER` | Power above the limit |
| `INA226_ALERT_BUS_UNDER` | Bus Voltage below the limit |
| `INA226_ALERT_BUS_OVER` | Bus Voltage above the limit |
| `INA226_ALERT_SHUNT_UNDER` | Shunt Voltage below the limit |
| `INA226_ALERT_SHUNT_OVER` | Shunt Voltage above the limit |

Only one threshold function can be selected at a time. Conversion Ready may be enabled together with that function.

### Example: VBUS Above 12 V

```c
uint16_t limit_raw = (uint16_t)(12000.0f / INA226_BUS_LSB_MV);

status = INA226_ConfigureAlert(&ina, INA226_ALERT_BUS_OVER, limit_raw, false, false, true);
```

This produces:

```text
limit_raw        = 12000 mV / 1.25 mV = 9600 = 0x2580
conversion_ready = false
active_high      = false, active LOW
latch            = true
```

### Alert Limit Calculation

Bus Voltage limit:

```c
limit_raw = (uint16_t)(threshold_mV / INA226_BUS_LSB_MV);
```

Shunt Voltage uses a signed threshold:

```c
int16_t shunt_limit = (int16_t)(threshold_uV / INA226_SHUNT_LSB_UV);
uint16_t limit_raw = (uint16_t)shunt_limit;
```

Power limit:

```c
limit_raw = (uint16_t)(threshold_mW / (ina.power_lsb_W * 1000.0f));
```

The application must verify that the calculated limit fits the corresponding 16-bit register.

### Polarity

With `active_high = false`:

- the idle state is HIGH through the pull-up;
- the active state is LOW.

With `active_high = true`, the logic is inverted.

### Transparent Mode

With `latch = false`:

- ALERT is active while the condition is true;
- ALERT automatically returns to its idle state after the condition clears.

Reading Mask/Enable is not required to release ALERT. `INA226_GetAlertStatus()` is optional and can be used to inspect the flags. A short event can disappear before the MCU polls it.

### Latched Mode

With `latch = true`:

- INA226 remembers the event;
- ALERT remains active after the threshold condition clears;
- reading Mask/Enable acknowledges the event and releases the hardware latch;
- if the condition is still present, ALERT can assert again after the next conversion.

Read the reason and acknowledge the latch with:

```c
INA226_AlertStatus_t alert_status;

status = INA226_GetAlertStatus(&ina, &alert_status);
```

Active-low polling example:

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

All of the following operations read Mask/Enable and therefore acknowledge a latched ALERT:

```c
INA226_GetAlertStatus()
INA226_IsConversionReady()
INA226_IsOverflow()
INA226_ReadRegister(..., INA226_REG_MASK_ENABLE, ...)
```

`INA226_Read()` does not read Mask/Enable and does not acknowledge the latch.

### `INA226_DisableAlert()`

```c
INA226_Status_t INA226_DisableAlert(INA226_Handle_t *dev);
```

Disables all ALERT sources, selects active LOW, disables latch mode, and writes zero to the Alert Limit Register.

## Status Flags

### `INA226_GetAlertStatus()`

```c
INA226_Status_t INA226_GetAlertStatus(INA226_Handle_t *dev, INA226_AlertStatus_t *alert_status);
```

Reads Alert Function Flag, Conversion Ready Flag, and Math Overflow Flag in one transaction. Reading Mask/Enable clears CVRF and acknowledges a latched ALERT.

The driver keeps an ALERT event in `alert_function_pending` if another status-reading API consumed AFF before `INA226_GetAlertStatus()` was called.

### `INA226_IsConversionReady()`

```c
bool ready;
status = INA226_IsConversionReady(&ina, &ready);
```

Reads the Conversion Ready Flag. Reading Mask/Enable clears CVRF and acknowledges a latched ALERT.

Polling this flag is normally unnecessary in continuous mode. It is primarily useful in triggered mode.

### `INA226_IsOverflow()`

```c
bool overflow;
status = INA226_IsOverflow(&ina, &overflow);
```

Reads the Math Overflow Flag. The same Mask/Enable read also clears CVRF and acknowledges a latched ALERT.

When `overflow` is true, Current and Power must not be treated as valid measurements.

## Reset and Deinitialization

### `INA226_Reset()`

```c
INA226_Status_t INA226_Reset(INA226_Handle_t *dev);
```

The function:

1. marks the handle as uninitialized;
2. writes the reset bit to the Configuration Register;
3. reads the Configuration Register until the self-clearing `RST` bit is clear;
4. stops polling after `INA226_RESET_POLL_ATTEMPTS` unsuccessful checks;
5. updates cached handle values.

Reset restores INA226 registers to their default values, including clearing the Calibration Register. If the `RST` bit does not clear, the function returns `INA226_ERROR_TIMEOUT`. The driver does not use a fixed delay or call `HAL_Delay()` internally. Call `INA226_Init()` before using high-level measurement functions again.

### `INA226_DeInit()`

```c
INA226_Status_t INA226_DeInit(INA226_Handle_t *dev);
```

The function performs two I2C writes:

1. places INA226 in power-down mode;
2. disables all ALERT sources.

Calibration and Alert Limit are preserved because they have no effect in power-down mode. The handle is marked uninitialized only after both writes succeed. Call `INA226_Init()` to resume operation.

## Multiple INA226 Devices

Every physical device needs its own handle and data structure:

```c
INA226_Handle_t ina1;
INA226_Handle_t ina2;

INA226_Data_t data1;
INA226_Data_t data2;
```

Same bus, different addresses:

```c
INA226_Init(&ina1, &hi2c1, INA226_ADDR_GND_GND, 0.1f, 0.8f);
INA226_Init(&ina2, &hi2c1, INA226_ADDR_GND_VS, 0.1f, 0.8f);
```

Different buses, identical addresses:

```c
INA226_Init(&ina1, &hi2c1, INA226_ADDR_GND_GND, 0.1f, 0.8f);
INA226_Init(&ina2, &hi2c2, INA226_ADDR_GND_GND, 0.1f, 0.8f);
```

Identical addresses are valid because the physical buses are independent. A temporary status variable can be shared:

```c
INA226_Status_t status;
```

The driver contains no global device state.

## Raw Register Access

### `INA226_ReadRegister()`

```c
uint16_t raw;
status = INA226_ReadRegister(&ina, INA226_REG_SHUNT_VOLTAGE, &raw);
```

The function returns the raw 16-bit register value.

Convert Shunt Voltage while preserving its signed value and `2.5 µV` step:

```c
float shunt_uV = (float)((int16_t)raw) * INA226_SHUNT_LSB_UV;
```

Reading `INA226_REG_MASK_ENABLE` clears CVRF and acknowledges a latched ALERT.

### `INA226_WriteRegister()`

```c
status = INA226_WriteRegister(&ina, register_address, value);
```

Writes one raw 16-bit value MSB first. Direct writes to Mask/Enable and Alert Limit update the corresponding cached handle values.

Writing Configuration or Calibration directly can make the hardware state inconsistent with scaling and configuration values cached in the handle. Run `INA226_Init()` again before using high-level measurement functions after such a write.

Raw register functions are intended for diagnostics and special cases. Prefer the high-level API in normal application code.

## Error Handling

Minimal handling:

```c
status = INA226_Read(&ina, &data);

if (status != INA226_OK)
{
    Error_Handler();
}
```

A real application can distinguish recoverable I2C failures from configuration errors:

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

All driver operations are blocking. Calling the driver from an interrupt handler is not recommended.

If multiple tasks or handlers use the same I2C peripheral, the application must serialize access.

## Recommended Usage Sequence

For the normal continuous mode:

1. Configure GPIO and I2C in CubeMX.
2. Call `MX_I2Cx_Init()`.
3. Call `INA226_Init()` once.
4. Optionally call `INA226_Configure()`.
5. Optionally configure ALERT.
6. Call `INA226_Read()` periodically.
7. Read status flags separately only when required.

Complete example using a transparent active-low 12 V bus alert:

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
        /* VBUS is above 12 V. */
    }

    HAL_Delay(100U);
}
```

## Current Limitations

- The API uses `float`; execution cost depends on the selected STM32 and whether it has a hardware FPU.
- All I2C operations are blocking.
- The HAL timeout for one I2C operation is `100 ms`.
- `INA226_Read()` does not check the Math Overflow Flag.
- Any explicit Mask/Enable read acknowledges a latched ALERT.
- The driver does not automatically recover a failed I2C peripheral.
- The driver does not configure the ALERT GPIO or EXTI.
- The application must provide synchronization when sharing an I2C peripheral.

## INA226 Registers

| Address | Register | Access |
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

## Author and Development

Author and copyright holder: **Yuriy Yaremchuk**.

OpenAI Codex was used as an AI-assisted development tool for code review, documentation, and implementation support. All technical decisions, testing, and final project responsibility remain with the author.

The driver was implemented for STM32 HAL from the Texas Instruments INA226 datasheet. The public [LibDriver INA226 project](https://github.com/libdriver/ina226) was reviewed as an architectural and feature reference. LibDriver INA226 is also distributed under the MIT License; this driver does not require LibDriver and does not include its hardware abstraction layer.

## License

This project is distributed under the MIT License. See [LICENSE](LICENSE).
