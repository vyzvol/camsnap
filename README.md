CamSnap takes a webcam snapshot, fetches your IP and location, sends everything to Telegram bot, and displays ASCII art. Requires a webcam and internet. Use responsibly. 

Snaps a 640x480 photo (YUYV format).
Gets IP and location via ip-api.com.
Sends photo + IP/location to a Telegram chat.
Kills VPNs (openvpn, wireguard, etc.) before capturing.
Shows ASCII art when done.

Requirements

Linux with a webcam (/dev/video0).
Libraries:
libcurl: For API calls.
nlohmann-json: JSON parsing.
libopenmp: Parallel processing.
stb_image_write.h: JPEG saving (included in repo).
V4L2: Linux kernel webcam support.



Setup

Clone the repo:git clone https://github.com/vyzvol/camsnap.git
cd camsnap


Install dependencies:
Arch Linux:sudo pacman -S curl nlohmann-json openmp pkg-config


Debian/Ubuntu:sudo apt update
sudo apt install libcurl4-openssl-dev nlohmann-json3-dev libomp-dev pkg-config


Fedora:sudo dnf install libcurl-devel nlohmann-json-devel libomp-devel pkgconf




Edit capture.cpp:
insert your Telegram bot token and telegram chat ID.

Notes

Works on most Linux distros with V4L2-compatible webcams.
Optimized with OpenMP for faster pixel conversion.
Replace VPN kill command regex if using distro-specific VPN clients (e.g., openvpn3 on Fedora).

License
MIT License. Hack it, share it, just keep it 
