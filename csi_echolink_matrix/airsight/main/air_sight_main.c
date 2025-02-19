/**
 * @brief: AirSight
 * @author: 伍志勇
 * @date: 2025-02-18
 * 
 * 概要设计：
 * 1、WiFi 初始化：
 *      使用 esp_wifi_init 初始化 WiFi。
 *      配置 SoftAP 和 STA 模式。
 *      注册 WiFi 事件处理函数，处理 STA 连接和断开事件。
 * 2、WiFi 事件处理：
 *      当 STA 启动时，自动连接 WiFi 热点。
 *      在 STA 连接成功后，获取其 IP 地址。注：只有在 STA 成功获取 IP 地址后，才会尝试转发数据。
 *      当 STA 断开连接时，等待 3 秒后重试。
 * 3、UDP 服务器：
 *      创建一个 UDP socket，绑定到端口 3333。
 *      接收CSI数据：使用 recvfrom 接收来自客户端的数据，并打印到日志。
 *      转发CSI数据：将接收到的数据通过 UDP 发送到指定的 IP 和端口。
 * 4、任务调度：
 *      使用 FreeRTOS 创建 UDP 服务器任务。
 * 
 * 5、注意事项
 * 1）WiFi 配置：
 *      确保 无线城市 热点没有密码，或者根据实际情况修改代码。
 *      确保 FORWARD_IP 和 FORWARD_PORT 设置正确，且目标设备在同一个局域网中。
 * 2）UDP 数据接收：
 *      接收的数据长度不能超过 rx_buffer 的大小（1024 字节）。
 * 3）调试：
 *      使用 ESP_LOGI 打印日志，方便调试和观察程序运行状态。
 */

 /**
  * 设计图：
  * 
  * 1、功能架构
  * ---------------------------------------------------------------------------------------------
```mermaid
graph TD
    A[WiFi SoftAP] -->|创建热点| B[SSID: AirSight, Password: AirSight@hkttech.cn]
    C[WiFi STA] -->|连接热点| D[SSID: 无线城市, Password: 空]
    E[UDP Server] -->|监听端口| F[Port: 3333]
    G[数据接收] -->|接收数据| E
    H[数据转发] -->|转发数据| I[目标 IP: 192.168.1.100, Port: 4444]
    B --> G
    D --> G
    G --> H
  * -----------------------------------------------------------------------------------------------
  * 
  * 2、数据流图
  * -----------------------------------------------------------------------------------------------
```mermaid
graph LR
    A[SoftAP 客户端] -->|发送 UDP 数据| B[ESP32-S3 UDP Server]
    B -->|接收数据| C{数据接收}
    C -->|转发数据| D[目标设备: 192.168.1.100:4444]
    E[WiFi STA] -->|连接到| F[无线城市热点]
    F -->|获取 IP| G[STA IP]
    G -->|用于转发| D
  * -----------------------------------------------------------------------------------------------
  */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

// 定义 WiFi 配置
#define SOFTAP_SSID "AirSight"
#define SOFTAP_PASSWORD "AirSight@hkttech.cn"
#define STA_SSID "无线城市"
#define STA_PASSWORD ""

// 定义 UDP 服务器端口
#define UDP_PORT 3333

// 定义转发目标的 IP 和端口
#define FORWARD_IP "192.168.99.55" // 替换为目标 IP
#define FORWARD_PORT 4444          // 替换为目标端口

// 日志标签
static const char *TAG = "ESP32_WIFI_UDP";

// STA 模式的 IP 地址
static esp_ip4_addr_t sta_ip = {0};

// WiFi 事件处理函数
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); // STA 模式启动后尝试连接
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "STA disconnected, retrying...");
        vTaskDelay(3000 / portTICK_PERIOD_MS); // 等待 3 秒后重试
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        sta_ip = event->ip_info.ip; // 保存 STA 的 IP 地址
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&sta_ip));
    }
}

// 初始化 WiFi
static void wifi_init() {
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化 WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建 SoftAP 和 STA 接口
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    // 初始化 WiFi 配置
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册 WiFi 事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 配置 SoftAP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = SOFTAP_SSID,
            .ssid_len = strlen(SOFTAP_SSID),
            .password = SOFTAP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // 配置 STA
    wifi_config_t sta_config = {
        .sta = {
            .ssid = STA_SSID,
            .password = STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_OPEN
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
}

// UDP 服务器任务
static void udp_server_task(void *pvParameters) {
    char rx_buffer[1024];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t socklen = sizeof(client_addr);

    // 创建 UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
    }

    // 绑定 socket 到端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(sock);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "UDP server started on port %d", UDP_PORT);

    // 创建转发目标的地址结构
    struct sockaddr_in forward_addr;
    forward_addr.sin_family = AF_INET;
    forward_addr.sin_port = htons(FORWARD_PORT);
    inet_pton(AF_INET, FORWARD_IP, &forward_addr.sin_addr);

    while (1) {
        // 接收数据
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&client_addr, &socklen);
        if (len > 0) {
            rx_buffer[len] = 0; // 添加字符串结束符
            ESP_LOGI(TAG, "Received data: %s", rx_buffer);

            // 转发数据到指定 IP 和端口
            if (sta_ip.addr != 0) { // 确保 STA 已获取 IP
                int sent = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&forward_addr, sizeof(forward_addr));
                if (sent < 0) {
                    ESP_LOGE(TAG, "Failed to send data to forward target");
                } else {
                    ESP_LOGI(TAG, "Forwarded data to %s:%d", FORWARD_IP, FORWARD_PORT);
                }
            } else {
                ESP_LOGE(TAG, "STA IP not available, cannot forward data");
            }
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

void app_main() {
    // 初始化 WiFi
    wifi_init();

    // 创建 UDP 服务器任务
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}