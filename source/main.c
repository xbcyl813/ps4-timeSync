#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include "jailbreak.h"

#define NTP_TIMESTAMP_DELTA 2208988800ULL

typedef struct {
    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t rootDelay;
    uint32_t rootDispersion;
    uint32_t refId;
    uint32_t refTm_s;
    uint32_t refTm_f;
    uint32_t origTm_s;
    uint32_t origTm_f;
    uint32_t rxTm_s;
    uint32_t rxTm_f;
    uint32_t txTm_s;
    uint32_t txTm_f;
} ntp_packet;

// 屏幕左上角发送 PS4 系统通知
void send_notification(const char *msg) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.unk3 = 0;
    req.useIconImageUri = 1;
    req.targetId = -1;
    snprintf(req.message, sizeof(req.message), "%s", msg);
    snprintf(req.iconUri, sizeof(req.iconUri), "cxml://psnotification/tex_icon_system");
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

// 通过 UDP 请求指定 NTP 服务器获取时间戳
int query_ntp_server(const char *ip, uint32_t *out_unix_time) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) return -1;

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(123);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    ntp_packet packet;
    memset(&packet, 0, sizeof(ntp_packet));
    packet.li_vn_mode = (0 << 6) | (3 << 3) | 3; // LI=0, VN=3, Mode=3 (Client)

    if (sendto(sockfd, &packet, sizeof(ntp_packet), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -2;
    }

    socklen_t addr_len = sizeof(serv_addr);
    if (recvfrom(sockfd, &packet, sizeof(ntp_packet), 0, (struct sockaddr *)&serv_addr, &addr_len) < 0) {
        close(sockfd);
        return -3;
    }

    close(sockfd);
    uint32_t txTm = ntohl(packet.txTm_s);
    if (txTm < NTP_TIMESTAMP_DELTA) return -4;

    *out_unix_time = txTm - NTP_TIMESTAMP_DELTA;
    return 0;
}

int main(void) {
    // 1. 隐藏启动画面
    sceSystemServiceHideSplashScreen();

    // 2. 通过 ps4-libjbc 进行沙盒逃逸与获取内核读写权限（允许修改系统时间）
    struct jbc_cred cred;
    jbc_get_cred(&cred);
    jbc_jailbreak_cred(&cred);
    cred.sceProcType = 0x3800000000000010; // System/Debugger 权限标识
    jbc_set_cred(&cred);

    // 3. 双节点容灾请求 NTP 时间（优先阿里云，失败切换腾讯云）
    uint32_t unix_time = 0;
    int ret = query_ntp_server("203.107.6.88", &unix_time);
    if (ret != 0) {
        ret = query_ntp_server("119.29.26.206", &unix_time);
    }

    // 4. 写入底层时钟并发送弹窗提示
    if (ret == 0 && unix_time > 1700000000) {
        OrbisKernelTimeval tv;
        tv.tv_sec = unix_time;
        tv.tv_usec = 0;
        int set_ret = sceKernelSettimeofday(&tv, NULL);
        if (set_ret == 0) {
            send_notification("ps4-timeSync: 时间同步成功！");
        } else {
            char err_buf[128];
            snprintf(err_buf, sizeof(err_buf), "ps4-timeSync: 写入时间失败 (0x%08X)", set_ret);
            send_notification(err_buf);
        }
    } else {
        send_notification("ps4-timeSync: 网络超时，无法连接 NTP 服务器");
    }

    // 5. 等待 1.5 秒让通知完整弹出后自动退出返回主界面
    sceKernelUsleep(1500000);
    sceSystemServiceLoadExec("exit", NULL);
    return 0;
}
