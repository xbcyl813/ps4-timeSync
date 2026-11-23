#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Rtc.h>
#include <orbis/Sysmodule.h>
#include <GoldHEN.h>

// 插件元信息声明
__attribute__((visibility("default"))) const char *g_pluginName = "ps4-timeSync";
__attribute__((visibility("default"))) const char *g_pluginDesc = "Auto sync PS4 system clock via NTP";
__attribute__((visibility("default"))) const char *g_pluginAuth = "Homebrew";
__attribute__((visibility("default"))) uint32_t g_pluginVersion = 0x00000101; // v1.01

// 公共 NTP 服务器 IP 列表（多节点兜底）
static const char *NTP_SERVERS[] = {
    "203.107.6.88",   // 阿里云 NTP
    "119.29.26.206",  // 腾讯云 NTP
    "120.25.115.20",  // 阿里云备用 NTP
    "182.92.12.11"    // 阿里云华北 NTP
};
#define NTP_SERVER_COUNT (sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]))

#define NTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ULL
// PS4 RTC 纪元起点为 0001-01-01，与 1970-01-01 相差 62135596800 秒
#define UNIX_TO_PS4_EPOCH_SEC 62135596800ULL

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
    OrbisNetId sockfd = sceNetSocket("gh_ntp_sock", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_DGRAM, ORBIS_NET_IPPROTO_UDP);
    if (sockfd < 0) {
        return 0;
    }

    // 修复：必须使用 OrbisNetTimeval 结构体设置超时
    OrbisNetTimeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    sceNetSetsockopt(sockfd, ORBIS_NET_SOL_SOCKET, ORBIS_NET_SO_RCVTIMEO, &timeout, sizeof(timeout));

    OrbisNetSockaddrIn server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_family = ORBIS_NET_AF_INET;
    server_addr.sin_port = sceNetHtons(NTP_PORT);
    sceNetInetPton(ORBIS_NET_AF_INET, server_ip, &server_addr.sin_addr);

    uint8_t packet[48] = {0};
    packet[0] = 0x1B; // LI=0, VN=3, Mode=3 (Client)

    if (sceNetSendto(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sceNetSocketClose(sockfd);
        return 0;
    }

    OrbisNetSockaddrIn from_addr;
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

// 后台同步线程（使用 PS4 原生 ScePthread）
static void *ntp_sync_thread(void *arg) {
    // 初始等待 5 秒，留出系统启动和连接 Wi-Fi 的时间
    sceKernelSleep(5);

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_RTC);
    sceNetInit();

    uint64_t unix_time = 0;

    // 最多重试 6 轮，防止开机联网慢导致首次失败
    for (int retry = 0; retry < 6; retry++) {
        for (size_t i = 0; i < NTP_SERVER_COUNT; i++) {
            unix_time = query_ntp_server(NTP_SERVERS[i]);
            if (unix_time > 0) {
                break;
            }
        }
        if (unix_time > 0) {
            break;
        }
        sceKernelSleep(3);
    }

    if (unix_time > 0) {
        struct ps4_timeval tv;
        tv.tv_sec = (int64_t)unix_time;
        tv.tv_usec = 0;

        typedef int (*settimeofday_fn)(const struct ps4_timeval *, const struct ps4_timezone *);
        int ret_sys = ((settimeofday_fn)sceKernelSettimeofday)(&tv, NULL);

        // 尝试同步更新底层 RTC 时钟（单位为微秒）
        OrbisRtcTick rtc_tick;
        rtc_tick.tick = (unix_time + UNIX_TO_PS4_EPOCH_SEC) * 1000000ULL;
        int ret_rtc = sceRtcSetCurrentTick(&rtc_tick);

        if (ret_sys == 0 || ret_rtc == 0) {
            show_notification("ps4-timeSync: NTP 时间同步成功！");
        } else {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "ps4-timeSync: 写入失败 (sys:%d, rtc:0x%x)", ret_sys, ret_rtc);
            show_notification(err_msg);
        }
    } else {
        show_notification("ps4-timeSync: 网络超时，NTP 同步失败");
    }

    return NULL;
}

// GoldHEN 插件入口函数
__attribute__((visibility("hidden"))) int module_start(size_t argc, const void *args) {
    ScePthread thread;
    scePthreadCreate(&thread, NULL, ntp_sync_thread, NULL, "ntp_sync_thread");
    scePthreadDetach(thread);
    return 0;
}

// GoldHEN 插件卸载函数
__attribute__((visibility("hidden"))) int module_stop(size_t argc, const void *args) {
    return 0;
}
