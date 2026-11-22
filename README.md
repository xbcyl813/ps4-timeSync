# PS4 NTP Clock Sync Plugin for GoldHEN (`ps4-timeSync.prx`)

**English** | [简体中文](README_zh.md)

---

### 📖 Overview
A lightweight background plugin (`.prx`) for **GoldHEN** on jailbroken **PS4 / PS4 Pro** consoles that automatically fetches accurate UTC internet time via UDP from public NTP servers and synchronizes it to the PS4 system kernel clock upon boot.

**Key Use Cases:**
* **CMOS Battery (CR2032) Workaround:** Automatically restores accurate system time after cold boot or battery depletion without motherboard disassembly or manual payload injection.
* **Fix Trophy Timestamps:** Prevents reset timestamps (`1970-01-01`) on earned trophies when using offline consoles or update-blocking DNS.
* **DNS-Blocker Friendly:** Connects directly via public NTP IP addresses (with dual-server fallback), bypassing custom update-blocking DNS settings.

### ✨ Features
* **Fully Automated & Non-Blocking:** Runs asynchronously via `pthread` after GoldHEN initializes without lag or freezing on the PS4 home screen.
* **Multi-Firmware Compatibility:** Uses the native `sceKernelSettimeofday` API, supporting `5.05`, `6.72`, `7.xx`, `9.00`, `11.00`, etc.
* **Dual-Server Failover:** Queries primary NTP (`203.107.6.88`) with automatic failover to secondary NTP (`119.29.26.206`) and a 3-second timeout guard.

### 🚀 Installation & Usage
1. Download `ps4-timeSync.prx` from the **Actions (Artifacts)** or **Releases** tab.
2. Transfer `ps4-timeSync.prx` via FTP (`port 2121`) or PS4-Xplorer into `/data/GoldHEN/plugins/ps4-timeSync.prx`.
3. Add the plugin path under the `[default]` section in `/data/GoldHEN/plugins.ini`.
4. Go to **GoldHEN Settings -> Plugins** on your PS4 and enable **`Enable Plugins Loader`**.
5. Reboot or activate GoldHEN with an active internet connection to trigger automatic time synchronization.
