#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/twai.h"
#include "esp_log.h"

#define NO_OF_MSGS 100
#define NO_OF_ITERS 3
#define TX_GPIO_NUM GPIO_NUM_13
#define RX_GPIO_NUM GPIO_NUM_14
#define TX_TASK_PRIO 8    // Sending task priority
#define RX_TASK_PRIO 9    // Receiving task priority
#define CTRL_TSK_PRIO 10  // Control task priority
#define MSG_ID 0x555      // 11 bit standard format ID
#define EXAMPLE_TAG "TWAI Receive Test"

#define LEDC_CHANNEL_R LEDC_CHANNEL_0
#define LEDC_CHANNEL_G LEDC_CHANNEL_1
#define LEDC_CHANNEL_B LEDC_CHANNEL_3
#define LEDC_GPIO_R GPIO_NUM_11
#define LEDC_GPIO_G GPIO_NUM_10
#define LEDC_GPIO_B GPIO_NUM_9

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_13, GPIO_NUM_14, TWAI_MODE_NO_ACK);

static SemaphoreHandle_t rxSem;
static SemaphoreHandle_t ctrlSem;
static SemaphoreHandle_t doneSem;

// put function declarations here:
static void twaiReceiveTask(void* arg);
static void twaiControlTask(void* arg);

void setRGB(int r, int g, int b);
void configureRGBLedC();

void app_main() {
    // put your setup code here, to run once:
    esp_log_level_set(EXAMPLE_TAG, ESP_LOG_DEBUG);

    configureRGBLedC();

    twai_filter_config_t f_config;
    f_config.acceptance_code = (MSG_ID << 21);
    f_config.acceptance_mask = ~(TWAI_STD_ID_MASK << 21);
    f_config.single_filter = true;

    rxSem = xSemaphoreCreateBinary();
    ctrlSem = xSemaphoreCreateBinary();
    doneSem = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(twaiControlTask, "TWAI_ctrl", 4096, NULL, CTRL_TSK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(twaiReceiveTask, "TWAI_rx", 4096, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY);

    // Install TWAI driver
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(EXAMPLE_TAG, "Driver Installed");

    xSemaphoreGive(ctrlSem);
    xSemaphoreTake(doneSem, portMAX_DELAY);

    ESP_ERROR_CHECK(twai_driver_uninstall());
    ESP_LOGI(EXAMPLE_TAG, "Driver Uninstalled");

    vSemaphoreDelete(rxSem);
    vSemaphoreDelete(ctrlSem);
    vQueueDelete(doneSem);
}

// put function definitions here:
static void twaiReceiveTask(void* arg) {
    xSemaphoreTake(rxSem, portMAX_DELAY);
    twai_message_t rxMsg;

    while (true) {
        ESP_LOGI(EXAMPLE_TAG, "Receiving...");
        esp_err_t err = twai_receive(&rxMsg, 0);
        if (err == ESP_OK) {
            ESP_LOGI(EXAMPLE_TAG, "Msg received - Data[0] = %d", rxMsg.data[0]);
            ESP_LOGI(EXAMPLE_TAG, "Msg received - Data[1] = %d", rxMsg.data[1]);
            ESP_LOGI(EXAMPLE_TAG, "Msg received - Data[2] = %d", rxMsg.data[2]);
            int r_value = rxMsg.data[0];
            int g_value = rxMsg.data[1];
            int b_value = rxMsg.data[2];
            setRGB(r_value, g_value, b_value);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
};

static void twaiControlTask(void* arg) {
    xSemaphoreTake(ctrlSem, portMAX_DELAY);

    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(EXAMPLE_TAG, "Driver started");

    xSemaphoreGive(rxSem);
    xSemaphoreTake(ctrlSem, portMAX_DELAY);

    ESP_ERROR_CHECK(twai_stop());
    ESP_LOGI(EXAMPLE_TAG, "Driver stopped");

    xSemaphoreGive(doneSem);
    vTaskDelete(NULL);
};

void setRGB(int r, int g, int b) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_R, r);  // Max intensity for Red
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_R);  // Apply Red duty cycle
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_G, g);  // Turn off Green
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_G);  // Apply Green duty cycle
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_B, b);  // Turn off Blue
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_B);  // Apply Blue duty cycle
}

void configureRGBLedC() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel_r = {
        .gpio_num = LEDC_GPIO_R,            // GPIO pin for Red
        .speed_mode = LEDC_LOW_SPEED_MODE,  // Low speed mode
        .channel = LEDC_CHANNEL_R,          // Channel for Red
        .intr_type = LEDC_INTR_DISABLE,     // Disable interrupts
        .duty = 0,                          // Initial duty cycle (off)
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_r));  // Configure the Red channel

    ledc_channel_config_t ledc_channel_g = {
        .gpio_num = LEDC_GPIO_G,            // GPIO pin for Green
        .speed_mode = LEDC_LOW_SPEED_MODE,  // Low speed mode
        .channel = LEDC_CHANNEL_G,          // Channel for Green
        .intr_type = LEDC_INTR_DISABLE,     // Disable interrupts
        .duty = 0,                          // Initial duty cycle (off)
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_g));  // Configure the Green channel

    ledc_channel_config_t ledc_channel_b = {
        .gpio_num = LEDC_GPIO_B,            // GPIO pin for Blue
        .speed_mode = LEDC_LOW_SPEED_MODE,  // Low speed mode
        .channel = LEDC_CHANNEL_B,          // Channel for Blue
        .intr_type = LEDC_INTR_DISABLE,     // Disable interrupts
        .duty = 0,                          // Initial duty cycle (off)
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_b));  // Configure the Blue channel
}