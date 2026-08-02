#include "console.h"

#include <stdlib.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"

#include "motor.h"

static int command_frequency(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: freq <1000-16000>\n");
        return 1;
    }

    uint32_t frequency = strtoul(argv[1], NULL, 10);

    motor_ramp_to_frequency(
        frequency,
        1500
    );

    return 0;
}

static int command_stop(int argc, char **argv)
{
    motor_ramp_stop(1500);
    return 0;
}

esp_err_t console_init(void)
{
    const esp_console_config_t console_config = {
        .max_cmdline_args = 4,
        .max_cmdline_length = 64,
    };

    esp_err_t err = esp_console_init(&console_config);
    if (err != ESP_OK) {
        return err;
    }

    const esp_console_cmd_t frequency_command = {
        .command = "freq",
        .help = "Smoothly start or change motor frequency",
        .hint = "<1000-16000>",
        .func = command_frequency,
    };

    err = esp_console_cmd_register(&frequency_command);
    if (err != ESP_OK) {
        return err;
    }

    const esp_console_cmd_t stop_command = {
        .command = "stop",
        .help = "Smoothly stop motor",
        .func = command_stop,
    };

    err = esp_console_cmd_register(&stop_command);
    if (err != ESP_OK) {
        return err;
    }

    esp_console_register_help_command();

    return ESP_OK;
}
