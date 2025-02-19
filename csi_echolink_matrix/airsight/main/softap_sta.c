/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*  WiFi softAP & station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "lwip/err.h"
#include "lwip/sys.h"

// #include "csi_data_tools.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_ESP_WIFI_STA_SSID "mywifissid"
*/

/* STA Configuration */
#define EXAMPLE_ESP_WIFI_STA_SSID CONFIG_ESP_WIFI_REMOTE_AP_SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_STA_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* AP Configuration */
#define EXAMPLE_ESP_WIFI_AP_SSID CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN_AP

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/*DHCP server option*/
#define DHCPS_OFFER_DNS 0x02

static const char *TAG_RECV = "AirSight Recv";
static const char *TAG_AP = "AirSight AP";
static const char *TAG_STA = "AirSigth Sta";

static int s_retry_num = 0;

/* FreeRTOS event group to signal when we are connected/disconnected */
static EventGroupHandle_t s_wifi_event_group;

// 定义错误编码
#define SEND_ERROR_SOCKET 1
#define SEND_ERROR_CONNECT 2
#define SEND_ERROR_SEND 3
// 定义最大包长度
#define MAX_PACKET_SIZE 512

#define ECHO_SERVER_PORT 3333

static int recv_multicast_init()
{
    struct sockaddr_in saddr = {0};

    char *udp_server_send_buf = "AirSight working!";

    ESP_LOGI(TAG_RECV, "recv_csi_data_multicast 1\n");

    // 创建 IPv4 UDP 套接字
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        ESP_LOGE(TAG_RECV, "Create UDP socket fail");
    }
    else
    {
        ESP_LOGI(TAG_RECV, "Created UPD socket!");

        // 启用 SO_REUSEADDR 选项， 允许服务器绑定当前已经存在已建立连接的地址
        int opt = 1;
        int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (ret < 0)
        {
            ESP_LOGE(TAG_RECV, "Failed to set SO_REUSEADDR. Error %d", errno);
        }
        else
        {
            // 绑定套接字
            saddr.sin_family = AF_INET;
            saddr.sin_port = ECHO_SERVER_PORT;
            saddr.sin_addr.s_addr = INADDR_ANY;
            ret = bind(sockfd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
            if (ret < 0)
            {
                ESP_LOGE(TAG_RECV, "Failed to bind socket. Error %d", errno);
                close(sockfd);
            } 

            ESP_LOGI(TAG_RECV, "recv_csi_data_multicast 2\n");
        }
    }

    return sockfd;
}

static void recv_csi_data_multicast(int sockfd)
{

    char udp_server_buf[1024] = {0};
    struct sockaddr_in from_addr = {0};
    socklen_t from_addr_len = sizeof(struct sockaddr_in);

    // 调用 recvfrom 接口接收组播数据
    int ret = recvfrom(sockfd, udp_server_buf, sizeof(udp_server_buf) - 1, 0, (struct sockaddr *)&from_addr, (socklen_t *)&from_addr_len);
    if (ret > 0)
    {
        ESP_LOGI(TAG_RECV, "Receive udp multicast from %s:%d, data is %s", inet_ntoa(((struct sockaddr_in *)&from_addr)->sin_addr), ntohs(((struct sockaddr_in *)&from_addr)->sin_port), udp_server_buf);
        // 如果收到组播请求数据，单播发送对端数据通信应用端口
        // if (!strcmp(udp_server_buf, "Are you Espressif IOT Smart Light")) {
        //    ret = sendto(sockfd, udp_server_send_buf, strlen(udp_server_send_buf), 0, (struct sockaddr *)&from_addr, from_addr_len);
        //    if (ret < 0) {
        //       ESP_LOGE(TAG_RECV, "Error occurred during sending: errno %d", errno);
        //    } else {
        //       ESP_LOGI(TAG_RECV, "Message sent successfully");
        //    }
        // }
    }
    else
    {
        ESP_LOGI(TAG_RECV, "Nothing!!!");
    }
}

// 定义 send_data 函数
int send_data(const char *data, const char *server_ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;
    int total_sent = 0;
    int data_len = strlen(data);
    int packet_size;
    int ret = ESP_OK;

    // 创建 UDP 套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return SEND_ERROR_SOCKET;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Invalid address/Address not supported");
        close(sockfd);
        return SEND_ERROR_CONNECT;
    }

    // 分包发送数据
    while (total_sent < data_len)
    {
        // 计算当前包的大小
        packet_size = (data_len - total_sent > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : (data_len - total_sent);

        // 发送数据包
        int sent = sendto(sockfd, data + total_sent, packet_size, 0,
                          (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent < 0)
        {
            perror("Send failed");
            ret = SEND_ERROR_SEND;
            break;
        }

        total_sent += sent;
    }

    // 关闭套接字
    close(sockfd);

    // 返回发送结果
    if (ret == ESP_OK && total_sent == data_len)
    {
        printf("Data sent successfully!\n");
    }
    else
    {
        printf("Data sent partially or failed!\n");
    }

    return ret;
}

int csi_data_tools_test()
{
    // 测试数据
    const char *data = "This is a long string that will be split into multiple packets for UDP transmission.";
    const char *server_ip = "127.0.0.1"; // 服务器 IP 地址
    int port = 12345;                    // 服务器端口

    // 调用 send_data 函数
    int result = send_data(data, server_ip, port);

    // 检查发送结果
    if (result == ESP_OK)
    {
        printf("Send operation completed successfully.\n");
    }
    else
    {
        printf("Send operation failed with error code: %d\n", result);
    }

    return 0;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);

        // csi_data_tools_test();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        ESP_LOGI(TAG_STA, "Station started");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Initialize soft AP */
esp_netif_t *wifi_init_softap(void)
{
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0)
    {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

    return esp_netif_ap;
}

/* Initialize wifi station */
esp_netif_t *wifi_init_sta(void)
{
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_STA_SSID,
            .password = EXAMPLE_ESP_WIFI_STA_PASSWD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = EXAMPLE_ESP_MAXIMUM_RETRY,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");

    return esp_netif_sta;
}

void softap_set_dns_addr(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta)
{
    esp_netif_dns_info_t dns;
    esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);
    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option, sizeof(dhcps_offer_option)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
}

static void test_task(void *arg)
{
    int count = 0;
    int inf = recv_multicast_init();
    while (1)
    {
        ESP_LOGI(TAG_RECV, "test count=%d!", ++count);
        recv_csi_data_multicast(inf);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_LOGI(TAG_RECV, "111111\n");

    TaskHandle_t xHandle = NULL;
    BaseType_t xReturned = xTaskCreate(
        test_task,
        "AirSight_Recv_Task",
        4 * 1024, NULL,
        0,
        &xHandle);
    if (xReturned == pdPASS)
    {
        ESP_LOGI(TAG_RECV, "Task created successfully!\n");
    }
    else
    {
        ESP_LOGE(TAG_RECV, "Failed to create task!\n");
    }
    // recv_csi_data_multicast();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize event group */
    s_wifi_event_group = xEventGroupCreate();

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    /*Initialize WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /* Initialize AP */
    ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
    esp_netif_t *esp_netif_ap = wifi_init_softap();

    /* Initialize STA */
    ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
    esp_netif_t *esp_netif_sta = wifi_init_sta();

    /* Start WiFi */
    ESP_ERROR_CHECK(esp_wifi_start());

    /*
     * Wait until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
     * The bits are set by event_handler() (see above)
     */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned,
     * hence we can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG_STA, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
        softap_set_dns_addr(esp_netif_ap, esp_netif_sta);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    }
    else
    {
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        return;
    }

    /* Set sta as the default interface */
    esp_netif_set_default_netif(esp_netif_sta);

    /* Enable napt on the AP netif */
    if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK)
    {
        ESP_LOGE(TAG_STA, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }

    vTaskResume(xHandle);
}
