#include "csi_data_sender.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include <netinet/in.h>

#define ECHO_SERVER_PORT 3333

static const char *TAG = "EchoCSI";

int echo_csi_data(const char *data)
{

    esp_netif_ip_info_t local_ip;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
    ESP_LOGI(TAG, "got ip:" IPSTR ", gw: " IPSTR, IP2STR(&local_ip.ip), IP2STR(&local_ip.gw));
    char ip_str[16];
    esp_ip4addr_ntoa((const esp_ip4_addr_t*)&local_ip.gw.addr, ip_str, sizeof(ip_str));

    ESP_LOGI(TAG, "%s Echo CSI data: %s", ip_str, data);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        return -1;
    }

    strcpy(ip_str, "192.168.3.173");


    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(6666);

    int err = sendto(sock, data, strlen(data), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    }
    close(sock);

    return 0;
}