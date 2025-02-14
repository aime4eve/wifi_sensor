/**
 * @file udp_multicast_main.c
 * @brief 本文件包含UDP组播发送和接收功能的示例实现。
 * @author [伍志勇]
 * @date 2025-02-13
 *
 * 该示例演示了如何发送和接收UDP组播数据包。可以配置为发送或接收组播消息。
 *
 * 主要功能：
 *   - UDP组播组管理。
 *   - 向组播组发送数据。
 *   - 从组播组接收数据。
 *   - 演示套接字操作的基本错误处理。
 * 
 * 概要设计：
 * 定义一个名为 `socket_add_ipv4_multicast_group` 的静态函数，用于将套接字加入到一个IPv4多播组。
 * 函数接受两个参数：一个套接字描述符 `sock` 和一个布尔值 `assign_source_if`，用于指示是否分配源接口。
 * 首先，函数声明了几个变量，包括：
 * -- `struct ip_mreq imreq` 和 `struct in_addr iaddr`，它们分别用于存储多播请求和接口地址信息。
 * --变量 `err` 用于存储错误代码。
 * 接下来，代码根据条件编译指令 `LISTEN_ALL_IF` 配置源接口。
 * --如果 `LISTEN_ALL_IF` 被定义，`imreq.imr_interface.s_addr` 被设置为 `IPADDR_ANY`，表示监听所有接口。
 * --否则，代码调用 `esp_netif_get_ip_info` 函数获取网络接口的IP信息，并将其存储在 `ip_info` 结构中。
 * --如果获取IP信息失败，函数会记录错误日志并跳转到 `err` 标签处返回错误代码。
 * --然后，使用 `inet_addr_from_ip4addr` 函数将IP地址转换为 `in_addr` 结构。
 * 接下来，代码配置要监听的多播地址。
 * 使用 `inet_aton` 函数将多播地址字符串转换为网络字节序的二进制形式，并存储在 `imreq.imr_multiaddr.s_addr` 中。
 * --如果转换失败，函数会记录错误日志并返回错误代码。然后，代码记录配置的多播地址，并检查该地址是否为有效的多播地址。
 * --如果 `assign_source_if` 为真，代码会调用 `setsockopt` 函数设置 `IP_MULTICAST_IF` 选项，将多播源接口分配给套接字。
 * --如果设置失败，函数会记录错误日志并返回错误代码。
 * 最后，代码调用 `setsockopt` 函数设置 `IP_ADD_MEMBERSHIP` 选项，将套接字加入到多播组。
 * --如果设置失败，函数会记录错误日志并返回错误代码。
 * 函数在 `err` 标签处返回错误代码或成功代码。
 */

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

/* 示例使用简单的配置，可以通过项目配置进行设置。

   如果不想这样做，只需将以下条目更改为带有所需配置的字符串
   例如 #define UDP_PORT 3333
*/
#define UDP_PORT CONFIG_EXAMPLE_PORT

#define MULTICAST_LOOPBACK CONFIG_EXAMPLE_LOOPBACK

#define MULTICAST_TTL CONFIG_EXAMPLE_MULTICAST_TTL

#define MULTICAST_IPV4_ADDR CONFIG_EXAMPLE_MULTICAST_IPV4_ADDR
#define MULTICAST_IPV6_ADDR CONFIG_EXAMPLE_MULTICAST_IPV6_ADDR

#define LISTEN_ALL_IF   EXAMPLE_MULTICAST_LISTEN_ALL_IF

static const char *TAG = "multicast";
#ifdef CONFIG_EXAMPLE_IPV4
static const char *V4TAG = "mcast-ipv4";
#endif
#ifdef CONFIG_EXAMPLE_IPV6
static const char *V6TAG = "mcast-ipv6";
#endif


#ifdef CONFIG_EXAMPLE_IPV4
/* 将套接字（仅限IPV4或IPV6双模式）添加到IPV4组播组 */
static int socket_add_ipv4_multicast_group(int sock, bool assign_source_if)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;
    // 配置源接口
#if LISTEN_ALL_IF
    imreq.imr_interface.s_addr = IPADDR_ANY;
#else
    esp_netif_ip_info_t ip_info = { 0 };
    err = esp_netif_get_ip_info(get_example_netif(), &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(V4TAG, "获取IP地址信息失败。错误 0x%x", err);
        goto err;
    }
    inet_addr_from_ip4addr(&iaddr, &ip_info.ip);
#endif // LISTEN_ALL_IF
    // 配置要监听的组播地址
    err = inet_aton(MULTICAST_IPV4_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(V4TAG, "配置的IPV4组播地址 '%s' 无效。", MULTICAST_IPV4_ADDR);
        // 返回值中的错误必须为负数
        err = -1;
        goto err;
    }
    ESP_LOGI(TAG, "配置的IPV4组播地址 %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(V4TAG, "配置的IPV4组播地址 '%s' 不是有效的组播地址。这可能无法正常工作。", MULTICAST_IPV4_ADDR);
    }

    if (assign_source_if) {
        // 分配IPv4组播源接口，通过其IP
        // （仅当此套接字仅限IPV4时才需要）
        err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
                         sizeof(struct in_addr));
        if (err < 0) {
            ESP_LOGE(V4TAG, "设置 IP_MULTICAST_IF 失败。错误 %d", errno);
            goto err;
        }
    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(V4TAG, "设置 IP_ADD_MEMBERSHIP 失败。错误 %d", errno);
        goto err;
    }

 err:
    return err;
}
#endif /* CONFIG_EXAMPLE_IPV4 */

#ifdef CONFIG_EXAMPLE_IPV4_ONLY
static int create_multicast_ipv4_socket(void)
{
    struct sockaddr_in saddr = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(V4TAG, "创建套接字失败。错误 %d", errno);
        return -1;
    }

    // 将套接字绑定到任意地址
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(UDP_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(V4TAG, "绑定套接字失败。错误 %d", errno);
        goto err;
    }


    // 分配组播TTL（与普通接口TTL分开设置）
    uint8_t ttl = MULTICAST_TTL;
    err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V4TAG, "设置 IP_MULTICAST_TTL 失败。错误 %d", errno);
        goto err;
    }

#if MULTICAST_LOOPBACK
    // 选择是否应由此设备接收组播流量
    // （如果未调用 setsockopt()，默认值为否）
    uint8_t loopback_val = MULTICAST_LOOPBACK;
    err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                     &loopback_val, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V4TAG, "设置 IP_MULTICAST_LOOP 失败。错误 %d", errno);
        goto err;
    }
#endif

    // 这是一个监听套接字，因此将其添加到组播组以进行监听...
    err = socket_add_ipv4_multicast_group(sock, true);
    if (err < 0) {
        goto err;
    }

    // 一切就绪，套接字已配置为发送和接收
    return sock;

err:
    close(sock);
    return -1;
}
#endif /* CONFIG_EXAMPLE_IPV4_ONLY */

#ifdef CONFIG_EXAMPLE_IPV6
static int create_multicast_ipv6_socket(void)
{
    struct sockaddr_in6 saddr = { 0 };
    int  netif_index;
    struct in6_addr if_inaddr = { 0 };
    struct ip6_addr if_ipaddr = { 0 };
    struct ipv6_mreq v6imreq = { 0 };
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
    if (sock < 0) {
        ESP_LOGE(V6TAG, "创建套接字失败。错误 %d", errno);
        return -1;
    }

    // 将套接字绑定到任意地址
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(UDP_PORT);
    bzero(&saddr.sin6_addr.un, sizeof(saddr.sin6_addr.un));
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in6));
    if (err < 0) {
        ESP_LOGE(V6TAG, "绑定套接字失败。错误 %d", errno);
        goto err;
    }

    // 选择要用作此套接字的组播源的接口。
#if LISTEN_ALL_IF
    bzero(&if_inaddr.un, sizeof(if_inaddr.un));
#else
    // 读取接口适配器的链路本地地址并使用它
    // 将组播IF绑定到此套接字。
    //
    // （请注意，接口可能还有其他非LL IPV6地址，
    // 但在此上下文中无关紧要，因为该地址仅用于标识接口。）
    err = esp_netif_get_ip6_linklocal(EXAMPLE_INTERFACE, (esp_ip6_addr_t*)&if_ipaddr);
    inet6_addr_from_ip6addr(&if_inaddr, &if_ipaddr);
    if (err != ESP_OK) {
        ESP_LOGE(V6TAG, "获取IPV6 LL地址失败。错误 0x%x", err);
        goto err;
    }
#endif // LISTEN_ALL_IF

    // 搜索netif索引
    netif_index = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
    if(netif_index < 0) {
        ESP_LOGE(V6TAG, "获取netif索引失败");
        goto err;
    }
    // 分配组播源接口，通过其IP
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &netif_index,sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V6TAG, "设置 IPV6_MULTICAST_IF 失败。错误 %d", errno);
        goto err;
    }

    // 分配组播TTL（与普通接口TTL分开设置）
    uint8_t ttl = MULTICAST_TTL;
    setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V6TAG, "设置 IPV6_MULTICAST_HOPS 失败。错误 %d", errno);
        goto err;
    }

#if MULTICAST_LOOPBACK
    // 选择是否应由此设备接收组播流量
    // （如果未调用 setsockopt()，默认值为否）
    uint8_t loopback_val = MULTICAST_LOOPBACK;
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                     &loopback_val, sizeof(uint8_t));
    if (err < 0) {
        ESP_LOGE(V6TAG, "设置 IPV6_MULTICAST_LOOP 失败。错误 %d", errno);
        goto err;
    }
#endif

    // 这是一个监听套接字，因此将其添加到组播组以进行监听...
#ifdef CONFIG_EXAMPLE_IPV6
    // 配置要监听的组播地址
    err = inet6_aton(MULTICAST_IPV6_ADDR, &v6imreq.ipv6mr_multiaddr);
    if (err != 1) {
        ESP_LOGE(V6TAG, "配置的IPV6组播地址 '%s' 无效。", MULTICAST_IPV6_ADDR);
        goto err;
    }
    ESP_LOGI(TAG, "配置的IPV6组播地址 %s", inet6_ntoa(v6imreq.ipv6mr_multiaddr));
    ip6_addr_t multi_addr;
    inet6_addr_to_ip6addr(&multi_addr, &v6imreq.ipv6mr_multiaddr);
    if (!ip6_addr_ismulticast(&multi_addr)) {
        ESP_LOGW(V6TAG, "配置的IPV6组播地址 '%s' 不是有效的组播地址。这可能无法正常工作。", MULTICAST_IPV6_ADDR);
    }
    // 配置源接口
    v6imreq.ipv6mr_interface = (unsigned int)netif_index;
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
                     &v6imreq, sizeof(struct ipv6_mreq));
    if (err < 0) {
        ESP_LOGE(V6TAG, "设置 IPV6_ADD_MEMBERSHIP 失败。错误 %d", errno);
        goto err;
    }
#endif

#if CONFIG_EXAMPLE_IPV4_V6
    // 添加通用IPV4配置选项
    err = socket_add_ipv4_multicast_group(sock, false);
    if (err < 0) {
        goto err;
    }
#endif

#if CONFIG_EXAMPLE_IPV4_V6
    int only = 0;
#else
    int only = 1; /* 仅限IPV6的套接字 */
#endif
    err = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &only, sizeof(int));
    if (err < 0) {
        ESP_LOGE(V6TAG, "设置 IPV6_V6ONLY 失败。错误 %d", errno);
        goto err;
    }
    ESP_LOGI(TAG, "套接字设置为仅限IPV6");

    // 一切就绪，套接字已配置为发送和接收
    return sock;

err:
    close(sock);
    return -1;
}
#endif

static void mcast_example_task(void *pvParameters)
{
    while (1) {
        int sock;

#ifdef CONFIG_EXAMPLE_IPV4_ONLY
        sock = create_multicast_ipv4_socket();
        if (sock < 0) {
            ESP_LOGE(TAG, "创建IPv4组播套接字失败");
        }
#else
        sock = create_multicast_ipv6_socket();
        if (sock < 0) {
            ESP_LOGE(TAG, "创建IPv6组播套接字失败");
        }
#endif

        if (sock < 0) {
            // 无事可做！
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }

#ifdef CONFIG_EXAMPLE_IPV4
        // 设置用于发送的目标组播地址
        struct sockaddr_in sdestv4 = {
            .sin_family = PF_INET,
            .sin_port = htons(UDP_PORT),
        };
        // 我们知道这个 inet_aton 会通过，因为我们已经在上面做过了
        inet_aton(MULTICAST_IPV4_ADDR, &sdestv4.sin_addr.s_addr);
#endif

#ifdef CONFIG_EXAMPLE_IPV6
        struct sockaddr_in6 sdestv6 = {
            .sin6_family = PF_INET6,
            .sin6_port = htons(UDP_PORT),
        };
        // 我们知道这个 inet_aton 会通过，因为我们已经在上面做过了
        inet6_aton(MULTICAST_IPV6_ADDR, &sdestv6.sin6_addr);
#endif

        // 循环等待UDP接收，如果没有看到任何数据包则发送UDP数据包。
        int err = 1;
        while (err > 0) {
            struct timeval tv = {
                .tv_sec = 2,
                .tv_usec = 0,
            };
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            int s = select(sock + 1, &rfds, NULL, NULL, &tv);
            if (s < 0) {
                ESP_LOGE(TAG, "选择失败：错误号 %d", errno);
                err = -1;
                break;
            }
            else if (s > 0) {
                if (FD_ISSET(sock, &rfds)) {
                    // 收到传入的数据报
                    char recvbuf[48];
                    char raddr_name[32] = { 0 };

                    struct sockaddr_storage raddr; // 足够大以容纳IPv4或IPv6
                    socklen_t socklen = sizeof(raddr);
                    int len = recvfrom(sock, recvbuf, sizeof(recvbuf)-1, 0,
                                       (struct sockaddr *)&raddr, &socklen);
                    if (len < 0) {
                        ESP_LOGE(TAG, "组播 recvfrom 失败：错误号 %d", errno);
                        err = -1;
                        break;
                    }

                    // 将发送者的地址转换为字符串
#ifdef CONFIG_EXAMPLE_IPV4
                    if (raddr.ss_family == PF_INET) {
                        inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr,
                                    raddr_name, sizeof(raddr_name)-1);
                    }
#endif
#ifdef CONFIG_EXAMPLE_IPV6
                    if (raddr.ss_family== PF_INET6) {
                        inet6_ntoa_r(((struct sockaddr_in6 *)&raddr)->sin6_addr, raddr_name, sizeof(raddr_name)-1);
                    }
#endif
                    ESP_LOGI(TAG, "从 %s 收到 %d 字节：", raddr_name, len);

                    recvbuf[len] = 0; // 将接收到的数据终止并视为字符串...
                    ESP_LOGI(TAG, "%s", recvbuf);
                }
            }
            else { // s == 0
                // 超时没有收到数据，因此发送一些数据！
                static int send_count;
                const char sendfmt[] = "Multicast #%d sent by ESP32S3 AirSight.\n";
                char sendbuf[48];
                char addrbuf[32] = { 0 };
                int len = snprintf(sendbuf, sizeof(sendbuf), sendfmt, send_count++);
                if (len > sizeof(sendbuf)) {
                    ESP_LOGE(TAG, "组播发送格式缓冲区溢出！！");
                    send_count = 0;
                    err = -1;
                    break;
                }

                struct addrinfo hints = {
                    .ai_flags = AI_PASSIVE,
                    .ai_socktype = SOCK_DGRAM,
                };
                struct addrinfo *res;

#ifdef CONFIG_EXAMPLE_IPV4 // 发送IPv4组播数据包

#ifdef CONFIG_EXAMPLE_IPV4_ONLY
                hints.ai_family = AF_INET; // 对于IPv4套接字
#else

#ifdef CONFIG_ESP_NETIF_TCPIP_LWIP  // 仅在官方TCPIP_LWIP堆栈（esp-lwip）中支持解析IPv4映射的IPv6地址
                hints.ai_family = AF_INET6; // 对于带有V4映射地址的IPv4套接字
                hints.ai_flags |= AI_V4MAPPED;
#endif // CONFIG_ESP_NETIF_TCPIP_LWIP

#endif
                int err = getaddrinfo(CONFIG_EXAMPLE_MULTICAST_IPV4_ADDR,
                                      NULL,
                                      &hints,
                                      &res);
                if (err < 0) {
                    ESP_LOGE(TAG, "getaddrinfo() 获取IPV4目标地址失败。错误：%d", err);
                    break;
                }
                if (res == 0) {
                    ESP_LOGE(TAG, "getaddrinfo() 未返回任何地址");
                    break;
                }
#ifdef CONFIG_EXAMPLE_IPV4_ONLY
                ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(UDP_PORT);
                inet_ntoa_r(((struct sockaddr_in *)res->ai_addr)->sin_addr, addrbuf, sizeof(addrbuf)-1);
                ESP_LOGI(TAG, "发送到IPV4组播地址 %s:%d...",  addrbuf, UDP_PORT);
#else
                ((struct sockaddr_in6 *)res->ai_addr)->sin6_port = htons(UDP_PORT);
                inet6_ntoa_r(((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addrbuf, sizeof(addrbuf)-1);
                ESP_LOGI(TAG, "发送到IPV6 (V4 mapped) 组播地址 %s port %d (%s)...",  addrbuf, UDP_PORT, CONFIG_EXAMPLE_MULTICAST_IPV4_ADDR);
#endif
                err = sendto(sock, sendbuf, len, 0, res->ai_addr, res->ai_addrlen);
                freeaddrinfo(res);
                if (err < 0) {
                    ESP_LOGE(TAG, "IPV4 sendto 失败。错误号：%d", errno);
                    break;
                }
#endif
#ifdef CONFIG_EXAMPLE_IPV6
                hints.ai_family = AF_INET6;
                hints.ai_protocol = 0;
                err = getaddrinfo(CONFIG_EXAMPLE_MULTICAST_IPV6_ADDR,
                                  NULL,
                                  &hints,
                                  &res);
                if (err < 0) {
                    ESP_LOGE(TAG, "getaddrinfo() 获取IPV6目标地址失败。错误：%d", err);
                    break;
                }

                struct sockaddr_in6 *s6addr = (struct sockaddr_in6 *)res->ai_addr;
                s6addr->sin6_port = htons(UDP_PORT);
                inet6_ntoa_r(s6addr->sin6_addr, addrbuf, sizeof(addrbuf)-1);
                ESP_LOGI(TAG, "发送到IPV6组播地址 %s port %d...",  addrbuf, UDP_PORT);
                err = sendto(sock, sendbuf, len, 0, res->ai_addr, res->ai_addrlen);
                freeaddrinfo(res);
                if (err < 0) {
                    ESP_LOGE(TAG, "IPV6 sendto 失败。错误号：%d", errno);
                    break;
                }
#endif
            }
        }

        ESP_LOGE(TAG, "关闭套接字并重新启动...");
        shutdown(sock, 0);
        close(sock);
    }

}

void app_main(void)
{
    // 初始化NVS闪存
    ESP_ERROR_CHECK(nvs_flash_init());
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 这里用了乐鑫官网的示例代码，用于连接Wi-Fi或以太网。 
     * 这个辅助函数根据menuconfig中的选择配置Wi-Fi或以太网。
     * 阅读 examples/protocols/README.md 中的 "Establishing Wi-Fi or Ethernet Connection" 部分以获取更多信息。
     * 
     * ToDo：修改为自己的Wi-Fi或以太网连接代码。
     */
    ESP_ERROR_CHECK(example_connect());

    // 创建并启动组播示例任务
    xTaskCreate(&mcast_example_task, "mcast_task", 4096, NULL, 5, NULL);
}
