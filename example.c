/*
 * Minimal INA226 continuous-mode example for STM32 HAL.
 *
 * Add ina226.c and ina226.h to the application, then call
 * INA226_ExampleInit() after MX_I2C1_Init(). Call INA226_ExampleProcess()
 * periodically from the main loop.
 */

#include "main.h"
#include "ina226.h"


extern I2C_HandleTypeDef hi2c1;

INA226_Handle_t ina226_example_device;
INA226_Data_t ina226_example_data;
INA226_Status_t ina226_example_status;


/** @brief Initialize an INA226 with an R100 shunt for continuous measurements. */
void INA226_ExampleInit(void)
{
    ina226_example_status = INA226_Init(&ina226_example_device, &hi2c1, INA226_ADDR_GND_GND, 0.1f);

    if (ina226_example_status != INA226_OK)
        Error_Handler();
}


/** @brief Read the latest continuous conversion results. */
void INA226_ExampleProcess(void)
{
    ina226_example_status = INA226_Read(&ina226_example_device, &ina226_example_data);

    if (ina226_example_status == INA226_OK)
    {
        /* ina226_example_data.bus_voltage_mV */
        /* ina226_example_data.shunt_voltage_uV */
        /* ina226_example_data.current_mA */
        /* ina226_example_data.power_mW */
    }
}
