
#include "uart_config.h"

static const char *TAG = "UART_CONFIG";
extern volatile bool uart_exit;
TaskHandle_t uartTaskHandle;
char status_data[50];

void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void uart_reinit(void)
{ 
    // Reinstall UART driver for OTA communication
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void uart_write_byte(uint8_t data) {
    uart_write_bytes(UART_PORT_NUM, &data, 1);
    ESP_LOGD(TAG, "TX: 0x%02x", data);
    vTaskDelay(pdMS_TO_TICKS(1));
}

int uart_read_bytes_timeout(uint8_t *data, size_t length, int timeout_ms) {
    int bytes_read = uart_read_bytes(UART_PORT_NUM, data, length, pdMS_TO_TICKS(timeout_ms));
    if (bytes_read > 0) {
        ESP_LOGD(TAG, "RX: %d bytes", bytes_read);
    }
    return bytes_read;
}

void uart_flush_rx_buffer(void) {
    uart_flush_input(UART_PORT_NUM);             // clear RX
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(100));  // wait for TX to finish (important)
    vTaskDelay(pdMS_TO_TICKS(50));
}

esp_err_t send_bootloader_packet(uint8_t *packet, size_t total_len) {
    uart_write_byte(packet[0]);
    vTaskDelay(pdMS_TO_TICKS(10));
    for (int i = 1; i < total_len; i++) {
        uart_write_byte(packet[i]);
        vTaskDelay(pdMS_TO_TICKS(2));
    } 
    return ESP_OK;
}

esp_err_t read_bootloader_reply(uint8_t command_code, uint8_t *response_data, size_t *response_len) {
    uint8_t ack[2] = {0};
    
    // Read ACK + length (2 bytes)
    int bytes_read = uart_read_bytes_timeout(ack, 2, 5000);
    if (bytes_read != 2) {
        ESP_LOGE(TAG, "Failed to read ACK+length: got %d bytes", bytes_read);
        return ESP_FAIL;
    }
    
    if (ack[0] == BL_ACK) { // 0xA5
        uint32_t len_to_follow = ack[1];
        ESP_LOGI(TAG, "CRC: SUCCESS Len: %" PRIu32, len_to_follow);
        
        if (len_to_follow > 0 && response_data && response_len) {
            int data_read = uart_read_bytes_timeout(response_data, len_to_follow, 3000);
            if (data_read == (int)len_to_follow) {
                *response_len = len_to_follow;
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "Failed to read response data: expected %" PRIu32 ", got %d", len_to_follow, data_read);
                return ESP_FAIL;
            }
        }
        return ESP_OK;
    } else if (ack[0] == BL_NACK) { // 0x7F
        ESP_LOGE(TAG, "CRC: FAIL - NACK received");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Unexpected response: 0x%02x", ack[0]);
        return ESP_FAIL;
    }
}

void uart_task_delete(void)
{
    if (uartTaskHandle != NULL) {
        ESP_LOGI(TAG, "Cancelling uart receive task before OTA");
        uart_exit = true;
        while (uartTaskHandle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        uart_driver_delete(UART_PORT_NUM);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void uart_rx_task(void *arg)
{
    uint8_t byte;
    char json_buffer[BUF_SIZE] = {0};
    int index = 0;

    while (!uart_exit) {
        int len = uart_read_bytes(UART_PORT_NUM, &byte, 1, pdMS_TO_TICKS(100)); 
        if (len > 0) {
            if (byte == '\n' || byte == '\r') {
                if (index > 0) {
                    json_buffer[index] = '\0'; 
                    ESP_LOGI(TAG, "Received JSON: %s", json_buffer);
                    esp_mqtt_client_publish(mqtt_client, current_status, json_buffer, 0, 1, 0);
                    if(ble_status())
                    {
                        ble_send_response(json_buffer);
                    }

                    index = 0;
                    memset(json_buffer, 0, sizeof(json_buffer));
                }
            }
            else {
                if (index < BUF_SIZE - 1) {
                    json_buffer[index++] = byte;
                }
            }
        }
    }
    ESP_LOGI(TAG, "UART RX task exiting");
    uartTaskHandle = NULL;
    vTaskDelete(NULL); 
}