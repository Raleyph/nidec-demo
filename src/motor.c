#include "motor.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MOTOR_PFM_GPIO GPIO_NUM_2

#define MOTOR_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_TIMER   LEDC_TIMER_0
#define MOTOR_LEDC_CHANNEL LEDC_CHANNEL_0

#define MOTOR_DUTY_BITS LEDC_TIMER_10_BIT
#define MOTOR_DUTY_50_PERCENT 512U

#define MOTOR_RAMP_STEP_HZ 100U

static bool motor_running = false;
static uint32_t motor_current_frequency_hz = MOTOR_MIN_FREQ_HZ;

static esp_err_t motor_set_signal_enabled(bool enabled)
{
    const uint32_t duty = enabled
        ? MOTOR_DUTY_50_PERCENT
        : 0U;

    esp_err_t err = ledc_set_duty(
        MOTOR_LEDC_MODE,
        MOTOR_LEDC_CHANNEL,
        duty
    );

    if (err != ESP_OK) {
        return err;
    }

    return ledc_update_duty(
        MOTOR_LEDC_MODE,
        MOTOR_LEDC_CHANNEL
    );
}

static uint32_t calculate_step_count(
    uint32_t start_frequency_hz,
    uint32_t target_frequency_hz
)
{
    const uint32_t difference_hz =
        start_frequency_hz > target_frequency_hz
            ? start_frequency_hz - target_frequency_hz
            : target_frequency_hz - start_frequency_hz;

    if (difference_hz == 0U) {
        return 0U;
    }

    return (
        difference_hz + MOTOR_RAMP_STEP_HZ - 1U
    ) / MOTOR_RAMP_STEP_HZ;
}

static TickType_t calculate_step_delay(
    uint32_t ramp_time_ms,
    uint32_t step_count
)
{
    if (step_count == 0U) {
        return 0;
    }

    const uint32_t delay_ms = ramp_time_ms / step_count;
    const TickType_t delay_ticks = pdMS_TO_TICKS(delay_ms);

    return delay_ticks > 0 ? delay_ticks : 1;
}

esp_err_t motor_init(void)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = MOTOR_LEDC_MODE,
        .duty_resolution = MOTOR_DUTY_BITS,
        .timer_num = MOTOR_LEDC_TIMER,
        .freq_hz = MOTOR_MIN_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer_config);

    if (err != ESP_OK) {
        return err;
    }

    const ledc_channel_config_t channel_config = {
        .gpio_num = MOTOR_PFM_GPIO,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel = MOTOR_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = MOTOR_LEDC_TIMER,

        .duty = 0,
        .hpoint = 0,
    };

    err = ledc_channel_config(&channel_config);

    if (err != ESP_OK) {
        return err;
    }

    motor_running = false;
    motor_current_frequency_hz = MOTOR_MIN_FREQ_HZ;

    return ESP_OK;
}

esp_err_t motor_set_frequency(uint32_t frequency_hz)
{
    if (
        frequency_hz < MOTOR_MIN_FREQ_HZ ||
        frequency_hz > MOTOR_MAX_FREQ_HZ
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t err = ledc_set_freq(
        MOTOR_LEDC_MODE,
        MOTOR_LEDC_TIMER,
        frequency_hz
    );

    if (err != ESP_OK) {
        return err;
    }

    motor_current_frequency_hz = frequency_hz;

    return ESP_OK;
}

esp_err_t motor_ramp_to_frequency(
    uint32_t target_frequency_hz,
    uint32_t ramp_time_ms
)
{
    if (
        target_frequency_hz < MOTOR_MIN_FREQ_HZ ||
        target_frequency_hz > MOTOR_MAX_FREQ_HZ
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ramp_time_ms == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!motor_running) {
        esp_err_t err = motor_set_frequency(
            MOTOR_MIN_FREQ_HZ
        );

        if (err != ESP_OK) {
            return err;
        }

        err = motor_set_signal_enabled(true);

        if (err != ESP_OK) {
            return err;
        }

        motor_running = true;
    }

    const uint32_t start_frequency_hz =
        motor_current_frequency_hz;

    const uint32_t step_count = calculate_step_count(
        start_frequency_hz,
        target_frequency_hz
    );

    if (step_count == 0U) {
        return ESP_OK;
    }

    const TickType_t step_delay = calculate_step_delay(
        ramp_time_ms,
        step_count
    );

    uint32_t frequency_hz = start_frequency_hz;

    while (frequency_hz != target_frequency_hz) {
        if (frequency_hz < target_frequency_hz) {
            const uint32_t remaining_hz =
                target_frequency_hz - frequency_hz;

            frequency_hz += remaining_hz > MOTOR_RAMP_STEP_HZ
                ? MOTOR_RAMP_STEP_HZ
                : remaining_hz;
        } else {
            const uint32_t remaining_hz =
                frequency_hz - target_frequency_hz;

            frequency_hz -= remaining_hz > MOTOR_RAMP_STEP_HZ
                ? MOTOR_RAMP_STEP_HZ
                : remaining_hz;
        }

        const esp_err_t err = motor_set_frequency(
            frequency_hz
        );

        if (err != ESP_OK) {
            return err;
        }

        vTaskDelay(step_delay);
    }

    return ESP_OK;
}

esp_err_t motor_ramp_stop(uint32_t ramp_time_ms)
{
    if (ramp_time_ms == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!motor_running) {
        return ESP_OK;
    }

    esp_err_t err = motor_ramp_to_frequency(
        MOTOR_MIN_FREQ_HZ,
        ramp_time_ms
    );

    if (err != ESP_OK) {
        return err;
    }

    err = motor_set_signal_enabled(false);

    if (err != ESP_OK) {
        return err;
    }

    motor_running = false;
    motor_current_frequency_hz = MOTOR_MIN_FREQ_HZ;

    return ESP_OK;
}

esp_err_t motor_stop_immediately(void)
{
    const esp_err_t err = motor_set_signal_enabled(false);

    if (err != ESP_OK) {
        return err;
    }

    motor_running = false;
    motor_current_frequency_hz = MOTOR_MIN_FREQ_HZ;

    return ESP_OK;
}

bool motor_is_running(void)
{
    return motor_running;
}

uint32_t motor_get_frequency(void)
{
    return motor_current_frequency_hz;
}
