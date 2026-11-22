# PS4 NTP Clock Sync Payload

**English** | [简体中文](README_zh.md)

---

### 📖 Overview
A lightweight homebrew payload (`.elf` / `.bin`) for jailbroken **PS4 / PS4 Pro** consoles. It fetches accurate UTC internet time via UDP from public NTP servers and synchronizes it directly to the PS4 system kernel clock.

**Key Use Cases:**
* **CMOS Battery (CR2032) Workaround:** Restore accurate system time after battery depletion without disassembling the motherboard.
* **Fix Trophy Timestamps:** Prevent invalid or reset timestamps on earned trophies when offline or using DNS blockers.
* **DNS-Blocker Friendly:** Connects directly via public NTP IP addresses (with dual-server fallback), bypassing custom update-blocking DNS settings without touching PSN servers.

### ✨ Features
* **Multi-Firmware Compatibility:** Uses the native `sceKernelSettimeofday` API with an automatic privilege-escalation fallback, safely supporting `5.05`, `6.72`, `7.xx`, `9.00`, `11.00`, etc., without hardcoded kernel offset panics.
* **Dual-Server Fallback:** Queries primary NTP (`203.107.6.88`) with automatic failover to secondary NTP (`119.29.26.206`) and a 3-second timeout guard.
* **Automated CI Build:** Pre-configured with GitHub Actions to compile `.elf` and `.bin` binaries automatically on push.

### 🚀 Usage
1. Connect your jailbroken PS4 / PS4 Pro to the internet and ensure **GoldHEN** (or another payload loader) is active.
2. Enable **BinLoader Server** on your console (`GoldHEN Settings -> Server Settings -> BinLoader Server`, default port `9090`).
3. Download `ntp_sync.bin` or `ntp_sync.elf` from the **Actions (Artifacts)** or **Releases** tab.
4. Send the payload to your PS4's IP address on port `9090` using NetCat GUI, Payload Sender, or CLI:
   ```bash
   nc -w 3 <PS4_IP_ADDRESS> 9090 < ntp_sync.bin
   ```
5. Wait for the on-screen notification confirming time synchronization (make sure your console's Time Zone setting matches your local region).

### 🛠️ Credits & License
* Built with [Scene-Collective/ps4-payload-sdk](https://github.com).
* Released under the MIT License.
