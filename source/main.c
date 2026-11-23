#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <orbis/libkernel.h>
#include <orbis/Net.h>
#include <orbis/Rtc.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>

#define CONFIG_PATH "/data/ps4-timeSync.ini"
#define MAX_NTP_SERVERS 4
#define MAX_IP_LEN 64
#define NTP_PORT 123
#define NTP_TIMESTAMP_DELTA 2208988800ULL
#define UNIX_TO_PS4_EPOCH_SEC 62135596800ULL

struct ps4_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct ps4_timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

typedef struct {
    int max_retries;
    int notify_mode;
    char server1[MAX_IP_LEN];
    char server2[MAX_IP_LEN];
    char server3[MAX_IP_LEN];
    char server4[MAX_IP_LEN];
} AppConfig;

// PS4 系统原生通知弹窗
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

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)(*str))) str++;
    if (*str == '\0') return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)(*end))) end--;
    *(end + 1) = '\0';
    return str;
}

static void write_default_config(void) {
    FILE *fp = fopen(CONFIG_PATH, "w");
    if (!fp) return;
    fprintf(fp, "; ps4-timeSync PKG 版配置文件\n\n");
    fprintf(fp, "[General]\n");
    fprintf(fp, "MaxRetries=4\n");
    fprintf(fp, "; 弹窗模式: 0 = 静默, 1 = 仅失败提示, 2 = 成功和失败均提示\n");
    fprintf(fp, "NotifyMode=2\n\n");
    fprintf(fp, "[NTP]\n");
    fprintf(fp, "Server1=203.107.6.88\n");
    fprintf(fp, "Server2=119.29.26.206\n");
    fprintf(fp, "Server3=120.25.115.20\n");
    fprintf(fp, "Server4=182.92.12.11\n");
    fclose(fp);
}

static void set_server_ip(char *dest, const char *src) {
    strncpy(dest, src, MAX_IP_LEN - 1);
    dest[MAX_IP_LEN - 1] = '\0';
}

static void load_config(AppConfig *cfg) {
    cfg->max_retries = 4;
    cfg->notify_mode = 2;
    set_server_ip(cfg->server1, "203.107.6.88");
    set_server_ip(cfg->server2, "119.29.26.206");
    set_server_ip(cfg->server3, "120.25.115.20");
    set_server_ip(cfg->server4, "182.92.12.11");

    FILE *fp = fopen(CONFIG_PATH, "r");
    if (!fp) {
        write_default_config();
        return;
    }

    char line_buf[256];
    while (fgets(line_buf, sizeof(line_buf), fp)) {
        char *trimmed = trim_whitespace(line_buf);
        char first = *trimmed;
        if (first == '\0' || first == ';' || first == '#' || first == '[') continue;

        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_whitespace(trimmed);
        char *val = trim_whitespace(eq + 1);

        if (strcmp(key, "MaxRetries") == 0) {
            int v = atoi(val);
            if (v >= 1 && v <= 30) cfg->max_retries = v;
        } else if (strcmp(key, "NotifyMode") == 0) {
            int v = atoi(val);
            if (v >= 0 && v <= 2) cfg->notify_mode = v;
        } else if (strcmp(key, "Server1") == 0) {
            set_server_ip(cfg->server1, val);
        } else if (strcmp(key, "Server2") == 0) {
            set_server_ip(cfg->server2, val);
        } else if (strcmp(key, "Server3") == 0) {
            set_server_ip(cfg->server3, val);
        } else if (strcmp(key, "Server4") == 0) {
            set_server_ip(cfg->server4, val);
        }
    }
    fclose(fp);
}

static uint64_t query_ntp_server(const char *server_ip) {
    if (!server_ip || *server_ip == '\0') return 0;

    OrbisNetId sockfd = sceNetSocket("pkg_ntp_sock", ORBIS_NET_AF_INET, ORBIS_NET_SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) return 0;

    int timeout_usec = 3000000;
    sceNetSetsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout_usec, sizeof(timeout_usec));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_family = ORBIS_NET_AF_INET;
    server_addr.sin_port = sceNetHtons(NTP_PORT);

    if (sceNetInetPton(ORBIS_NET_AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        sceNetSocketClose(sockfd);
        return 0;
    }

    uint8_t packet[48];
    memset(packet, 0, sizeof(packet));
    packet[0] = 0x1B;

    if (sceNetSendto(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sceNetSocketClose(sockfd);
        return 0;
    }

    struct sockaddr_in from_addr;
    OrbisNetSocklen_t from_len = sizeof(from_addr);
    int recv_len = sceNetRecvfrom(sockfd, packet, sizeof(packet), 0, (OrbisNetSockaddr *)&from_addr, &from_len);
    sceNetSocketClose(sockfd);

    if (recv_len < 48) return 0;

    uint32_t ntp_seconds = ((uint32_t)packet[40] << 24) |
                           ((uint32_t)packet[41] << 16) |
                           ((uint32_t)packet[42] << 8)  |
                           ((uint32_t)packet[43]);

    if (ntp_seconds <= NTP_TIMESTAMP_DELTA) return 0;
    return (uint64_t)(ntp_seconds - NTP_TIMESTAMP_DELTA);
}

int main(void) {
    // 隐藏启动画面，直接进入后台执行
    sceSystemServiceHideSplashScreen();

    AppConfig cfg;
    load_config(&cfg);

    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
    sceNetInit();

    const char *server_list[MAX_NTP_SERVERS] = {
        cfg.server1, cfg.server2, cfg.server3, cfg.server4
    };

    uint64_t unix_time = 0;
    for (int retry = 0; retry < cfg.max_retries; retry++) {
        for (int i = 0; i < MAX_NTP_SERVERS; i++) {
            if (server_list[i][0] != '\0') {
                unix_time = query_ntp_server(server_list[i]);
                if (unix_time > 0) break;
            }
        }
        if (unix_time > 0) break;
        sceKernelSleep(2);
    }

    if (unix_time > 0) {
        struct ps4_timeval tv;
        tv.tv_sec = (int64_t)unix_time;
        tv.tv_usec = 0;
        typedef int (*settimeofday_fn)(const struct ps4_timeval *, const struct ps4_timezone *);
        int ret_sys = ((settimeofday_fn)sceKernelSettimeofday)(&tv, NULL);

        typedef int (*rtc_set_tick_fn)(const uint64_t *);
        uint64_t ps4_tick = (unix_time + UNIX_TO_PS4_EPOCH_SEC) * 1000000ULL;
        int ret_rtc = ((rtc_set_tick_fn)sceRtcSetCurrentTick)(&ps4_tick);

        if (ret_sys == 0 || ret_rtc == 0) {
            if (cfg.notify_mode == 2) {
                show_notification("ps4-timeSync: NTP 时间同步成功！");
            }
        } else if (cfg.notify_mode >= 1) {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "ps4-timeSync: 时间写入失败 (sys:%d, rtc:0x%x)", ret_sys, ret_rtc);
            show_notification(err_msg);
        }
    } else if (cfg.notify_mode >= 1) {
        show_notification("ps4-timeSync: 网络超时，NTP 同步失败");
    }

    // 留出 1 秒确保弹窗消息完成投递后自动退出回主界面
    sceKernelSleep(1);
    sceSystemServiceLoadExec("exit", NULL);
    return 0;
}
