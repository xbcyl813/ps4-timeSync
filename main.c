#include "ps4.h"

// 备用公共 NTP 服务器 IP（直接使用 IP 绕过破解机的 DNS 屏蔽）
#define NTP_SERVER_PRIMARY   "203.107.6.88"    // 阿里云 NTP
#define NTP_SERVER_SECONDARY "119.29.26.206"   // 腾讯云 NTP
#define NTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ULL      // 1900 到 1970 年的秒数差

// 声明 PS4 系统内核原生时间结构体与函数（全固件通用，零内核偏移风险）
struct ps4_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct ps4_timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int sceKernelSettimeofday(const struct ps4_timeval *tv, const struct ps4_timezone *tz);

// 向指定 NTP 服务器 IP 请求时间（成功返回 Unix 时间戳，失败返回 0）
uint64_t query_ntp_server(const char *server_ip) {
    int sockfd;
    struct sockaddr_in server_addr;
    uint8_t packet[48] = {0};

    // NTP 请求头：LI=0, VN=3 (IPv4), Mode=3 (Client) -> 0x1B
    packet[0] = 0x1B;

    // 创建 UDP Socket (AF_INET=2, SOCK_DGRAM=2, IPPROTO_UDP=17)
    sockfd = sceNetSocket("ntp_client", 2, 2, 17);
    if (sockfd < 0) {
        return 0;
    }

    // 设置 3 秒接收超时（3,000,000 微秒），避免断网时卡死
    int timeout = 3000000;
    sceNetSetsockopt(sockfd, 0xffff, 0x1006, &timeout, sizeof(timeout));

    // 配置服务器 IP 与端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = 2; // AF_INET
    server_addr.sin_port = sceNetHtons(NTP_PORT);
    sceNetInetPton(2, server_ip, &server_addr.sin_addr);

    // 发送请求包
    if (sceNetSendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sceNetSocketClose(sockfd);
        return 0;
    }

    // 接收响应包
    struct sockaddr_in from_addr;
    uint32_t from_len = sizeof(from_addr);
    int recv_len = sceNetRecvfrom(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&from_addr, &from_len);
    sceNetSocketClose(sockfd);

    if (recv_len < 48) {
        return 0;
    }

    // 解析第 40~43 字节（大端序整数秒）
    uint32_t ntp_seconds = ((uint32_t)packet[40] << 24) |
                           ((uint32_t)packet[41] << 16) |
                           ((uint32_t)packet[42] << 8)  |
                           ((uint32_t)packet[43]);

    if (ntp_seconds <= NTP_TIMESTAMP_DELTA) {
        return 0;
    }

    return (uint64_t)(ntp_seconds - NTP_TIMESTAMP_DELTA);
}

int _main(struct thread *td) {
    UNUSED(td);

    // 1. 初始化基础系统库与网络
    initKernel();
    initLibc();
    initNetwork();
    initSysUtil();

    printf_notification("正在获取互联网 NTP 时间...");

    // 2. 优先尝试阿里云 NTP，失败自动切换腾讯云 NTP
    uint64_t unix_time = query_ntp_server(NTP_SERVER_PRIMARY);
    if (unix_time == 0) {
        unix_time = query_ntp_server(NTP_SERVER_SECONDARY);
    }

    // 3. 写入 PS4 系统时钟
    if (unix_time > 0) {
        struct ps4_timeval tv;
        tv.tv_sec = (int64_t)unix_time;
        tv.tv_usec = 0;

        // 优先直接调用系统原生时间设置函数（GoldHEN 环境下默认已具备权限）
        int ret = sceKernelSettimeofday(&tv, NULL);

        // 若返回非 0（如使用未自动提权的加载器），再执行越狱提权后重试
        if (ret != 0) {
            jailbreak();
            ret = sceKernelSettimeofday(&tv, NULL);
        }

        if (ret == 0) {
            char msg[128];
            sprintf(msg, "PS4 时间同步成功！\nUTC 时间戳: %lu", unix_time);
            printf_notification(msg);
        } else {
            printf_notification("时间获取成功，但写入系统时钟失败！");
        }
    } else {
        printf_notification("NTP 时间获取失败！\n请检查 PS4 网络连接。");
    }

    return 0;
}
