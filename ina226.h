#ifndef INA226_H
#define INA226_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>


/* INA226 7-bit I2C addresses selected by A1 and A0. */

#define INA226_ADDR_GND_GND     0x40U
#define INA226_ADDR_GND_VS      0x41U
#define INA226_ADDR_GND_SDA     0x42U
#define INA226_ADDR_GND_SCL     0x43U

#define INA226_ADDR_VS_GND      0x44U
#define INA226_ADDR_VS_VS       0x45U
#define INA226_ADDR_VS_SDA      0x46U
#define INA226_ADDR_VS_SCL      0x47U

#define INA226_ADDR_SDA_GND     0x48U
#define INA226_ADDR_SDA_VS      0x49U
#define INA226_ADDR_SDA_SDA     0x4AU
#define INA226_ADDR_SDA_SCL     0x4BU

#define INA226_ADDR_SCL_GND     0x4CU
#define INA226_ADDR_SCL_VS      0x4DU
#define INA226_ADDR_SCL_SDA     0x4EU
#define INA226_ADDR_SCL_SCL     0x4FU


/* INA226 registers. */

#define INA226_REG_CONFIG           0x00U
#define INA226_REG_SHUNT_VOLTAGE    0x01U
#define INA226_REG_BUS_VOLTAGE      0x02U
#define INA226_REG_POWER            0x03U
#define INA226_REG_CURRENT          0x04U
#define INA226_REG_CALIBRATION      0x05U
#define INA226_REG_MASK_ENABLE      0x06U
#define INA226_REG_ALERT_LIMIT      0x07U
#define INA226_REG_MANUFACTURER_ID  0xFEU
#define INA226_REG_DIE_ID           0xFFU


/* Configuration register fields. */

#define INA226_CONFIG_RESET         0x8000U
#define INA226_CONFIG_DEFAULT       0x4127U
#define INA226_CONFIG_RESERVED      0x4000U
#define INA226_CONFIG_AVG_MASK      0x0E00U
#define INA226_CONFIG_VBUSCT_MASK   0x01C0U
#define INA226_CONFIG_VSHCT_MASK    0x0038U
#define INA226_CONFIG_AVG_16        0x0400U
#define INA226_CONFIG_VBUSCT_1_1MS  0x0100U
#define INA226_CONFIG_VSHCT_1_1MS   0x0020U
#define INA226_CONFIG_MODE_MASK     0x0007U
#define INA226_CONFIG_MODE_CONT     0x0007U
#define INA226_CONFIG_AVG_SHIFT     9U
#define INA226_CONFIG_VBUSCT_SHIFT  6U
#define INA226_CONFIG_VSHCT_SHIFT   3U


/* Mask/Enable register status bits. */

#define INA226_MASK_ALERT_FUNCTION  0xF800U
#define INA226_MASK_CNVR            0x0400U
#define INA226_MASK_AFF             0x0010U
#define INA226_MASK_CVRF            0x0008U
#define INA226_MASK_OVF             0x0004U
#define INA226_MASK_APOL            0x0002U
#define INA226_MASK_LEN             0x0001U
#define INA226_MASK_WRITABLE        0xFC03U


/* Identification values. */

#define INA226_MANUFACTURER_ID      0x5449U
#define INA226_DIE_ID_MASK          0xFFF0U
#define INA226_DIE_ID_VALUE         0x2260U


/* Measurement and calibration constants. */

#define INA226_I2C_TIMEOUT_MS       100U
#define INA226_RESET_POLL_ATTEMPTS  10U
#define INA226_CURRENT_REG_COUNTS   32767.0f
#define INA226_CALIBRATION_MAX      0x7FFFU
#define INA226_CALIBRATION_CONSTANT 0.00512f
#define INA226_SHUNT_RANGE_V        0.0819175f
#define INA226_SHUNT_LSB_UV         2.5f
#define INA226_BUS_LSB_MV           1.25f
#define INA226_POWER_LSB_MULTIPLIER 25.0f


/** @brief Driver operation result. */
typedef enum
{
    INA226_OK = 0,

    INA226_ERROR_PARAM,
    INA226_ERROR_I2C,
    INA226_ERROR_TIMEOUT,
    INA226_ERROR_NOT_FOUND,
    INA226_ERROR_OVERFLOW

} INA226_Status_t;


/** @brief Number of ADC samples averaged into each reported value. */
typedef enum
{
    INA226_AVERAGES_1 = 0,
    INA226_AVERAGES_4,
    INA226_AVERAGES_16,
    INA226_AVERAGES_64,
    INA226_AVERAGES_128,
    INA226_AVERAGES_256,
    INA226_AVERAGES_512,
    INA226_AVERAGES_1024

} INA226_Averaging_t;


/** @brief ADC conversion time for the bus or shunt channel. */
typedef enum
{
    INA226_CONVERSION_TIME_140_US = 0,
    INA226_CONVERSION_TIME_204_US,
    INA226_CONVERSION_TIME_332_US,
    INA226_CONVERSION_TIME_588_US,
    INA226_CONVERSION_TIME_1_1_MS,
    INA226_CONVERSION_TIME_2_116_MS,
    INA226_CONVERSION_TIME_4_156_MS,
    INA226_CONVERSION_TIME_8_244_MS

} INA226_ConversionTime_t;


/** @brief INA226 power-down, triggered, and continuous operating modes. */
typedef enum
{
    INA226_MODE_POWER_DOWN = 0,
    INA226_MODE_SHUNT_TRIGGERED,
    INA226_MODE_BUS_TRIGGERED,
    INA226_MODE_SHUNT_BUS_TRIGGERED,
    INA226_MODE_POWER_DOWN_ALT,
    INA226_MODE_SHUNT_CONTINUOUS,
    INA226_MODE_BUS_CONTINUOUS,
    INA226_MODE_SHUNT_BUS_CONTINUOUS

} INA226_Mode_t;


/** @brief Measurement condition that can drive the ALERT pin. */
typedef enum
{
    INA226_ALERT_NONE = 0x0000U,
    INA226_ALERT_POWER_OVER = 0x0800U,
    INA226_ALERT_BUS_UNDER = 0x1000U,
    INA226_ALERT_BUS_OVER = 0x2000U,
    INA226_ALERT_SHUNT_UNDER = 0x4000U,
    INA226_ALERT_SHUNT_OVER = 0x8000U

} INA226_AlertFunction_t;


/** @brief Status flags read from the Mask/Enable register. */
typedef struct
{
    bool alert_function;       /**< Configured alert limit was reached. */
    bool conversion_ready;     /**< A conversion result is ready. */
    bool overflow;             /**< Current or power calculation overflowed. */

} INA226_AlertStatus_t;


/** @brief Converted bus voltage, shunt voltage, current, and power values. */
typedef struct
{
    float bus_voltage_mV;      /**< Bus voltage in millivolts. */
    float shunt_voltage_uV;    /**< Signed shunt voltage in microvolts. */

    float current_mA;          /**< Signed current in milliamperes. */
    float power_mW;            /**< Power in milliwatts. */

} INA226_Data_t;


/** @brief Per-device state, interface selection, and calibration data. */
typedef struct
{
    I2C_HandleTypeDef *hi2c;   /**< STM32 HAL I2C interface used by this device. */

    uint8_t address;           /**< 7-bit I2C address. */

    float shunt_ohm;           /**< Shunt resistance in ohms. */
    float max_current_A;       /**< Maximum expected current magnitude in amperes. */

    float current_lsb_A;       /**< Current register LSB in amperes. */
    float power_lsb_W;         /**< Power register LSB in watts. */

    uint16_t calibration;      /**< Cached Calibration register value. */
    uint16_t configuration;    /**< Cached Configuration register value. */
    uint16_t mask_enable;      /**< Cached writable Mask/Enable bits. */
    uint16_t alert_limit;      /**< Cached raw Alert Limit register value. */

    bool alert_function_pending; /**< ALERT event preserved until status read. */
    bool conversion_ready_pending; /**< Conversion event preserved until consumed. */
    bool initialized;          /**< Device passed initialization successfully. */

} INA226_Handle_t;


/**
 * @brief Initialize INA226 for continuous shunt and bus measurements.
 *
 * The driver verifies the manufacturer and die IDs, checks the requested
 * current against the fixed +/-81.92 mV shunt range, calculates Current_LSB,
 * Power_LSB, and Calibration, then configures continuous measurements.
 *
 * @param dev            INA226 device handle.
 * @param hi2c           STM32 HAL I2C handle.
 * @param address        7-bit INA226 I2C address (0x40 ... 0x4F).
 * @param shunt_ohm      Shunt resistance in ohms.
 * @param max_current_A  Maximum expected current magnitude in amperes.
 *
 * Parameter validation failure leaves the handle and hardware unchanged.
 * Once I2C initialization starts, any failure leaves initialized false.
 * Starting a new conversion discards pending CVRF from the previous cycle;
 * pending AFF survives mode changes until explicitly consumed or disabled.
 */
INA226_Status_t INA226_Init(INA226_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t address, float shunt_ohm, float max_current_A);


/**
 * @brief Software reset INA226.
 *
 * A successful reset clears calibration and marks the handle as not
 * initialized. Call INA226_Init() before reading measurements again.
 */
INA226_Status_t INA226_Reset(INA226_Handle_t *dev);


/**
 * @brief Deinitialize INA226 and place it in power-down mode.
 *
 * The function disables conversions and ALERT sources, then marks the handle
 * as not initialized. Call INA226_Init() to use it again.
 */
INA226_Status_t INA226_DeInit(INA226_Handle_t *dev);


/**
 * @brief Configure averaging, conversion times, and operating mode.
 *
 * Selecting a triggered mode starts one conversion immediately.
 */
INA226_Status_t INA226_Configure(INA226_Handle_t *dev, INA226_Averaging_t averaging, INA226_ConversionTime_t bus_time, INA226_ConversionTime_t shunt_time, INA226_Mode_t mode);


/**
 * @brief Change only the operating mode.
 *
 * Selecting a triggered mode starts one conversion immediately.
 */
INA226_Status_t INA226_SetMode(INA226_Handle_t *dev, INA226_Mode_t mode);


/**
 * @brief Start another conversion using the configured triggered mode.
 */
INA226_Status_t INA226_Trigger(INA226_Handle_t *dev);


/**
 * @brief Configure the ALERT pin and its raw comparison limit.
 *
 * Only one limit function can be selected. Conversion Ready can be enabled
 * together with a limit function. For shunt alerts, limit_raw is signed
 * two's-complement with a 2.5 uV LSB. For bus alerts it is unsigned with a
 * 1.25 mV LSB. For power alerts it is unsigned with Power_LSB scaling.
 */
INA226_Status_t INA226_ConfigureAlert(INA226_Handle_t *dev, INA226_AlertFunction_t function, uint16_t limit_raw, bool conversion_ready, bool active_high, bool latch);


/**
 * @brief Disable all ALERT pin sources and clear the alert limit.
 */
INA226_Status_t INA226_DisableAlert(INA226_Handle_t *dev);


/**
 * @brief Read ALERT, Conversion Ready, and Math Overflow flags.
 *
 * Reading Mask/Enable clears CVRF and a latched AFF flag. The driver preserves
 * AFF events consumed by other API calls until this function reads them.
 */
INA226_Status_t INA226_GetAlertStatus(INA226_Handle_t *dev, INA226_AlertStatus_t *alert_status);


/**
 * @brief Read bus voltage, shunt voltage, current, and power.
 *
 * Each measurement register is read once. Mask/Enable is not read, so this
 * function does not clear status flags or acknowledge a latched ALERT. Use the
 * dedicated status functions when CNVR, OVF, or ALERT status is required.
 */
INA226_Status_t INA226_Read(INA226_Handle_t *dev, INA226_Data_t *data);


/**
 * @brief Read bus voltage in millivolts.
 */
INA226_Status_t INA226_GetBusVoltage_mV(INA226_Handle_t *dev, float *voltage_mV);


/**
 * @brief Read signed shunt voltage in microvolts.
 */
INA226_Status_t INA226_GetShuntVoltage_uV(INA226_Handle_t *dev, float *voltage_uV);


/**
 * @brief Read signed current in milliamperes.
 *
 * Checks OVF regardless of LEN. Acknowledges the hardware latch while
 * preserving AFF and CVRF in software.
 */
INA226_Status_t INA226_GetCurrent_mA(INA226_Handle_t *dev, float *current_mA);


/**
 * @brief Read power in milliwatts.
 *
 * Checks OVF regardless of LEN. Acknowledges the hardware latch while
 * preserving AFF and CVRF in software.
 */
INA226_Status_t INA226_GetPower_mW(INA226_Handle_t *dev, float *power_mW);


/**
 * @brief Read the Conversion Ready Flag.
 *
 * Reading Mask/Enable clears CVRF and acknowledges a latched ALERT.
 */
INA226_Status_t INA226_IsConversionReady(INA226_Handle_t *dev, bool *ready);


/**
 * @brief Read the Math Overflow Flag.
 *
 * Reading Mask/Enable clears CVRF and acknowledges a latched ALERT.
 */
INA226_Status_t INA226_IsOverflow(INA226_Handle_t *dev, bool *overflow);


/**
 * @brief Read a raw 16-bit INA226 register.
 *
 * Reading INA226_REG_MASK_ENABLE acknowledges a latched ALERT.
 */
INA226_Status_t INA226_ReadRegister(INA226_Handle_t *dev, uint8_t reg, uint16_t *value);


/**
 * @brief Write a raw 16-bit INA226 register.
 *
 * Writing Configuration or Calibration invalidates initialization. Any failed
 * write also invalidates initialization because the hardware may have accepted
 * data. Reinitialize before using high-level operations again.
 */
INA226_Status_t INA226_WriteRegister(INA226_Handle_t *dev, uint8_t reg, uint16_t value);


#ifdef __cplusplus
}
#endif

#endif /* INA226_H */
