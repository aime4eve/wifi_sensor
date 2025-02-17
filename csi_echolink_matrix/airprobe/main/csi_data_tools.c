#include "csi_data_tools.h"
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"


#define MULTICAST_IPV4_ADDR "232.10.11.12"
#define ECHO_SERVER_PORT 3333
#define MULTICAST_TTL 1

static const char *TAG = "EchoCSI";



static int esp_join_multicast_group(int sockfd)
{
   struct ip_mreq imreq = { 0 };
   struct in_addr iaddr = { 0 };
   int err = 0;
   
   // 配置组播报文发送的接口
   esp_netif_ip_info_t ip_info = { 0 };
   err = esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
   if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to get IP address info. Error 0x%x", err);
      goto err;
   }
   inet_addr_from_ip4addr(&iaddr, &ip_info.ip);
   err = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                     sizeof(struct in_addr));
   if (err < 0) {
      ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
      goto err;
   }

   // 配置监听的组播组地址
   inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);

   // 配置套接字加入组播组
   err = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        &imreq, sizeof(struct ip_mreq));
   if (err < 0) {
      ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
   }

err:
   return err;
}

// static esp_err_t esp_send_multicast(void)
static int echo_csi_data_mcast(const char *data)
{
   esp_err_t err = ESP_FAIL;
   struct sockaddr_in saddr = {0};
   struct sockaddr_in from_addr = {0};
   socklen_t from_addr_len      = sizeof(struct sockaddr_in);
   char udp_recv_buf[64 + 1] = {0};

   // 创建 IPv4 UDP 套接字
   int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd == -1) {
      ESP_LOGE(TAG, "Create UDP socket fail");
      return err;
   }

   // 绑定套接字
   saddr.sin_family = PF_INET;
   saddr.sin_port = htons(ECHO_SERVER_PORT);
   saddr.sin_addr.s_addr = htonl(INADDR_ANY);
   int ret = bind(sockfd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
   if (ret < 0) {
      ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
      goto exit;
   }

   // 设置组播 TTL 为 1，表示该组播包只能经由一个路由
   uint8_t ttl = 1;
   ret = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
   if (ret < 0) {
      ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
      goto exit;
   }

   // 加入组播组
   ret = esp_join_multicast_group(sockfd);
   if (ret < 0) {
      ESP_LOGE(TAG, "Failed to join multicast group");
      goto exit;
   }

   // 设置组播目的地址和端口
   struct sockaddr_in dest_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(ECHO_SERVER_PORT),
   };
   inet_aton(MULTICAST_IPV4_ADDR, &dest_addr.sin_addr.s_addr);

//    char *multicast_msg_buf = "Are you Espressif IOT Smart Light";

   // 调用 sendto 接口发送组播数据
//    ret = sendto(sockfd, multicast_msg_buf, strlen(multicast_msg_buf), 0, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
   ret = sendto(sockfd, data, strlen(data), 0, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
   if (ret < 0) {
      ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
   } else {
      ESP_LOGI(TAG, "Message sent #%d successfully!", strlen(data));
      ret = recvfrom(sockfd, udp_recv_buf, sizeof(udp_recv_buf) - 1, 0, (struct sockaddr *)&from_addr, (socklen_t *)&from_addr_len);
      if (ret > 0) {
         ESP_LOGI(TAG, "Receive udp unicast from %s:%d, data is %s", inet_ntoa(((struct sockaddr_in *)&from_addr)->sin_addr), ntohs(((struct sockaddr_in *)&from_addr)->sin_port), udp_recv_buf);
         err = ESP_OK;
      }
   }

exit:
   close(sockfd);
   return err;
}

static int recv_csi_data_multicast(void)
{
   esp_err_t err = ESP_FAIL;
   struct sockaddr_in saddr = {0};
   struct sockaddr_in from_addr = {0};
   socklen_t from_addr_len      = sizeof(struct sockaddr_in);
   char udp_server_buf[64 + 1] = {0};
   char *udp_server_send_buf = "ESP32-C3 Smart Light https 443";

   // 创建 IPv4 UDP 套接字
   int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd == -1) {
      ESP_LOGE(TAG, "Create UDP socket fail");
      return err;
   }

   // 绑定套接字
   saddr.sin_family = PF_INET;
   saddr.sin_port = htons(ECHO_SERVER_PORT);
   saddr.sin_addr.s_addr = htonl(INADDR_ANY);
   int ret = bind(sockfd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
   if (ret < 0) {
      ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
      goto exit;
   }

   // 设置组播 TTL 为 1，表示该组播包只能经由一个路由
   uint8_t ttl = 1;
   ret = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
   if (ret < 0) {
      ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
      goto exit;
   }

   // 加入组播组
   ret = esp_join_multicast_group(sockfd);
   if (ret < 0) {
      ESP_LOGE(TAG, "Failed to join multicast group");
      goto exit;
   }

   // 调用 recvfrom 接口接收组播数据
   while (1) {
      ret = recvfrom(sockfd, udp_server_buf, sizeof(udp_server_buf) - 1, 0, (struct sockaddr *)&from_addr, (socklen_t *)&from_addr_len);
      if (ret > 0) {
         ESP_LOGI(TAG, "Receive udp multicast from %s:%d, data is %s", inet_ntoa(((struct sockaddr_in *)&from_addr)->sin_addr), ntohs(((struct sockaddr_in *)&from_addr)->sin_port), udp_server_buf);
         // 如果收到组播请求数据，单播发送对端数据通信应用端口
         if (!strcmp(udp_server_buf, "Are you Espressif IOT Smart Light")) {
            ret = sendto(sockfd, udp_server_send_buf, strlen(udp_server_send_buf), 0, (struct sockaddr *)&from_addr, from_addr_len);
            if (ret < 0) {
               ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            } else {
               ESP_LOGI(TAG, "Message sent successfully");
            }
         }
      }
   }

exit:
   close(sockfd);
   return err;
}

esp_err_t echo_csi_data(const char *data)
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

    // strcpy(ip_str, "192.168.3.173");


    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(ECHO_SERVER_PORT);

    int err = sendto(sock, data, strlen(data), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    }
    close(sock);

    return 0;
}