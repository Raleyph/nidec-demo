#include "motor.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#define COMMAND_BUFFER_SIZE 64

#define MOTOR_START_RAMP_TIME_MS 2000U
#define MOTOR_CHANGE_RAMP_TIME_MS 1000U
#define MOTOR_STOP_RAMP_TIME_MS 2000U

static const char *TAG = "console";

static void print_help(void)
{
    printf(
        "\nCommands:\n"
        "  freq <1000-16000>  Start or change frequency smoothly\n"
        "  stop               Stop motor smoothly\n"
        "  emergency           Stop PFM immediately\n"
        "  status             Show motor state\n"
        "  help               Show this help\n\n"
    );
}

static bool parse_frequency(const char *text, uint32_t *frequency_hz)
{
    if (text == NULL || frequency_hz == NULL) {
        return false;
    }

    errno = 0;
    char *end = NULL;

    const unsigned long value = strtoul(
        text,
        &end,
        10
    );

    if (
        errno != 0 ||
        end == text ||
        *end != '\0' ||
        value < MOTOR_MIN_FREQ_HZ ||
        value > MOTOR_MAX_FREQ_HZ
    ) {
        return false;
    }

    *frequency_hz = (uint32_t)value;

    return true;
}

static void print_status(void)
{
    if (!motor_is_running()) {
        printf("Motor: stopped\n");
        return;
    }

    printf(
        "Motor: running, frequency=%lu Hz\n",
        (unsigned long)motor_get_frequency()
    );
}

static void process_frequency_command(char *argument)
{
    uint32_t frequency_hz = 0;

    if (!parse_frequency(argument, &frequency_hz)) {
        ESP_LOGE(
            TAG,
            "Usage: freq <%u-%u>",
            MOTOR_MIN_FREQ_HZ,
            MOTOR_MAX_FREQ_HZ
        );

        return;
    }

    const bool was_running = motor_is_running();

    const uint32_t ramp_time_ms = was_running
        ? MOTOR_CHANGE_RAMP_TIME_MS
        : MOTOR_START_RAMP_TIME_MS;

    ESP_LOGI(
        TAG,
        "%s to %lu Hz",
        was_running ? "Ramping" : "Starting",
        (unsigned long)frequency_hz
    );

    const esp_err_t err = motor_ramp_to_frequency(
        frequency_hz,
        ramp_time_ms
    );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Motor ramp failed: %s",
            esp_err_to_name(err)
        );
        
        motor_stop_immediately();
        return;
    }

    ESP_LOGI(
        TAG,
        "Target frequency reached: %lu Hz",
        (unsigned long)frequency_hz
    );
}

static bool read_command(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size < 2) {
        return false;
    }

    size_t length = 0;

    while (true) {
        const int character = getchar();

        if (character == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (character == '\r' || character == '\n') {
            if (length == 0) {
                continue;
            }

            buffer[length] = '\0';
            printf("\r\n");
            return true;
        }

        if (character == '\b' || character == 0x7F) {
            if (length > 0) {
                --length;
                printf("\b \b");
                fflush(stdout);
            }

            continue;
        }

        if (length >= buffer_size - 1) {
            continue;
        }

        buffer[length++] = (char)character;

        putchar(character);
        fflush(stdout);
    }
}

static void process_command(char *line)
{
    line[strcspn(line, "\r\n")] = '\0';

    char *command = strtok(line, " ");

    if (command == NULL) {
        return;
    }

    if (strcmp(command, "freq") == 0) {
        char *argument = strtok(NULL, " ");
        process_frequency_command(argument);
        return;
    }

    if (strcmp(command, "stop") == 0) {
        if (!motor_is_running()) {
            ESP_LOGI(TAG, "Motor is already stopped");
            return;
        }

        ESP_LOGI(TAG, "Stopping motor smoothly");

        const esp_err_t err = motor_ramp_stop(
            MOTOR_STOP_RAMP_TIME_MS
        );

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Smooth stop failed: %s",
                esp_err_to_name(err)
            );

            motor_stop_immediately();
            return;
        }

        ESP_LOGI(TAG, "Motor stopped");
        return;
    }

    if (strcmp(command, "emergency") == 0) {
        const esp_err_t err = motor_stop_immediately();

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Emergency stop failed: %s",
                esp_err_to_name(err)
            );

            return;
        }

        ESP_LOGW(TAG, "PFM stopped immediately");
        return;
    }

    if (strcmp(command, "status") == 0) {
        print_status();
        return;
    }

    if (strcmp(command, "help") == 0) {
        print_help();
        return;
    }

    ESP_LOGW(TAG, "Unknown command: %s", command);
    print_help();
}

void app_main(void)
{
    ESP_ERROR_CHECK(motor_init());

    ESP_LOGI(TAG, "Motor initialized in stopped state");

    print_help();

    char command_buffer[COMMAND_BUFFER_SIZE];

    while (true) {
        printf("> ");
        fflush(stdout);

        if (!read_command(
                command_buffer,
                sizeof(command_buffer)
            )) {
            continue;
        }

        process_command(command_buffer);
    }
}
