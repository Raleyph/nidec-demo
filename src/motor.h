#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define MOTOR_MIN_FREQ_HZ 1000U
#define MOTOR_MAX_FREQ_HZ 16000U

esp_err_t motor_init(void);

esp_err_t motor_set_frequency(uint32_t frequency_hz);

esp_err_t motor_ramp_to_frequency(uint32_t target_frequency_hz, uint32_t ramp_time_ms);
esp_err_t motor_ramp_stop(uint32_t ramp_time_ms);
esp_err_t motor_stop_immediately(void);

bool motor_is_running(void);
uint32_t motor_get_frequency(void);
