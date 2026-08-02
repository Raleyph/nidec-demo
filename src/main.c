#include "console.h"
#include "motor.h"

#include <stdbool.h>
#include <stdio.h>

#include "esp_console.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define COMMAND_BUFFER_SIZE 64

static bool read_line(char *buffer, size_t buffer_size)
{
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

        if (character == '\b' || character == 0x7f) {
            if (length > 0) {
                length--;
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

void app_main(void)
{
    ESP_ERROR_CHECK(motor_init());
    ESP_ERROR_CHECK(console_init());

    printf("\nMotor console\n");
    printf("Type 'help' to list commands\n\n");

    char line[COMMAND_BUFFER_SIZE];

    while (true) {
        printf("> ");
        fflush(stdout);

        if (!read_line(line, sizeof(line))) {
            continue;
        }

        int command_result = 0;

        const esp_err_t err = esp_console_run(
            line,
            &command_result
        );

        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unknown command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            printf("Invalid command arguments\n");
        } else if (err != ESP_OK) {
            printf(
                "Console error: %s\n",
                esp_err_to_name(err)
            );
        } else if (command_result != 0) {
            printf(
                "Command returned error: %d\n",
                command_result
            );
        }
    }
}
