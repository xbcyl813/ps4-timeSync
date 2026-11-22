name: Build PS4 NTP Sync Payload

on:
  push:
    branches: [ main, master ]
  workflow_dispatch: # 允许在网页端手动点击运行编译

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - name: 1. 检出当前仓库代码
        uses: actions/checkout@v4

      - name: 2. 拉取并编译 Scene-Collective PS4-Payload-SDK
        run: |
          git clone https://github.com /tmp/ps4-payload-sdk
          make -C /tmp/ps4-payload-sdk/libPS4

      - name: 3. 编译 PS4 ELF 与 BIN 载荷
        env:
          PS4SDK: /tmp/ps4-payload-sdk
        run: |
          make

      - name: 4. 上传编译产物 (ntp_sync.elf & ntp_sync.bin)
        uses: actions/upload-artifact@v4
        with:
          name: ps4-ntp-sync-payload
          path: |
            ntp_sync.elf
            ntp_sync.bin
