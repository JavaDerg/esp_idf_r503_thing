#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"

typedef struct __attribute__((__packed__)) {
    uint16_t header;
    uint32_t address;
    uint8_t id;
    uint16_t length;
    uint8_t data[258];
} fingerprint_packet_t;

uint16_t compute_sum(fingerprint_packet_t* packet)
{
    uint16_t sum = 0;
    
    sum += packet->id;

    packet->length += 2;
    sum += packet->length & 0xFF;
    sum += packet->length >> 8;
    packet->length -= 2;

    for (int i = 0; i < packet->length; i++)    
        sum += packet->data[i];

    printf("sum: %04X\n", sum);

    return sum;
}

int write_packet(uart_port_t uart_num, fingerprint_packet_t* packet)
{
    uint16_t sum = compute_sum(packet);

    packet->data[packet->length] = sum >> 8;
    packet->data[packet->length + 1] = sum & 0xFF;

    packet->length += 2;

    ESP_LOGI("write_packet", "packet->header: %04X", packet->header);

    uint16_t len = packet->length;
    // switch up bytes
    packet->length = (packet->length >> 8) | (packet->length << 8);

    for (int i = 0; i < len + 11; i++)
        printf("%02X ", ((char*) packet)[i]);

    int res = uart_write_bytes(uart_num, packet, len + 11);
    
    // restore bytes
    packet->length = (packet->length >> 8) | (packet->length << 8);

    packet->length -= 2;

    return res;
}

static const char* TAG = "main";

const uart_port_t FINGERPRINT = UART_NUM_1;

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "4");
    //QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(FINGERPRINT, 1024, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "1");
    uart_config_t uart_config = {
        .baud_rate = 57600,
        // .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(FINGERPRINT, &uart_config));
    ESP_LOGI(TAG, "2");

    ESP_ERROR_CHECK(uart_set_pin(FINGERPRINT, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "3");

    // verify password
    fingerprint_packet_t packet = {
        .header = 0x01EF,
        .address = 0xFFFFFFFF,
        .id = 0x01,
        .length = 0x0005,
        .data = {
            // instruction code
            0x13,
            // password
            0x00, 0x00, 0x00, 0x00,
        },
    };

    if (write_packet(FINGERPRINT, &packet) == -1) {
        ESP_ERROR_CHECK(-1);
    }
    ESP_LOGI(TAG, "5");

    uint8_t data[128];

    /*if (write_packet(FINGERPRINT, &packet) == -1) {
        ESP_ERROR_CHECK(-1);
    }*/

    for(;;) {
        uint32_t length = uart_read_bytes(FINGERPRINT, data, 128, pdMS_TO_TICKS(1000));

        if (length == 0) continue;

        ESP_LOGE(TAG, "length: %lu", length);

        for (int i = 0; i < length; i++)
            printf("%02X ", data[i]);
        printf(" :::: %lu\n", length);
    }
}
