#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <GoldHEN.h>

// 插件元信息声明
__attribute__((visibility("default"))) const char *g_pluginName = "ps4-timeSync";
__attribute__((visibility("default"))) const char *g_pluginDesc = "Auto sync PS4 system clock via NTP";
__attribute__((visibility("default"))) const char *g_pluginAuth = "Homebrew";
__attribute__((visibility("default"))) uint32_t g_pluginVersion = 0x00000100; // v1.00

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
    OrbisNetId sockfd = sceNetSocket("gh_ntp_sock", 2, 2, 17);
    if (sockfd < 0) {
        return 0;
    }

    int timeout = 3000000;
    sceNetSetsockopt(sockfd, 0xffff, 0x1006, &timeout, sizeof(timeout));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_family = 2; // AF_INET
    server_addr.sin_port = sceNetHtons(NTP_PORT);
    sceNetInetPton(2, server_ip, &server_addr.sin_addr);

    uint8_t packet[48] = {0};
    packet[0] = 0x1B; // LI=0, VN=3, Mode=3 (Client)

    if (sceNetSendto(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sceNetSocketClose(sockfd);
        return 0;
    }

    struct sockaddr_in from_addr;
    OrbisNetSocklen_t from_len = sizeof(from_addr);
    int recv_len = sceNetRecvfrom(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&from_addr, &from_len);
    sceNetSocketClose(sockfd);

    if (recv_len < 48) {
        return 0;
    }

    uint32_t ntp_seconds = ((uint32_t)packet[40] << 24) |
                           ((uint32_t)packet[41] << 16) |
                           ((uint32_t)packet[42] << 8)  |
                           ((uint32_t)packet[43]);

    if (ntp_seconds <= NTP_TIMESTAMP_DELTA) {
        return 0;
    }

    return (uint64_t)(ntp_seconds - NTP_TIMESTAMP_DELTA);
}

// 后台同步线程
static void *ntp_sync_thread(void *arg) {
    sceKernelSleep(3);

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceNetInit();

    uint64_t unix_time = query_ntp_server(NTP_SERVER_PRIMARY);
    if (unix_time == 0) {
        unix_time = query_ntp_server(NTP_SERVER_SECONDARY);
    }

    if (unix_time > 0) {
        struct ps4_timeval tv;
        tv.tv_sec = (int64_t)unix_time;
        tv.tv_usec = 0;

        typedef int (*settimeofday_fn)(const struct ps4_timeval *, const struct ps4_timezone *);
        int ret = ((settimeofday_fn)sceKernelSettimeofday)(&tv, NULL);

        if (ret == 0) {
            show_notification("ps4-timeSync: NTP 时间自动同步成功！");
        } else {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "ps4-timeSync: 时间写入失败 (错误码: %d)", ret);
            show_notification(err_msg);
        }
    }

    return NULL;
}

// GoldHEN 插件入口函数
__attribute__((visibility("hidden"))) int module_start(size_t argc, const void *args) {
    pthread_t thread;
    pthread_create(&thread, NULL, ntp_sync_thread, NULL);
    pthread_detach(thread);
    return 0;
}

// GoldHEN 插件卸载函数
__attribute__((visibility("hidden"))) int module_stop(size_t argc, const void *args) {
    return 0;
}
