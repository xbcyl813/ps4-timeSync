# ps4-timeSync (PKG Homebrew Edition)

**English** | [简体中文](README_zh.md)

---

### 📖 Overview
**ps4-timeSync (PKG Edition)** is a standalone homebrew application (`.pkg`) for jailbroken **PS4 / PS4 Pro** consoles to sync UTC time via public NTP servers. *(Looking for the automatic background GoldHEN plugin version? Please switch to the **`main`** branch.)*

**Key Use Cases:**
* **One-Click Time Sync:** Launch directly from the PS4 home screen without a PC payload or plugin config.
* **CMOS Battery Workaround:** Restore system time after cold boot or battery depletion.
* **Fix Trophy Timestamps:** Prevents reset timestamps (`1970-01-01`) on offline consoles.

### ✨ Features
* **Standalone & Self-Exiting:** Exits back to the PS4 home screen in ~2 seconds after a status notification.
* **Multi-Firmware Compatibility:** Supports `5.05`, `6.72`, `7.xx`, `9.00`, `11.00`, etc. via `ps4-libjbc`.
* **Dual-Server Failover:** Queries primary NTP (`203.107.6.88`) with automatic failover to secondary NTP (`119.29.26.206`).

### 🚀 Installation & Usage
1. Download the `.pkg` artifact from the **Actions** (`pkg` branch) or **Releases** page.
2. Copy the `.pkg` file to an **exFAT / FAT32** USB drive root and install via **GoldHEN -> Package Installer**.
3. Ensure the console has internet and correct local time zone settings.
4. Launch the app from the home screen; it will auto-exit once synchronized.
