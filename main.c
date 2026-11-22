#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include "jailbreak.h"

// 公共 NTP 服务器 IP（绕过 DNS 屏蔽）
#define NTP_SERVER_PRIMARY   "203.107.6.88"    // 阿里云 NTP
#define NTP_SERVER_SECONDARY "119.29.26.206"   // 腾讯云 NTP
#define NTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ULL

struct ps4_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct ps4_timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int sceKernelSettimeofday(const struct ps4_timeval *tv, const struct ps4_timezone *tz);

// PS4 系统原生左上角弹窗通知
static void show_notification(const char *msg) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.unk3 = 0;
    req.useIconImageUri = 0;
    req.targetId = -1;
    strncpy(req.message, msg, sizeof(req.message) - 1);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

// 查询单个 NTP 服务器
static uint64_t query_ntp_server(const char *server_ip) {
    OrbisNetId sockfd = sceNetSocket("pkg_ntp_sock", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_DGRAM, ORBIS_NET_IPPROTO_UDP);
    if (sockfd < 0) return 0;

    int timeout = 3000000;
    sceNetSetsockopt(sockfd, ORBIS_NET_SOL_SOCKET, ORBIS_NET_SO_RCVTIMEO, &timeout, sizeof(timeout));

    OrbisNetSockaddrIn server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_family = ORBIS_NET_AF_INET;
    server_addr.sin_port = sceNetHtons(NTP_PORT);
    sceNetInetPton(ORBIS_NET_AF_INET, server_ip, &server_addr.sin_addr);

    uint8_t packet = {0};
    packet = 0x1B;

    if (sceNetSendto(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sceNetSocketClose(sockfd);
        return 0;
    }

    OrbisNetSockaddrIn from_addr;
    OrbisNetSocklen_t from_len = sizeof(from_addr);
    int recv_len = sceNetRecvfrom(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&from_addr, &from_len);
    sceNetSocketClose(sockfd);

    if (recv_len < 48) return 0;

    uint32_t ntp_seconds = ((uint32_t)packet << 24) |
                           ((uint32_t)packet << 16) |
                           ((uint32_t)packet << 8)  |
                           ((uint32_t)packet);

    if (ntp_seconds <= NTP_TIMESTAMP_DELTA) return 0;
    return (uint64_t)(ntp_seconds - NTP_TIMESTAMP_DELTA);
}

int main(void) {
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceNetInit();

    // 1. 逃逸 PKG 沙盒获取 root 权限（支持 5.05 ~ 11.00 全固件）
    struct jbc_cred cred;
    jbc_get_cred(&cred);
    jbc_jailbreak_cred(&cred);
    jbc_set_cred(&cred);

    show_notification("ps4-timeSync: 正在获取互联网时间...");

    // 2. 获取网络时间
    uint64_t unix_time = query_ntp_server(NTP_SERVER_PRIMARY);
    if (unix_time == 0) {
        unix_time = query_ntp_server(NTP_SERVER_SECONDARY);
    }

    // 3. 写入系统时钟
    if (unix_time > 0) {
        struct ps4_timeval tv;
        tv.tv_sec = (int64_t)unix_time;
        tv.tv_usec = 0;

        int ret = sceKernelSettimeofday(&tv, NULL);
        if (ret == 0) {
            show_notification("ps4-timeSync: 时间同步成功！");
        } else {
            char err_msg;
            snprintf(err_msg, sizeof(err_msg), "时间写入失败 (错误码: %d)", ret);
            show_notification(err_msg);
        }
    } else {
        show_notification("ps4-timeSync: 获取网络时间失败！");
    }

    // 4. 延迟 1.5 秒确保系统通知完整显示后，自动退出返回主界面
    usleep(1500000);
    sceSystemServiceLoadExec("exit", NULL);
    return 0;
}
