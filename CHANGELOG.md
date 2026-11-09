# Changelog

## 0.2.0 — 2026-11-09

### Fixed

- Recalculate Current LSB and Power LSB from the integer Calibration register value, avoiding the scaling error caused by truncation.
- Limit Calibration using a conservative shunt ADC code bound to prevent Current register overflow within the requested ideal measurement range. Sensor offset, gain error, and noise still require application headroom.
- Validate float intermediates and output scaling limits before converting Calibration to an integer. Production calculations use float throughout.
- Check OVF in current and power getters even when ALERT latch mode is enabled.
- Preserve AFF and CVRF consumed by Mask/Enable reads, including raw register reads. Pending flags coalesce events rather than count them.
- Discard stale pending CVRF when a new conversion starts. Preserve pending AFF across mode changes until consumed. Clear pending events on reset, deinitialization, and write failures.
- Invalidate initialization after any failed write or a direct Configuration/Calibration write. Reinitialize before using high-level operations again.
- Recognize HAL_ERROR with HAL_I2C_ERROR_TIMEOUT during device probing.
- Prevent reset poll counter wraparound if the attempt limit is increased.

### Portability

- Add an optional INA226_HAL_HEADER compile definition to select the HAL header; main.h remains the default.
- Include stdint.h explicitly in the public header.

### Compatibility

- Public function signatures are unchanged.
- INA226_Handle_t gains conversion_ready_pending. Rebuild the application and driver together; binary compatibility is not preserved.
- Current/power getters now read Mask/Enable in latch mode, acknowledging the physical ALERT latch while preserving events in software. INA226_Read still performs four measurement reads without acknowledging ALERT or checking OVF.
- Init parameter validation failure leaves the previous handle and hardware unchanged. Failure after starting I2C initialization leaves the handle uninitialized.

### Validation

- Linux HAL-mock regressions, UBSan, calibration boundary checks, GCC warnings and static analysis completed.
- Linked STM32G030F6P6 Release firmware (-Os, ARM GCC 14.3): Flash 10624 → 10812 bytes; static RAM unchanged at 1720 bytes. Results depend on application and toolchain; hardware validation of the new behavior is pending.

## 0.1.0 — 2026-09-29

- Initial public release: blocking STM32 HAL I2C driver, per-device handles, automatic calibration, configurable conversion modes and averaging, ALERT support, raw register access, and English/Russian documentation.
