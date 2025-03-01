#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_log.h"

#define TX_GPIO_NUM GPIO_NUM_13
#define RX_GPIO_NUM GPIO_NUM_14
#define MSG_ID 0x555  // 11 bit standard format ID
#define EXAMPLE_TAG "TWAI Transmit Test"

#define BUTTON_IN GPIO_NUM_10

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_13, GPIO_NUM_14, TWAI_MODE_NO_ACK);

typedef enum {
    RED,
    GREEN,
    BLUE
} color_state_t;

volatile color_state_t color_state = RED;

uint8_t colors[3][3] = {
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
};

// put function declarations here:
static void transmit_twai_msg();
static void button_poll_task(void* arg);
static char* color_state_to_string();

void app_main() {
    // put your setup code here, to run once:
    esp_log_level_set(EXAMPLE_TAG, ESP_LOG_DEBUG);

    gpio_set_direction(BUTTON_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_IN, GPIO_PULLUP_ONLY);

    twai_filter_config_t f_config;
    f_config.acceptance_code = (MSG_ID << 21);
    f_config.acceptance_mask = ~(TWAI_STD_ID_MASK << 21);
    f_config.single_filter = true;

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(EXAMPLE_TAG, "Driver Installed");

    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(EXAMPLE_TAG, "Driver started");

    xTaskCreatePinnedToCore(button_poll_task, "TWAI_btn_poll", 4096, NULL, 5, NULL, tskNO_AFFINITY);
}

// put function definitions here:
static void transmit_twai_msg() {
    twai_message_t txMsg;
    txMsg.data_length_code = 3;
    txMsg.identifier = MSG_ID;

    txMsg.data[0] = colors[color_state][0];
    txMsg.data[1] = colors[color_state][1];
    txMsg.data[2] = colors[color_state][2];

    ESP_LOGI(EXAMPLE_TAG, "Transmitting color: %s", color_state_to_string());
    ESP_ERROR_CHECK(twai_transmit(&txMsg, portMAX_DELAY));
}

static void button_poll_task(void* arg) {
    bool last_button_state = true;
    while (true) {
        bool button_pressed = gpio_get_level(BUTTON_IN) == 0;

        if (button_pressed && !last_button_state) {
            color_state = (color_state + 1) % 3;
            ESP_LOGI(EXAMPLE_TAG, "Setting color to: %s", color_state_to_string());
            transmit_twai_msg();
        }

        last_button_state = button_pressed;  // Update last button state
        vTaskDelay(pdMS_TO_TICKS(50));       // Debounce the button
    }
}

static char* color_state_to_string() {
    switch (color_state) {
        case RED:
            return "RED";
        case GREEN:
            return "GREEN";
        case BLUE:
            return "BLUE";
        default:
            return "UNKNOWN";
    }
}