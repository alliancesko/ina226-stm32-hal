#ifdef INA226_HAL_HEADER
#include INA226_HAL_HEADER
#else
#include "main.h"
#endif
#include "ina226.h"

#include <math.h>
#include <float.h>


/** @brief Check the device handle and its basic addressing parameters. */
static bool INA226_IsHandleValid(const INA226_Handle_t *dev)
{
    return (dev != NULL) && (dev->hi2c != NULL) && (dev->address >= 0x40U) && (dev->address <= 0x4FU);
}


/** @brief Check whether an ALERT function selector is supported. */
static bool INA226_IsAlertFunctionValid(INA226_AlertFunction_t function)
{
    return (function == INA226_ALERT_NONE) || (function == INA226_ALERT_POWER_OVER) || (function == INA226_ALERT_BUS_UNDER) || (function == INA226_ALERT_BUS_OVER) || (function == INA226_ALERT_SHUNT_UNDER) || (function == INA226_ALERT_SHUNT_OVER);
}


/** @brief Convert an STM32 HAL result into an INA226 driver status. */
static INA226_Status_t INA226_FromHALStatus(I2C_HandleTypeDef *hi2c, HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
        return INA226_OK;

    /* Some STM32 HAL implementations report blocking timeouts as HAL_ERROR. */
    if ((status == HAL_TIMEOUT) || ((status == HAL_ERROR) && ((HAL_I2C_GetError(hi2c) & HAL_I2C_ERROR_TIMEOUT) != 0U)))
    {
        return INA226_ERROR_TIMEOUT;
    }

    return INA226_ERROR_I2C;
}


/** @brief Write one 16-bit register in INA226 big-endian byte order. */
static INA226_Status_t INA226_Write16(INA226_Handle_t *dev, uint8_t reg, uint16_t value)
{
    uint8_t data[2];
    HAL_StatusTypeDef hal_status;

    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)(value & 0xFFU);

    hal_status = HAL_I2C_Mem_Write(dev->hi2c, (uint16_t)(dev->address << 1), reg, I2C_MEMADD_SIZE_8BIT, data, 2U, INA226_I2C_TIMEOUT_MS);

    /* An error does not prove that the device rejected the write. */
    if (hal_status != HAL_OK)
    {
        dev->initialized = false;
        dev->conversion_ready_pending = false;
        dev->alert_function_pending = false;
    }
    else if ((reg == INA226_REG_CONFIG) && ((value & INA226_CONFIG_MODE_MASK) != 0U) && ((value & INA226_CONFIG_MODE_MASK) != 4U))
    {
        /* A new conversion must not inherit completion of the old cycle. */
        dev->conversion_ready_pending = false;
    }

    return INA226_FromHALStatus(dev->hi2c, hal_status);
}


/** @brief Read one 16-bit register in INA226 big-endian byte order. */
static INA226_Status_t INA226_Read16(INA226_Handle_t *dev, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];
    HAL_StatusTypeDef hal_status;
    INA226_Status_t status;

    hal_status = HAL_I2C_Mem_Read(dev->hi2c, (uint16_t)(dev->address << 1), reg, I2C_MEMADD_SIZE_8BIT, data, 2U, INA226_I2C_TIMEOUT_MS);

    status = INA226_FromHALStatus(dev->hi2c, hal_status);

    if (status != INA226_OK)
        return status;

    *value = (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);

    if (reg == INA226_REG_MASK_ENABLE)
    {
        dev->mask_enable = *value & INA226_MASK_WRITABLE;
        dev->alert_function_pending |= ((*value & INA226_MASK_AFF) != 0U);
        dev->conversion_ready_pending |= ((*value & INA226_MASK_CVRF) != 0U);
    }

    return INA226_OK;
}


/** @brief Read status flags and preserve an ALERT event in software. */
static INA226_Status_t INA226_ReadFlags(INA226_Handle_t *dev, bool *conversion_ready, bool *overflow)
{
    uint16_t raw;
    INA226_Status_t status;

    status = INA226_Read16(dev, INA226_REG_MASK_ENABLE, &raw);

    if (status != INA226_OK)
        return status;

    if ((raw & INA226_MASK_AFF) != 0U)
        dev->alert_function_pending = true;

    if (conversion_ready != NULL)
    {
        *conversion_ready = dev->conversion_ready_pending;
        dev->conversion_ready_pending = false;
    }

    if (overflow != NULL)
        *overflow = ((raw & INA226_MASK_OVF) != 0U);

    return INA226_OK;
}


/** @brief Read a raw 16-bit INA226 register. */
INA226_Status_t INA226_ReadRegister(INA226_Handle_t *dev, uint8_t reg, uint16_t *value)
{
    if (!INA226_IsHandleValid(dev) || (value == NULL))
        return INA226_ERROR_PARAM;

    return INA226_Read16(dev, reg, value);
}


/** @brief Write a raw 16-bit INA226 register and update relevant cached values. */
INA226_Status_t INA226_WriteRegister(INA226_Handle_t *dev, uint8_t reg, uint16_t value)
{
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev))
        return INA226_ERROR_PARAM;

    status = INA226_Write16(dev, reg, value);

    if (status != INA226_OK)
        return status;

    if (reg == INA226_REG_MASK_ENABLE)
        dev->mask_enable = value & INA226_MASK_WRITABLE;
    else if (reg == INA226_REG_ALERT_LIMIT)
        dev->alert_limit = value;
    else if ((reg == INA226_REG_CONFIG) || (reg == INA226_REG_CALIBRATION))
        dev->initialized = false;

    return INA226_OK;
}


/** @brief Reset INA226 and invalidate its initialized driver state. */
INA226_Status_t INA226_Reset(INA226_Handle_t *dev)
{
    uint16_t configuration = INA226_CONFIG_RESET;
    unsigned int attempt;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev))
        return INA226_ERROR_PARAM;

    dev->initialized = false;

    dev->conversion_ready_pending = false;
    dev->alert_function_pending = false;
    status = INA226_Write16(dev, INA226_REG_CONFIG, INA226_CONFIG_RESET);

    if (status != INA226_OK)
        return status;

    for (attempt = 0U; attempt < INA226_RESET_POLL_ATTEMPTS; attempt++)
    {
        status = INA226_Read16(dev, INA226_REG_CONFIG, &configuration);

        if (status != INA226_OK)
            return status;

        if ((configuration & INA226_CONFIG_RESET) == 0U)
            break;
    }

    if ((configuration & INA226_CONFIG_RESET) != 0U)
        return INA226_ERROR_TIMEOUT;

    dev->calibration = 0U;
    dev->configuration = INA226_CONFIG_DEFAULT;
    dev->mask_enable = 0U;
    dev->alert_limit = 0U;
    dev->alert_function_pending = false;
    dev->conversion_ready_pending = false;

    return INA226_OK;
}


/** @brief Place INA226 in power-down mode and disable ALERT sources. */
INA226_Status_t INA226_DeInit(INA226_Handle_t *dev)
{
    uint16_t power_down_configuration;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || !dev->initialized)
        return INA226_ERROR_PARAM;

    power_down_configuration = dev->configuration & (uint16_t)(~INA226_CONFIG_MODE_MASK);

    status = INA226_Write16(dev, INA226_REG_CONFIG, power_down_configuration);

    if (status != INA226_OK)
        return status;

    dev->configuration = power_down_configuration;

    status = INA226_Write16(dev, INA226_REG_MASK_ENABLE, 0U);

    if (status != INA226_OK)
        return status;

    dev->mask_enable = 0U;
    dev->alert_function_pending = false;
    dev->conversion_ready_pending = false;
    dev->initialized = false;

    return INA226_OK;
}


/** @brief Configure averaging, conversion times, and operating mode. */
INA226_Status_t INA226_Configure(INA226_Handle_t *dev, INA226_Averaging_t averaging, INA226_ConversionTime_t bus_time, INA226_ConversionTime_t shunt_time, INA226_Mode_t mode)
{
    uint16_t configuration;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || !dev->initialized || ((uint32_t)averaging > 7U) || ((uint32_t)bus_time > 7U) || ((uint32_t)shunt_time > 7U) || ((uint32_t)mode > 7U))
        return INA226_ERROR_PARAM;

    configuration = (uint16_t)(INA226_CONFIG_RESERVED | ((uint32_t)averaging << INA226_CONFIG_AVG_SHIFT) | ((uint32_t)bus_time << INA226_CONFIG_VBUSCT_SHIFT) | ((uint32_t)shunt_time << INA226_CONFIG_VSHCT_SHIFT) | (uint32_t)mode);
    status = INA226_Write16(dev, INA226_REG_CONFIG, configuration);

    if (status != INA226_OK)
        return status;

    dev->configuration = configuration;

    return INA226_OK;
}


/** @brief Change the operating mode without modifying ADC timing. */
INA226_Status_t INA226_SetMode(INA226_Handle_t *dev, INA226_Mode_t mode)
{
    uint16_t configuration;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || !dev->initialized || ((uint32_t)mode > 7U))
        return INA226_ERROR_PARAM;

    configuration = (dev->configuration & (uint16_t)(~INA226_CONFIG_MODE_MASK)) | (uint16_t)mode;
    status = INA226_Write16(dev, INA226_REG_CONFIG, configuration);

    if (status != INA226_OK)
        return status;

    dev->configuration = configuration;

    return INA226_OK;
}


/** @brief Start another measurement in the configured triggered mode. */
INA226_Status_t INA226_Trigger(INA226_Handle_t *dev)
{
    uint16_t mode;

    if (!INA226_IsHandleValid(dev) || !dev->initialized)
        return INA226_ERROR_PARAM;

    mode = dev->configuration & INA226_CONFIG_MODE_MASK;

    if ((mode < (uint16_t)INA226_MODE_SHUNT_TRIGGERED) || (mode > (uint16_t)INA226_MODE_SHUNT_BUS_TRIGGERED))
        return INA226_ERROR_PARAM;

    return INA226_Write16(dev, INA226_REG_CONFIG, dev->configuration);
}


/** @brief Configure the ALERT source, raw limit, polarity, and latch mode. */
INA226_Status_t INA226_ConfigureAlert(INA226_Handle_t *dev, INA226_AlertFunction_t function, uint16_t limit_raw, bool conversion_ready, bool active_high, bool latch)
{
    uint16_t mask_enable;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || !dev->initialized || !INA226_IsAlertFunctionValid(function))
        return INA226_ERROR_PARAM;

    mask_enable = (uint16_t)function;

    if (conversion_ready)
        mask_enable |= INA226_MASK_CNVR;

    if (active_high)
        mask_enable |= INA226_MASK_APOL;

    if (latch)
        mask_enable |= INA226_MASK_LEN;

    status = INA226_Write16(dev, INA226_REG_ALERT_LIMIT, limit_raw);

    if (status != INA226_OK)
        return status;

    dev->alert_limit = limit_raw;
    status = INA226_Write16(dev, INA226_REG_MASK_ENABLE, mask_enable);

    if (status != INA226_OK)
        return status;

    dev->mask_enable = mask_enable;

    if (function == INA226_ALERT_NONE)
        dev->alert_function_pending = false;

    return INA226_OK;
}


/** @brief Disable every ALERT source and clear the alert limit. */
INA226_Status_t INA226_DisableAlert(INA226_Handle_t *dev)
{
    return INA226_ConfigureAlert(dev, INA226_ALERT_NONE, 0U, false, false, false);
}


/** @brief Read ALERT status and acknowledge a latched event. */
INA226_Status_t INA226_GetAlertStatus(INA226_Handle_t *dev, INA226_AlertStatus_t *alert_status)
{
    uint16_t raw;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || !dev->initialized || (alert_status == NULL))
        return INA226_ERROR_PARAM;

    status = INA226_Read16(dev, INA226_REG_MASK_ENABLE, &raw);

    if (status != INA226_OK)
        return status;

    dev->mask_enable = raw & INA226_MASK_WRITABLE;
    alert_status->alert_function = ((raw & INA226_MASK_AFF) != 0U) || dev->alert_function_pending;
    alert_status->conversion_ready = dev->conversion_ready_pending;
    alert_status->overflow = ((raw & INA226_MASK_OVF) != 0U);
    dev->alert_function_pending = false;
    dev->conversion_ready_pending = false;

    return INA226_OK;
}


/** @brief Verify, calibrate, and initialize one INA226 device. */
INA226_Status_t INA226_Init(INA226_Handle_t *dev, I2C_HandleTypeDef *hi2c, uint8_t address, float shunt_ohm, float max_current_A)
{
    float current_lsb;
    float calibration_value;
    float shunt_voltage;
    uint16_t calibration;
    uint32_t shunt_code_bound;
    uint32_t calibration_bound;
    uint16_t configuration;
    uint16_t manufacturer_id;
    uint16_t die_id;
    HAL_StatusTypeDef hal_status;
    INA226_Status_t status;

    if ((dev == NULL) || (hi2c == NULL) || (!isfinite(shunt_ohm)) || (!isfinite(max_current_A)) || (shunt_ohm <= 0.0f) || (max_current_A <= 0.0f))
    {
        return INA226_ERROR_PARAM;
    }

    if ((address < 0x40U) || (address > 0x4FU))
        return INA226_ERROR_PARAM;

    shunt_voltage = max_current_A * shunt_ohm;

    if ((!isfinite(shunt_voltage)) || (shunt_voltage <= 0.0f) || (shunt_voltage > INA226_SHUNT_RANGE_V))
    {
        return INA226_ERROR_PARAM;
    }

    /* This form avoids an intermediate subnormal Current_LSB * Rshunt. */
    calibration_value = (INA226_CALIBRATION_CONSTANT * INA226_CURRENT_REG_COUNTS) / shunt_voltage;

    if ((!isfinite(calibration_value)) || (calibration_value < 1.0f) || (calibration_value >= (float)INA226_CALIBRATION_MAX + 1.0f))
    {
        return INA226_ERROR_PARAM;
    }

    calibration = (uint16_t)calibration_value;
    /* Bound ADC rounding as well as float rounding, then use exact integers.
     * One whole shunt count exceeds half-count ADC quantization plus the
     * binary32 product/division error throughout the accepted input range.
     */
    shunt_code_bound = (uint32_t)(shunt_voltage / 0.0000025f) + 1U;
    calibration_bound = (32767U * 2048U) / shunt_code_bound;
    if ((uint32_t)calibration > calibration_bound)
        calibration = (uint16_t)calibration_bound;

    /* Match software scaling to the integer actually programmed into CAL. */
    current_lsb = (INA226_CALIBRATION_CONSTANT / (float)calibration) / shunt_ohm;
    if ((!isfinite(current_lsb)) || (current_lsb < FLT_MIN) || (current_lsb > FLT_MAX / (65535.0f * 25000.0f)))
        return INA226_ERROR_PARAM;

    dev->hi2c          = hi2c;
    dev->address       = address;
    dev->shunt_ohm     = shunt_ohm;
    dev->max_current_A = max_current_A;
    dev->current_lsb_A = current_lsb;
    dev->power_lsb_W   = INA226_POWER_LSB_MULTIPLIER * current_lsb;
    dev->calibration   = calibration;
    configuration = INA226_CONFIG_RESERVED | INA226_CONFIG_AVG_16 | INA226_CONFIG_VBUSCT_1_1MS | INA226_CONFIG_VSHCT_1_1MS | INA226_CONFIG_MODE_CONT;
    dev->configuration = configuration;
    dev->mask_enable = 0U;
    dev->alert_limit = 0U;
    dev->alert_function_pending = false;
    dev->conversion_ready_pending = false;
    dev->initialized = false;

    hal_status = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(address << 1), 3U, INA226_I2C_TIMEOUT_MS);

    if (hal_status != HAL_OK)
    {
        if (hal_status == HAL_BUSY)
            return INA226_ERROR_I2C;

        if (INA226_FromHALStatus(hi2c, hal_status) == INA226_ERROR_TIMEOUT)
            return INA226_ERROR_TIMEOUT;

        return INA226_ERROR_NOT_FOUND;
    }

    status = INA226_Read16(dev, INA226_REG_MANUFACTURER_ID, &manufacturer_id);

    if (status != INA226_OK)
        return status;

    status = INA226_Read16(dev, INA226_REG_DIE_ID, &die_id);

    if (status != INA226_OK)
        return status;

    if ((manufacturer_id != INA226_MANUFACTURER_ID) || ((die_id & INA226_DIE_ID_MASK) != INA226_DIE_ID_VALUE))
    {
        return INA226_ERROR_NOT_FOUND;
    }

    status = INA226_Reset(dev);

    if (status != INA226_OK)
        return status;

    status = INA226_Write16(dev, INA226_REG_CONFIG, configuration);

    if (status != INA226_OK)
        return status;

    dev->configuration = configuration;
    status = INA226_Write16(dev, INA226_REG_CALIBRATION, calibration);

    if (status != INA226_OK)
        return status;

    dev->calibration = calibration;
    dev->initialized = true;

    return INA226_OK;
}


/** @brief Read bus voltage in millivolts. */
INA226_Status_t INA226_GetBusVoltage_mV(INA226_Handle_t *dev, float *voltage_mV)
{
    uint16_t raw;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || (voltage_mV == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    status = INA226_Read16(dev, INA226_REG_BUS_VOLTAGE, &raw);

    if (status != INA226_OK)
        return status;

    *voltage_mV = (float)raw * INA226_BUS_LSB_MV;

    return INA226_OK;
}


/** @brief Read signed shunt voltage in microvolts. */
INA226_Status_t INA226_GetShuntVoltage_uV(INA226_Handle_t *dev, float *voltage_uV)
{
    uint16_t raw;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || (voltage_uV == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    status = INA226_Read16(dev, INA226_REG_SHUNT_VOLTAGE, &raw);

    if (status != INA226_OK)
        return status;

    *voltage_uV = (float)((int16_t)raw) * INA226_SHUNT_LSB_UV;

    return INA226_OK;
}


/** @brief Read signed current in milliamperes. */
INA226_Status_t INA226_GetCurrent_mA(INA226_Handle_t *dev, float *current_mA)
{
    uint16_t raw;
    bool overflow = false;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || (current_mA == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    status = INA226_ReadFlags(dev, NULL, &overflow);

    if (status != INA226_OK)
        return status;

    if (overflow)
        return INA226_ERROR_OVERFLOW;

    status = INA226_Read16(dev, INA226_REG_CURRENT, &raw);

    if (status != INA226_OK)
        return status;

    *current_mA = (float)((int16_t)raw) * (dev->current_lsb_A * 1000.0f);

    return INA226_OK;
}


/** @brief Read power in milliwatts. */
INA226_Status_t INA226_GetPower_mW(INA226_Handle_t *dev, float *power_mW)
{
    uint16_t raw;
    bool overflow = false;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || (power_mW == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    status = INA226_ReadFlags(dev, NULL, &overflow);

    if (status != INA226_OK)
        return status;

    if (overflow)
        return INA226_ERROR_OVERFLOW;

    status = INA226_Read16(dev, INA226_REG_POWER, &raw);

    if (status != INA226_OK)
        return status;

    *power_mW = (float)raw * (dev->power_lsb_W * 1000.0f);

    return INA226_OK;
}


/** @brief Read and clear the Conversion Ready flag. */
INA226_Status_t INA226_IsConversionReady(INA226_Handle_t *dev, bool *ready)
{
    if (!INA226_IsHandleValid(dev) || (ready == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    return INA226_ReadFlags(dev, ready, NULL);
}


/** @brief Read the Math Overflow flag. */
INA226_Status_t INA226_IsOverflow(INA226_Handle_t *dev, bool *overflow)
{
    if (!INA226_IsHandleValid(dev) || (overflow == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    return INA226_ReadFlags(dev, NULL, overflow);
}


/** @brief Read all measurement values into one data structure. */
INA226_Status_t INA226_Read(INA226_Handle_t *dev, INA226_Data_t *data)
{
    uint16_t bus_raw;
    uint16_t shunt_raw;
    uint16_t current_raw;
    uint16_t power_raw;
    INA226_Status_t status;

    if (!INA226_IsHandleValid(dev) || (data == NULL))
        return INA226_ERROR_PARAM;

    if (!dev->initialized)
        return INA226_ERROR_PARAM;

    status = INA226_Read16(dev, INA226_REG_BUS_VOLTAGE, &bus_raw);

    if (status != INA226_OK)
        return status;

    status = INA226_Read16(dev, INA226_REG_SHUNT_VOLTAGE, &shunt_raw);

    if (status != INA226_OK)
        return status;

    status = INA226_Read16(dev, INA226_REG_CURRENT, &current_raw);

    if (status != INA226_OK)
        return status;

    status = INA226_Read16(dev, INA226_REG_POWER, &power_raw);

    if (status != INA226_OK)
        return status;

    data->bus_voltage_mV = (float)bus_raw * INA226_BUS_LSB_MV;
    data->shunt_voltage_uV = (float)((int16_t)shunt_raw) * INA226_SHUNT_LSB_UV;
    data->current_mA = (float)((int16_t)current_raw) * (dev->current_lsb_A * 1000.0f);
    data->power_mW = (float)power_raw * (dev->power_lsb_W * 1000.0f);

    return INA226_OK;
}
