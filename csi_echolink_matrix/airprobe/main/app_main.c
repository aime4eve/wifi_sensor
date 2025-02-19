/* Get recv router csi

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"

#include "esp_mac.h"
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_chip_info.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

#include "protocol_examples_common.h"

#include "csi_data_tools.h"

#define CONFIG_SEND_FREQUENCY 100

static const char *TAG = "AirProbe";

/**
 * @brief CSI 回调函数，当接收到 CSI 数据时被调用
 *
 * @param ctx 用户上下文，这里是 AP 的 BSSID
 * @param info CSI 信息结构体指针
 */
static void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info)
{
    // 检查 info 和 info->buf 是否为空
    if (!info || !info->buf)
    {
        ESP_LOGW(TAG, "<%s> wifi_csi_cb", esp_err_to_name(ESP_ERR_INVALID_ARG));
        return;
    }

    // 检查接收到的 CSI 数据的 MAC 地址是否与期望的 MAC 地址匹配
    if (memcmp(info->mac, ctx, 6))
    {
        return;
    }

    // static int s_count = 0; // 静态计数器，用于记录接收到的 CSI 数据包的数量
    static int s_version = 0;                           // 静态芯片版本号
    const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl; // 指向接收控制信息的指针

    // 打印 CSI 数据的头部信息，只在第一次接收到数据时打印
    // if (!s_count)
    if (!s_version)
    {
        ESP_LOGI(TAG, "================ CSI RECV ================");
        ets_printf("type,seq,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,len,first_word,data\n");
        esp_chip_info_t Esp_Info;
        esp_chip_info(&Esp_Info);
        s_version = Esp_Info.revision;
    }

    /** Only LLTF sub-carriers are selected. */
    info->len = 128; // 设置 CSI 数据的长度为 128

    char csi_values[1024]; // 假设1024足够长来存储整个字符串
    char server_mac_str[18];
    // 格式化 CSI 数据为字符串
    int snprintf_result = snprintf(csi_values, sizeof(csi_values), "CSI_DATA,%d," MACSTR ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                                   s_version, // s_count++,
                                   MAC2STR(info->mac), rx_ctrl->rssi, rx_ctrl->rate, rx_ctrl->sig_mode,
                                   rx_ctrl->mcs, rx_ctrl->cwb, rx_ctrl->smoothing, rx_ctrl->not_sounding,
                                   rx_ctrl->aggregation, rx_ctrl->stbc, rx_ctrl->fec_coding, rx_ctrl->sgi,
                                   rx_ctrl->noise_floor, rx_ctrl->ampdu_cnt, rx_ctrl->channel, rx_ctrl->secondary_channel,
                                   rx_ctrl->timestamp, rx_ctrl->ant, rx_ctrl->sig_len, rx_ctrl->rx_state);
    snprintf(server_mac_str, sizeof(server_mac_str), MACSTR, MAC2STR(info->mac));

    // 检查 snprintf 是否成功，并且没有发生缓冲区溢出
    if (snprintf_result >= 0 && snprintf_result < sizeof(csi_values))
    {
        int current_length = snprintf_result;
        // 添加 CSI 数据的长度、第一个无效字和第一个 CSI 值到字符串
        snprintf_result = snprintf(csi_values + current_length, sizeof(csi_values) - current_length, ",%d,%d,\"[%d", info->len, info->first_word_invalid, info->buf[0]);

        // 再次检查 snprintf 是否成功，并且没有发生缓冲区溢出
        if (snprintf_result >= 0 && current_length + snprintf_result < sizeof(csi_values))
        {
            current_length += snprintf_result;
            // 循环添加剩余的 CSI 值到字符串
            for (int i = 1; i < info->len; i++)
            {
                snprintf_result = snprintf(csi_values + current_length, sizeof(csi_values) - current_length, ",%d", info->buf[i]);
                if (snprintf_result >= 0 && current_length + snprintf_result < sizeof(csi_values))
                {
                    current_length += snprintf_result;
                }
                else
                {
                    ESP_LOGE(TAG, "snprintf error or buffer overflow in loop");
                    break;
                }
            }

            // 添加字符串的结束符号
            snprintf_result = snprintf(csi_values + current_length, sizeof(csi_values) - current_length, "]\"");
            if (snprintf_result >= 0 && current_length + snprintf_result < sizeof(csi_values))
            {
                // printf("%s", csi_values);
                echo_csi_data(csi_values); // 调用函数发送 CSI 数据
                // echo_csi_data_mcast(csi_values);// 用组播方式发送 CSI 数据
            }
            else
            {
                ESP_LOGE(TAG, "snprintf error or buffer overflow at the end");
            }
        }
        else
        {
            ESP_LOGE(TAG, "snprintf error or buffer overflow after first snprintf");
        }
    }
    else
    {
        ESP_LOGE(TAG, "snprintf error or buffer overflow at the beginning");
    }
}

static void wifi_csi_init()
{
    /**
     * @brief In order to ensure the compatibility of routers, only LLTF sub-carriers are selected.
     */
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = false,
        .stbc_htltf2_en = false,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = true,
        .shift = true,
    };

    static wifi_ap_record_t s_ap_info = {0};
    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&s_ap_info));
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_rx_cb, s_ap_info.bssid));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));
}

static esp_err_t wifi_ping_router_start()
{
    static esp_ping_handle_t ping_handle = NULL;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.count = 0;
    ping_config.interval_ms = 1000 / CONFIG_SEND_FREQUENCY;
    ping_config.task_stack_size = 3072;
    ping_config.data_size = 1;

    esp_netif_ip_info_t local_ip;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
    ESP_LOGI(TAG, "got ip:" IPSTR ", gw: " IPSTR, IP2STR(&local_ip.ip), IP2STR(&local_ip.gw));
    ping_config.target_addr.u_addr.ip4.addr = ip4_addr_get_u32(&local_ip.gw);
    ping_config.target_addr.type = ESP_IPADDR_TYPE_V4;

    esp_ping_callbacks_t cbs = {0};
    esp_ping_new_session(&ping_config, &cbs, &ping_handle);
    esp_err_t ping_start_result = esp_ping_start(ping_handle);

    if (ping_start_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Ping start failed, reconnecting Wi-Fi...");
        // esp_wifi_disconnect();
        // esp_wifi_connect();
        // wifi_csi_init();
        // vTaskDelay(3000 / portTICK_PERIOD_MS);
        // wifi_ping_router_start();
    }

    return ESP_OK;
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /**
     * @brief This helper function configures Wi-Fi, as selected in menuconfig.
     *        Read "Establishing Wi-Fi Connection" section in esp-idf/examples/protocols/README.md
     *        for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    wifi_csi_init();
    wifi_ping_router_start();
}