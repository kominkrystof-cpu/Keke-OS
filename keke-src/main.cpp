#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <linux/fb.h>
#include <errno.h>
#include <sys/syscall.h>

// Load a kernel module directly via finit_module() syscall
// This works without modprobe/insmod — just a raw syscall
static int loadKernelModule(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    
    // finit_module(fd, params, flags) — flags=0 means strict version check
    int ret = syscall(SYS_finit_module, fd, "", 0);
    int saved_errno = errno;
    close(fd);
    
    if (ret < 0) {
        errno = saved_errno;
        return -1;
    }
    return 0;
}

// Framebuffer class for direct graphics rendering
class Framebuffer {
private:
    int fb_fd;
    unsigned char* fb_mem;
    unsigned char* backbuffer;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long screensize;
    int width, height, bpp;
    bool flip_y = false;
    bool flip_x = false;
    
public:
    Framebuffer() : fb_fd(-1), fb_mem(nullptr), backbuffer(nullptr), screensize(0), width(0), height(0), bpp(0) {}
    
    bool init() {
        // First try opening /dev/fb0 directly
        fb_fd = open("/dev/fb0", O_RDWR);
        
        // If /dev/fb0 doesn't exist, try loading the Bochs DRM driver
        // QEMU's default -vga std emulates a Bochs VBE card
        if (fb_fd < 0 && errno == ENOENT) {
            std::cout << "\033[33m[FB] /dev/fb0 not found, loading bochs-drm module...\033[0m\n";
            
            int ret = loadKernelModule("/lib/modules/bochs.ko");
            if (ret == 0) {
                std::cout << "\033[32m[FB] bochs-drm module loaded\033[0m\n";
                // Wait for /dev/fb0 to appear
                for (int i = 0; i < 20; i++) {
                    usleep(100000); // 100ms
                    fb_fd = open("/dev/fb0", O_RDWR);
                    if (fb_fd >= 0) break;
                }
            } else if (ret < 0 && errno == EEXIST) {
                std::cout << "\033[32m[FB] bochs-drm module already loaded\033[0m\n";
                for (int i = 0; i < 20; i++) {
                    usleep(100000);
                    fb_fd = open("/dev/fb0", O_RDWR);
                    if (fb_fd >= 0) break;
                }
            } else {
                std::cout << "\033[31m[FB] Failed to load bochs-drm module (errno=" << errno << ")\033[0m\n";
            }
        }
        
        if (fb_fd < 0) {
            std::cerr << "[DEBUG] Failed to open /dev/fb0. Error code (" << errno
                      << "): " << std::strerror(errno) << std::endl;
            return false;
        }
        
        // Get fixed screen information
        if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
            ::close(fb_fd);
            return false;
        }
        
        // Get variable screen information
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
            ::close(fb_fd);
            return false;
        }
        
        width = vinfo.xres;
        height = vinfo.yres;
        bpp = vinfo.bits_per_pixel;
        screensize = finfo.smem_len;
        
        // Check if framebuffer has rotated orientation
        // rotate=1 = 90° CCW, rotate=2 = 180°, rotate=3 = 270° CCW
        if (vinfo.rotate == 2) {
            flip_y = true;
            flip_x = true;
            std::cout << "\033[33m[FB] Framebuffer rotated 180 degrees, enabling X+Y flip\033[0m\n";
        }
        
        // Map framebuffer to memory
        fb_mem = (unsigned char*)mmap(nullptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        if (fb_mem == MAP_FAILED) {
            ::close(fb_fd);
            return false;
        }
        
        // Allocate backbuffer for double-buffered rendering
        backbuffer = new unsigned char[screensize];
        if (backbuffer) {
            // Initialize backbuffer with black
            memset(backbuffer, 0, screensize);
        }
        
        return true;
    }
    
    // Get effective X coordinate (flipped if needed)
    int getEffectiveX(int x) const {
        return flip_x ? (width - 1 - x) : x;
    }
    
    // Get effective Y coordinate (flipped if needed)
    int getEffectiveY(int y) const {
        return flip_y ? (height - 1 - y) : y;
    }
    
    // Swap backbuffer to screen (single memcpy = no tearing)
    void swapBuffers() {
        if (backbuffer && fb_mem) {
            memcpy(fb_mem, backbuffer, screensize);
        }
    }
    
    // Swap only a region from backbuffer to screen (for cursor-only updates)
    void swapBuffersRegion(int x, int y, int w, int h) {
        if (!backbuffer || !fb_mem) return;
        // When flipped, region mapping is complex; fall back to full swap
        if (flip_x || flip_y) {
            memcpy(fb_mem, backbuffer, screensize);
            return;
        }
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > width) w = width - x;
        if (y + h > height) h = height - y;
        if (w <= 0 || h <= 0) return;
        int bpp_bytes = bpp / 8;
        for (int row = 0; row < h; row++) {
            int offset = (y + row) * finfo.line_length + x * bpp_bytes;
            memcpy(fb_mem + offset, backbuffer + offset, w * bpp_bytes);
        }
    }
    
    void close() {
        delete[] backbuffer;
        backbuffer = nullptr;
        if (fb_mem != nullptr && fb_mem != MAP_FAILED) {
            munmap(fb_mem, screensize);
        }
        if (fb_fd >= 0) {
            ::close(fb_fd);
        }
        fb_fd = -1;
        fb_mem = nullptr;
    }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getBpp() const { return bpp; }
    int getLineLength() const { return finfo.line_length; }
    unsigned char* getBackbuffer() const { return backbuffer; }
    
    // Set pixel at (x, y) with RGB color — writes to backbuffer
    void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        if (!backbuffer) return;
        
        int eff_x = getEffectiveX(x);
        int eff_y = getEffectiveY(y);
        long location = (eff_x + vinfo.xoffset) * (bpp / 8) + (eff_y + vinfo.yoffset) * finfo.line_length;
        
        if (bpp == 32) {
            backbuffer[location] = b;
            backbuffer[location + 1] = g;
            backbuffer[location + 2] = r;
            backbuffer[location + 3] = 0; // Alpha
        } else if (bpp == 24) {
            backbuffer[location] = b;
            backbuffer[location + 1] = g;
            backbuffer[location + 2] = r;
        } else if (bpp == 16) {
            unsigned short color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            *((unsigned short*)(backbuffer + location)) = color;
        }
    }
    
    // Fill screen with color
    void clear(unsigned char r, unsigned char g, unsigned char b) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                setPixel(x, y, r, g, b);
            }
        }
    }
    
    // Draw rectangle
    void drawRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b) {
        for (int i = 0; i < w; i++) {
            for (int j = 0; j < h; j++) {
                setPixel(x + i, y + j, r, g, b);
            }
        }
    }
    
    // Draw line (Bresenham's algorithm)
    void drawLine(int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b) {
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;
        
        while (true) {
            setPixel(x0, y0, r, g, b);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }
    
// Simple 8x8 bitmap font for text rendering
    void drawChar(int x, int y, char c, unsigned char r, unsigned char g, unsigned char b) {
        // Simple 8x8 font data for basic characters
        static const unsigned char font[][8] = {
            {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space (32)
            {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // ! (33)
            {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // " (34)
            {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x36}, // # (35)
            {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $ (36)
            {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // % (37)
            {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // & (38)
            {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // ' (39)
            {0x18,0x0C,0x06,0x06,0x06,0x06,0x0C,0x18}, // ( (40)
            {0x06,0x0C,0x18,0x18,0x18,0x18,0x0C,0x06}, // ) (41)
            {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // * (42)
            {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // + (43)
            {0x00,0x00,0x00,0x00,0x00,0x0C,0x06,0x06}, // , (44)
            {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // - (45)
            {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // . (46)
            {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // / (47)
            {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0 (48)
            {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1 (49)
            {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2 (50)
            {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3 (51)
            {0x38,0x3C,0x36,0x33,0x3F,0x30,0x78,0x00}, // 4 (52)
            {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5 (53)
            {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6 (54)
            {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7 (55)
            {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8 (56)
            {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9 (57)
            {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // : (58)
            {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x06,0x06}, // ; (59)
            {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // < (60)
            {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // = (61)
            {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // > (62)
            {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ? (63)
            {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @ (64)
            {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A (65)
            {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B (66)
            {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C (67)
            {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D (68)
            {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // E (69)
            {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // F (70)
            {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G (71)
            {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H (72)
            {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I (73)
            {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J (74)
            {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K (75)
            {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // L (76)
            {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // M (77)
            {0x63,0x73,0x7B,0x6F,0x67,0x63,0x63,0x00}, // N (78)
            {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O (79)
            {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // P (80)
            {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q (81)
            {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R (82)
            {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S (83)
            {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T (84)
            {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // U (85)
            {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V (86)
            {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W (87)
            {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X (88)
            {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y (89)
            {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z (90)
        };
        
        // Map lowercase to uppercase (a-z -> A-Z)
        unsigned char uc = (unsigned char)c;
        if (uc >= 'a' && uc <= 'z') {
            uc = uc - 32;
        }
        
        // Valid range: chars 32 (space) through 90 (Z) -> indices 0-58
        int char_index = uc - 32;
        if (char_index < 0 || char_index > 58) {
            char_index = 0; // Default to space
        }
        
        // Draw font bitmap: row 0 = top of character
        // Framebuffer setPixel handles Y-flip if needed
        for (int row = 0; row < 8; row++) {
            unsigned char font_row = font[char_index][row];
            for (int col = 0; col < 8; col++) {
                if (font_row & (1 << (7 - col))) {
                    setPixel(x + col, y + row, r, g, b);
                }
            }
        }
    }
    
    // Draw string at position
    void drawString(int x, int y, const std::string& text, unsigned char r, unsigned char g, unsigned char b) {
        for (size_t i = 0; i < text.length(); i++) {
            drawChar(x + i * 8, y, text[i], r, g, b);
        }
    }
};

// Include the Keke GUI framework (Windows XP Luna style)
#include "gui.hpp"

// ANSI Color Codes - following kernel.c VGA color scheme
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    
    // Text colors
    const std::string BLACK = "\033[30m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    
    // Bright text colors
    const std::string BRIGHT_BLACK = "\033[90m";
    const std::string BRIGHT_RED = "\033[91m";
    const std::string BRIGHT_GREEN = "\033[92m";
    const std::string BRIGHT_YELLOW = "\033[93m";
    const std::string BRIGHT_BLUE = "\033[94m";
    const std::string BRIGHT_MAGENTA = "\033[95m";
    const std::string BRIGHT_CYAN = "\033[96m";
    const std::string BRIGHT_WHITE = "\033[97m";
    
    // Background colors
    const std::string BG_BLACK = "\033[40m";
    const std::string BG_RED = "\033[41m";
    const std::string BG_GREEN = "\033[42m";
    const std::string BG_YELLOW = "\033[43m";
    const std::string BG_BLUE = "\033[44m";
    const std::string BG_MAGENTA = "\033[45m";
    const std::string BG_CYAN = "\033[46m";
    const std::string BG_WHITE = "\033[47m";
}

class KekeShell {
private:
    static constexpr int HISTORY_SIZE = 5;
    static constexpr int INPUT_BUFFER_SIZE = 256;
    
    std::string current_dir;
    std::string current_text_color;
    std::string current_bg_color;
    std::string repo_url;  // KPM repository URL
    int cursor_style; // 0 = arrow, 1 = cat paw
    std::vector<std::string> command_history;
    int history_count;
    int history_index;
    
    // String utilities - avoiding exceptions
    static int strcmp_custom(const char* s1, const char* s2) {
        while (*s1 && (*s1 == *s2)) {
            s1++;
            s2++;
        }
        return *(const unsigned char*)s1 - *(const unsigned char*)s2;
    }
    
    static int strlen_custom(const char* s) {
        int len = 0;
        while (s[len]) len++;
        return len;
    }
    
    static void strcpy_custom(char* dest, const char* src) {
        while ((*dest++ = *src++));
    }
    
    static int atoi_custom(const char* s) {
        int result = 0;
        int sign = 1;
        
        if (*s == '-') {
            sign = -1;
            s++;
        }
        
        while (*s >= '0' && *s <= '9') {
            result = result * 10 + (*s - '0');
            s++;
        }
        
        return sign * result;
    }
    
    void printHeader() {
        std::cout << Colors::CYAN << "Keke OS Terminal [Verze 2.7.5 - Multi-Language Build]\n";
        std::cout << "Copyright (c) 2026 Keke Corporation. Vsechna prava vyzrazena.\n";
        std::cout << "Licence: KekeOS Personal Edition - Aktivovano pro ThinkPad X380.\n";
        std::cout << "Languages: C++ | C | Python | JavaScript | Purr++ | Shell\n";
        std::cout << Colors::RESET << "\n";
    }
    
    void printWelcome() {
        std::cout << Colors::BRIGHT_MAGENTA << Colors::BOLD;
        std::cout << "╔════════════════════════════════════════════════╗\n";
        std::cout << "║           " << Colors::BRIGHT_YELLOW << "[ SECURE BOOT ACTIVATED ]" << Colors::BRIGHT_MAGENTA << "           ║\n";
        std::cout << "║       " << Colors::BRIGHT_CYAN << "KekeOS Security System v2.7.5" << Colors::BRIGHT_MAGENTA << "          ║\n";
        std::cout << "╚════════════════════════════════════════════════╝\n";
        std::cout << Colors::RESET << "\n";
    }
    
    void printPrompt() {
        std::cout << current_bg_color << current_text_color;
        std::cout << "keke@os " << Colors::MAGENTA << "MSYS " 
                  << current_dir << Colors::MAGENTA << "$ " << Colors::RESET;
        std::cout.flush();
    }
    
    void printError(const char* msg) {
        std::cout << Colors::RED << msg << Colors::RESET << "\n";
    }
    
    void printSuccess(const char* msg) {
        std::cout << Colors::GREEN << msg << Colors::RESET << "\n";
    }
    
    void printInfo(const char* msg) {
        std::cout << Colors::WHITE << msg << Colors::RESET << "\n";
    }
    
    void clearScreen() {
        std::cout << "\033[2J\033[H";
        printHeader();
    }
    
    // Calculator command from kernel.c
    void cmdCalc() {
        std::cout << Colors::YELLOW << "\n=== Keke OS Kalkulacka v1.0 ===\n";
        std::cout << Colors::WHITE << "Pro ukonceni napiste 'exit' misto cisla.\n\n";
        
        std::cout << Colors::WHITE << "Cislo 1: " << Colors::RESET;
        std::string input;
        std::getline(std::cin, input);
        
        if (strcmp_custom(input.c_str(), "exit") == 0) return;
        int a = atoi_custom(input.c_str());
        
        std::cout << Colors::WHITE << "Operace (+,-,*,/): " << Colors::RESET;
        std::getline(std::cin, input);
        char op = input[0];
        
        std::cout << Colors::WHITE << "Cislo 2: " << Colors::RESET;
        std::getline(std::cin, input);
        int b = atoi_custom(input.c_str());
        
        std::cout << "\n";
        int result = 0;
        bool valid = true;
        
        switch (op) {
            case '+': result = a + b; break;
            case '-': result = a - b; break;
            case '*': result = a * b; break;
            case '/':
                if (b == 0) {
                    printError("Chyba: Deleni nulou!");
                    valid = false;
                } else {
                    result = a / b;
                }
                break;
            default:
                printError("Neznama operace!");
                valid = false;
        }
        
        if (valid) {
            std::cout << Colors::GREEN << "Vysledek: " << result << "\n" << Colors::RESET;
        }
        
        std::cout << Colors::WHITE << "\nStisknete Enter pro navrat do Shellu...\n" << Colors::RESET;
        std::getline(std::cin, input);
    }
    
    // Time command using system time
    void cmdTime() {
        time_t now = time(nullptr);
        char* dt = ctime(&now);
        std::cout << Colors::WHITE << "Aktualni cas: " << Colors::RESET << dt;
    }
    
    // CD command - check if directory exists with path normalization
    void cmdCd(const std::string& arg) {
        if (arg.empty() || strcmp_custom(arg.c_str(), "~") == 0) {
            current_dir = "/mnt";
            return;
        }
        
        std::string full_path;
        
        // Handle absolute paths (starting with /)
        if (arg[0] == '/') {
            full_path = "/mnt" + arg;
        }
        // Handle step back (..) or multiple .. sequences
        else if (arg.find("..") == 0) {
            full_path = current_dir;
            
            // Count how many .. sequences
            int dot_count = 0;
            size_t pos = 0;
            while (pos < arg.length()) {
                if (arg[pos] == '.' && pos + 1 < arg.length() && arg[pos + 1] == '.') {
                    dot_count++;
                    pos += 2;
                    // Skip slash if present
                    if (pos < arg.length() && arg[pos] == '/') {
                        pos++;
                    }
                } else {
                    break;
                }
            }
            
            // Trim path for each ..
            for (int i = 0; i < dot_count; i++) {
                if (full_path == "/mnt") {
                    // Can't go back further than root
                    break;
                }
                
                size_t last_slash = full_path.find_last_of('/');
                if (last_slash != std::string::npos && last_slash > 0) {
                    full_path = full_path.substr(0, last_slash);
                } else {
                    full_path = "/mnt";
                }
            }
        }
        // Handle relative paths (no slash, no dots)
        else {
            full_path = current_dir + "/" + arg;
        }
        
        // Check if directory exists
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            current_dir = full_path;
        } else {
            std::cout << Colors::RED << "Adresář neexistuje" << Colors::RESET << "\n";
        }
    }
    
    // LS command - list directory contents
    void cmdLs() {
        std::string path = current_dir;
        DIR* dir = opendir(path.c_str());
        
        if (dir == nullptr) {
            std::cout << Colors::RED << "Nepodařilo se otevřít adresář" << Colors::RESET << "\n";
            return;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            // Skip . and ..
            if (strcmp_custom(entry->d_name, ".") == 0 || strcmp_custom(entry->d_name, "..") == 0) {
                continue;
            }
            
            // Color directories differently
            std::string full_path = path + "/" + entry->d_name;
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                std::cout << Colors::BRIGHT_BLUE << entry->d_name << Colors::RESET << "  ";
            } else {
                std::cout << Colors::WHITE << entry->d_name << Colors::RESET << "  ";
            }
        }
        
        closedir(dir);
        std::cout << "\n";
    }
    
    // MKDIR command - create directory
    void cmdMkdir(const std::string& arg) {
        if (arg.empty()) {
            std::cout << Colors::RED << "Použití: mkdir <název_adresáře>" << Colors::RESET << "\n";
            return;
        }
        
        std::string full_path = current_dir + "/" + arg;
        
        if (mkdir(full_path.c_str(), 0755) != 0) {
            std::cout << Colors::RED << "Nepodařilo se vytvořit adresář" << Colors::RESET << "\n";
        } else {
            std::cout << Colors::GREEN << "Adresář vytvořen" << Colors::RESET << "\n";
        }
    }
    
    // CAT command - read file or show easter egg
    void cmdCat(const std::string& arg) {
        if (arg.empty()) {
            // Show easter egg
            std::cout << Colors::BRIGHT_MAGENTA;
            std::cout << "Mnozi se ptaji, proc PCI scanner v Keke OS funguje tak stabilne. Pravda je takova, ze hlavni architekt systemu nesedi u klavesnice, ale sleduje kod z nejvyssiho patra kociciho stromu v rohu mistnosti. Kdyz kod v noci nesel zkompilovat a Rax uz propadal zoufalstvi, prosla se po klavesnici tlapka. Smazala tri stredniky a zmenila uint16_t na uint32_t. Byl to akt ciste ritualni magie. Tento system nebyl naprogramovan, byl schvalen vyssi kocici inteligenci. Pokud ti kod pada, zapomnel jsi nasypat granule.\n";
            std::cout << Colors::RESET;
            return;
        }
        
        std::string full_path = current_dir + "/" + arg;
        
        // Try to open file
        int fd = open(full_path.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cout << Colors::RED << "Soubor neexistuje" << Colors::RESET << "\n";
            return;
        }
        
        // Read file contents
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            std::cout.write(buffer, bytes_read);
        }
        
        close(fd);
        std::cout << "\n";
    }
    
    // RM command - delete file or directory
    void cmdRm(const std::string& arg) {
        if (arg.empty()) {
            std::cout << Colors::RED << "Použití: rm <soubor_nebo_adresář>" << Colors::RESET << "\n";
            return;
        }
        
        std::string full_path = current_dir + "/" + arg;
        
        // First try unlink for regular files
        if (unlink(full_path.c_str()) == 0) {
            std::cout << Colors::GREEN << "Soubor smazán" << Colors::RESET << "\n";
            return;
        }
        
        // If unlink fails, try rmdir for directories
        if (rmdir(full_path.c_str()) == 0) {
            std::cout << Colors::GREEN << "Adresář smazán" << Colors::RESET << "\n";
            return;
        }
        
        // Both failed
        std::cout << Colors::RED << "Soubor nebo adresář neexistuje" << Colors::RESET << "\n";
    }
    
    // TOUCH command - create empty file
    void cmdTouch(const std::string& arg) {
        if (arg.empty()) {
            std::cout << Colors::RED << "Použití: touch <název_souboru>" << Colors::RESET << "\n";
            return;
        }
        
        std::string full_path = current_dir + "/" + arg;
        
        // Create file with O_CREAT | O_WRONLY
        int fd = open(full_path.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fd < 0) {
            std::cout << Colors::RED << "Nepodařilo se vytvořit soubor" << Colors::RESET << "\n";
            return;
        }
        
        close(fd);
        std::cout << Colors::GREEN << "Soubor vytvořen" << Colors::RESET << "\n";
    }
    
    // Simple HTTP GET request using sockets
    std::string httpGet(const std::string& host, const std::string& path) {
        struct sockaddr_in server_addr;
        struct hostent* server;
        int sockfd;
        char buffer[4096];
        std::string response;
        
        // Create socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            return "ERROR: Could not create socket";
        }
        
        // Resolve hostname
        server = gethostbyname(host.c_str());
        if (server == nullptr) {
            close(sockfd);
            return "ERROR: Could not resolve hostname";
        }
        
        // Set up server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(80);
        memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        
        // Connect to server
        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            return "ERROR: Could not connect to server";
        }
        
        // Send HTTP GET request
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Connection: close\r\n\r\n";
        
        if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
            close(sockfd);
            return "ERROR: Could not send request";
        }
        
        // Receive response
        int bytes_received;
        while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            response += buffer;
        }
        
        close(sockfd);
        return response;
    }
    
    // KPM (Keke Package Manager) commands
    void cmdKpm(const std::string& arg) {
        if (arg.empty()) {
            std::cout << Colors::YELLOW << "Keke Package Manager v2.0\n";
            std::cout << Colors::WHITE << "Použití: kpm <příkaz> [argumenty]\n";
            std::cout << Colors::CYAN << "Příkazy:\n";
            std::cout << "  repo <url>       - Nastavit repozitář\n";
            std::cout << "  install <balík>  - Nainstalovat balík ze serveru\n";
            std::cout << "  list             - Vypsat dostupné balíky\n";
            std::cout << "  remove <balík>   - Odstranit balík\n";
            std::cout << "  update           - Aktualizovat seznam balíků\n" << Colors::RESET;
            return;
        }

        size_t space_pos = arg.find(' ');
        std::string kpm_cmd = (space_pos == std::string::npos) ? arg : arg.substr(0, space_pos);
        std::string kpm_arg = (space_pos == std::string::npos) ? "" : arg.substr(space_pos + 1);

        if (strcmp_custom(kpm_cmd.c_str(), "repo") == 0) {
            if (kpm_arg.empty()) {
                if (repo_url.empty()) {
                    std::cout << Colors::YELLOW << "Repozitář není nastaven. Použijte: kpm repo <url>\n" << Colors::RESET;
                } else {
                    std::cout << Colors::CYAN << "Aktuální repozitář: " << repo_url << Colors::RESET << "\n";
                }
                return;
            }
            repo_url = kpm_arg;
            if (repo_url.find("http://") != 0 && repo_url.find("https://") != 0) {
                repo_url = "http://" + repo_url;
            }
            std::cout << Colors::GREEN << "Repozitář nastaven na: " << repo_url << Colors::RESET << "\n";
            return;
        }

        if (repo_url.empty()) {
            std::cout << Colors::RED << "Repozitář není nastaven! Použijte: kpm repo <url>\n" << Colors::RESET;
            return;
        }

        if (strcmp_custom(kpm_cmd.c_str(), "list") == 0) {
            std::cout << Colors::YELLOW << "Načítám seznam balíků z " << repo_url << "/packages.json...\n" << Colors::RESET;
            std::string json = httpGet(repo_url, "/packages.json");
            if (json.find("ERROR") == 0) {
                std::cout << Colors::RED << "Chyba při stažení seznamu: " << json << Colors::RESET << "\n";
                return;
            }
            // Simple JSON parsing: extract "name": "description" pairs
            std::cout << Colors::CYAN << "Dostupné balíky:\n" << Colors::RESET;
            size_t pos = 0;
            while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
                pos += 6;
                while (pos < json.length() && json[pos] != '"') pos++;
                if (pos >= json.length()) break;
                pos++;
                size_t name_start = pos;
                while (pos < json.length() && json[pos] != '"') pos++;
                std::string name = json.substr(name_start, pos - name_start);

                pos += 2; // skip ",
                while (pos < json.length() && json[pos] != '"') pos++;
                if (pos >= json.length()) break;
                pos++;
                while (pos < json.length() && json[pos] != '"') pos++;
                if (pos < json.length()) pos++;
                size_t desc_start = pos;
                while (pos < json.length() && json[pos] != '"') pos++;
                std::string desc = json.substr(desc_start, pos - desc_start);

                std::cout << "  " << name << "  - " << desc << "\n";
            }
            std::cout << Colors::RESET;
        }
        else if (strcmp_custom(kpm_cmd.c_str(), "install") == 0) {
            if (kpm_arg.empty()) {
                std::cout << Colors::RED << "Zadejte název balíku" << Colors::RESET << "\n";
                return;
            }

            std::cout << Colors::YELLOW << "Stahuji balík " << kpm_arg << " ze " << repo_url << "...\n" << Colors::RESET;

            std::string url = repo_url + "/packages/" + kpm_arg + ".pkg";
            std::string content = httpGet(repo_url, "/packages/" + kpm_arg + ".pkg");

            if (content.find("ERROR") == 0) {
                std::cout << Colors::RED << "Chyba při stažení: " << content << Colors::RESET << "\n";
                return;
            }

            std::string install_path = "/mnt/" + kpm_arg;
            int fd = open(install_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0755);
            if (fd < 0) {
                std::cout << Colors::RED << "Chyba při vytváření souboru" << Colors::RESET << "\n";
                return;
            }
            write(fd, content.c_str(), content.length());
            close(fd);
            std::cout << Colors::GREEN << "Balík " << kpm_arg << " úspěšně nainstalován do /mnt/" << Colors::RESET << "\n";
        }
        else if (strcmp_custom(kpm_cmd.c_str(), "remove") == 0) {
            if (kpm_arg.empty()) {
                std::cout << Colors::RED << "Zadejte název balíku" << Colors::RESET << "\n";
                return;
            }

            std::string package_path = "/mnt/" + kpm_arg;
            if (unlink(package_path.c_str()) == 0) {
                std::cout << Colors::GREEN << "Balík " << kpm_arg << " odstraněn" << Colors::RESET << "\n";
            } else {
                std::cout << Colors::RED << "Balík " << kpm_arg << " nenalezen" << Colors::RESET << "\n";
            }
        }
        else if (strcmp_custom(kpm_cmd.c_str(), "update") == 0) {
            std::cout << Colors::YELLOW << "Aktualizuji seznam balíků z " << repo_url << "...\n" << Colors::RESET;
            std::string json = httpGet(repo_url, "/packages.json");
            if (json.find("ERROR") == 0) {
                std::cout << Colors::RED << "Chyba při aktualizaci: " << json << Colors::RESET << "\n";
                return;
            }
            // Count packages in JSON
            int count = 0;
            size_t pos = 0;
            while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
                count++;
                pos += 6;
            }
            std::cout << Colors::GREEN << "Seznam balíků aktualizován (" << count << " balíků)" << Colors::RESET << "\n";
        }
        else {
            std::cout << Colors::RED << "Neznámý příkaz kpm: " << kpm_cmd << Colors::RESET << "\n";
        }
    }

    // NET command: raw HTTP GET request for debugging/testing
    void cmdNet(const std::string& arg) {
        if (arg.empty()) {
            std::cout << Colors::YELLOW << "Net - HTTP GET tool\n";
            std::cout << Colors::WHITE << "Použití: net <URL>\n";
            std::cout << Colors::CYAN << "Příklad: net http://example.com/index.html\n" << Colors::RESET;
            return;
        }

        // Parse URL: http://host/path
        std::string url = arg;
        if (url.find("http://") == 0) {
            url = url.substr(7);
        } else if (url.find("https://") == 0) {
            std::cout << Colors::RED << "HTTPS not supported yet, use http:// URL\n" << Colors::RESET;
            return;
        }

        size_t path_pos = url.find('/');
        std::string host = (path_pos == std::string::npos) ? url : url.substr(0, path_pos);
        std::string path = (path_pos == std::string::npos) ? "/" : url.substr(path_pos);

        std::cout << Colors::YELLOW << "GET http://" << host << path << "\n" << Colors::RESET;

        std::string response = httpGet(host, path);

        if (response.find("ERROR") == 0) {
            std::cout << Colors::RED << response << Colors::RESET << "\n";
            return;
        }

        // Strip HTTP headers (find \r\n\r\n)
        size_t body_start = response.find("\r\n\r\n");
        if (body_start != std::string::npos) {
            std::cout << response.substr(body_start + 4);
        } else {
            std::cout << response;
        }
    }
    
    // Simple Purr++ interpreter with labels, goto, variables, and math
    void executeKekeScript(const std::string& filepath) {
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cout << Colors::RED << "Soubor neexistuje" << Colors::RESET << "\n";
            return;
        }
        
        // Read entire file
        std::string script;
        char buffer[1024];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            script.append(buffer, bytes_read);
        }
        close(fd);
        
        // Build label map (label name -> line number)
        std::vector<std::string> lines;
        std::map<std::string, size_t> labels;
        
        size_t line_start = 0;
        int line_num = 0;
        while (line_start < script.length()) {
            size_t line_end = script.find('\n', line_start);
            if (line_end == std::string::npos) line_end = script.length();
            
            std::string line = script.substr(line_start, line_end - line_start);
            lines.push_back(line);
            
            // Check for label (lines starting with :)
            if (line.length() > 0 && line[0] == ':') {
                std::string label_name = line.substr(1);
                labels[label_name] = line_num;
            }
            
            line_start = line_end + 1;
            line_num++;
        }
        
        // Variable storage
        std::map<std::string, int> variables;
        
        // Execute line by line with goto support
        size_t current_line = 0;
        while (current_line < lines.size()) {
            std::string line = lines[current_line];
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                current_line++;
                continue;
            }
            
            // Skip labels (they're just markers)
            if (line[0] == ':') {
                current_line++;
                continue;
            }
            
            // Parse command
            if (line.find("print(") == 0) {
                size_t start = line.find('"');
                size_t end = line.rfind('"');
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    std::string text = line.substr(start + 1, end - start - 1);
                    std::cout << Colors::WHITE << text << Colors::RESET << "\n";
                }
                current_line++;
            }
            else if (line.find("print_var(") == 0) {
                size_t start = line.find('(');
                size_t end = line.find(')');
                if (start != std::string::npos && end != std::string::npos) {
                    std::string var_name = line.substr(start + 1, end - start - 1);
                    if (variables.find(var_name) != variables.end()) {
                        std::cout << Colors::WHITE << variables[var_name] << Colors::RESET << "\n";
                    }
                }
                current_line++;
            }
            else if (line.find("sleep(") == 0) {
                size_t start = line.find('(');
                size_t end = line.find(')');
                if (start != std::string::npos && end != std::string::npos) {
                    std::string num_str = line.substr(start + 1, end - start - 1);
                    int seconds = atoi_custom(num_str.c_str());
                    sleep(seconds);
                }
                current_line++;
            }
            else if (line.find("cls") == 0 || line.find("clear") == 0) {
                std::cout << "\033[2J\033[H";
                current_line++;
            }
            else if (line.find("color(") == 0) {
                size_t start = line.find('"');
                size_t end = line.rfind('"');
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    std::string color_name = line.substr(start + 1, end - start - 1);
                    if (color_name == "red") current_text_color = Colors::RED;
                    else if (color_name == "green") current_text_color = Colors::GREEN;
                    else if (color_name == "blue") current_text_color = Colors::BLUE;
                    else if (color_name == "yellow") current_text_color = Colors::YELLOW;
                    else if (color_name == "white") current_text_color = Colors::WHITE;
                    else if (color_name == "reset") current_text_color = Colors::WHITE;
                }
                current_line++;
            }
            else if (line.find("goto ") == 0) {
                std::string label_name = line.substr(5);
                if (labels.find(label_name) != labels.end()) {
                    current_line = labels[label_name];
                } else {
                    current_line++;
                }
            }
            else if (line.find("set ") == 0) {
                // set var = value or set var = var2
                size_t eq_pos = line.find('=');
                if (eq_pos != std::string::npos) {
                    std::string var_name = line.substr(4, eq_pos - 4);
                    std::string value_str = line.substr(eq_pos + 1);
                    
                    // Trim whitespace from var_name
                    while (!var_name.empty() && var_name.front() == ' ') var_name.erase(0, 1);
                    while (!var_name.empty() && var_name.back() == ' ') var_name.pop_back();
                    
                    // Trim whitespace from value_str
                    while (!value_str.empty() && value_str.front() == ' ') value_str.erase(0, 1);
                    while (!value_str.empty() && value_str.back() == ' ') value_str.pop_back();
                    
                    // Check if value is a variable or number
                    if (variables.find(value_str) != variables.end()) {
                        variables[var_name] = variables[value_str];
                    } else {
                        variables[var_name] = atoi_custom(value_str.c_str());
                    }
                }
                current_line++;
            }
            else if (line.find("add ") == 0) {
                // add var value
                size_t space_pos = line.find(' ', 4);
                if (space_pos != std::string::npos) {
                    std::string var_name = line.substr(4, space_pos - 4);
                    std::string value_str = line.substr(space_pos + 1);
                    
                    if (variables.find(var_name) != variables.end()) {
                        if (variables.find(value_str) != variables.end()) {
                            variables[var_name] += variables[value_str];
                        } else {
                            variables[var_name] += atoi_custom(value_str.c_str());
                        }
                    }
                }
                current_line++;
            }
            else if (line.find("sub ") == 0) {
                // sub var value
                size_t space_pos = line.find(' ', 4);
                if (space_pos != std::string::npos) {
                    std::string var_name = line.substr(4, space_pos - 4);
                    std::string value_str = line.substr(space_pos + 1);
                    
                    if (variables.find(var_name) != variables.end()) {
                        if (variables.find(value_str) != variables.end()) {
                            variables[var_name] -= variables[value_str];
                        } else {
                            variables[var_name] -= atoi_custom(value_str.c_str());
                        }
                    }
                }
                current_line++;
            }
            else if (line.find("if_eq ") == 0) {
                // if_eq var1 var2 command
                size_t space1 = line.find(' ', 6);
                size_t space2 = line.find(' ', space1 + 1);
                if (space1 != std::string::npos && space2 != std::string::npos) {
                    std::string var1 = line.substr(6, space1 - 6);
                    std::string var2 = line.substr(space1 + 1, space2 - space1 - 1);
                    std::string cmd = line.substr(space2 + 1);
                    
                    int val1 = variables.find(var1) != variables.end() ? variables[var1] : atoi_custom(var1.c_str());
                    int val2 = variables.find(var2) != variables.end() ? variables[var2] : atoi_custom(var2.c_str());
                    
                    if (val1 == val2) {
                        // Execute the command
                        if (cmd == "exit") {
                            break;
                        }
                    }
                }
                current_line++;
            }
            else if (line.find("input ") == 0) {
                // input var
                std::string var_name = line.substr(6);
                // Trim whitespace from var_name
                while (!var_name.empty() && var_name.front() == ' ') var_name.erase(0, 1);
                while (!var_name.empty() && var_name.back() == ' ') var_name.pop_back();
                
                std::cout << Colors::CYAN << "> " << Colors::RESET;
                std::string input;
                std::getline(std::cin, input);
                
                // Convert input to integer and store
                int value = atoi_custom(input.c_str());
                variables[var_name] = value;
                current_line++;
            }
            else if (line.find("exit") == 0) {
                break;
            }
            else {
                current_line++;
            }
        }
    }
    
    // Execute file (./filename)
    void executeFile(const std::string& filename) {
        std::string full_path = current_dir + "/" + filename;

        // Check if file exists
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) {
            std::cout << Colors::RED << "Soubor neexistuje" << Colors::RESET << "\n";
            return;
        }

        if (!S_ISREG(st.st_mode)) {
            std::cout << Colors::RED << "Soubor není spustitelný" << Colors::RESET << "\n";
            return;
        }

        // Detect file extension and run with appropriate interpreter
        size_t dot = filename.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = filename.substr(dot);
            if (ext == ".py") {
                executeExternal("/mnt/bin/python3 " + full_path);
                return;
            } else if (ext == ".js") {
                executeExternal("/mnt/bin/qjs " + full_path);
                return;
            } else if (ext == ".sh") {
                executeExternal("/bin/sh " + full_path);
                return;
            } else if (ext == ".c") {
                std::cout << Colors::YELLOW << "C soubory musí být nejprve zkompilovány. Použijte 'kkc " << filename << "'" << Colors::RESET << "\n";
                return;
            }
        }

        // Default: run as Purr++ script
        std::cout << Colors::CYAN << "Spouštím Purr++: " << filename << "\n" << Colors::RESET;
        executeKekeScript(full_path);
    }
    
    // Launch the Windows XP-style GUI
    void cmdGui() {
        Framebuffer fb;
        if (!fb.init()) {
            std::cout << Colors::RED << "Nepodařilo se inicializovat framebuffer (/dev/fb0)" << Colors::RESET << "\n";
            return;
        }
        
        std::cout << Colors::GREEN << "Framebuffer inicializován: " << fb.getWidth() << "x" << fb.getHeight() << " @ " << fb.getBpp() << "bpp" << Colors::RESET << "\n";
        std::cout << Colors::YELLOW << "Spouštím Keke GUI... (ESC pro návrat do shellu)" << Colors::RESET << "\n";
        
        // Restore terminal to canonical mode for GUI input
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        newt.c_cc[VMIN] = 0;
        newt.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        
        // Launch the GUI
        {
            GuiManager gui(&fb, fb.getWidth(), fb.getHeight(), cursor_style);
            gui.run();
        }
        
        // Restore terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        
        // Drain any leftover escape sequence bytes from stdin
        tcflush(STDIN_FILENO, TCIFLUSH);
        
        // Clear terminal screen after GUI exit
        std::cout << "\033[2J\033[H";
        std::cout.flush();
        
        fb.close();
        std::cout << Colors::GREEN << "GUI ukončen, návrat do shellu" << Colors::RESET << "\n";
    }
    
    // Add command to history
    void addToHistory(const std::string& cmd) {
        if (cmd.empty()) return;
        
        // Shift older commands
        if (history_count >= HISTORY_SIZE) {
            for (int i = HISTORY_SIZE - 1; i > 0; i--) {
                command_history[i] = command_history[i - 1];
            }
            command_history[0] = cmd;
        } else {
            command_history.insert(command_history.begin(), cmd);
            history_count++;
        }
        
        history_index = -1;
    }
    
    // Read a line of input with raw terminal mode, supporting arrow key history
    std::string readLine() {
        if (!isatty(STDIN_FILENO)) {
            std::string input;
            std::getline(std::cin, input);
            return input;
        }
        
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        newt.c_cc[VMIN] = 1;
        newt.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        
        std::string line;
        int hist_pos = -1;
        std::string saved_input;
        
        auto clearDisplay = [&](const std::string& s) {
            for (size_t i = 0; i < s.length(); i++) std::cout << "\b \b";
            std::cout.flush();
        };
        
        while (true) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) != 1) break;
            
            if (ch == '\n' || ch == '\r') {
                std::cout << std::endl;
                break;
            }
            else if (ch == 127 || ch == 8) {
                if (!line.empty()) {
                    line.pop_back();
                    std::cout << "\b \b";
                    std::cout.flush();
                }
            }
            else if (ch == 3) {
                clearDisplay(line);
                line.clear();
                std::cout << std::endl;
                break;
            }
            else if (ch == 27) {
                char seq[2];
                if (read(STDIN_FILENO, seq, 2) == 2 && seq[0] == '[') {
                    if (seq[1] == 'A' && history_count > 0 && hist_pos < history_count - 1) {
                        if (hist_pos == -1) saved_input = line;
                        hist_pos++;
                        clearDisplay(line);
                        line = command_history[hist_pos];
                        std::cout << line;
                        std::cout.flush();
                    }
                    else if (seq[1] == 'B') {
                        if (hist_pos > 0) {
                            hist_pos--;
                            clearDisplay(line);
                            line = command_history[hist_pos];
                            std::cout << line;
                            std::cout.flush();
                        }
                        else if (hist_pos == 0) {
                            hist_pos = -1;
                            clearDisplay(line);
                            line = saved_input;
                            std::cout << line;
                            std::cout.flush();
                        }
                    }
                }
            }
            else if (ch >= 32 && ch < 127) {
                if (hist_pos != -1) {
                    clearDisplay(line);
                    line.clear();
                    hist_pos = -1;
                    saved_input.clear();
                }
                line += ch;
                std::cout << ch;
                std::cout.flush();
            }
        }
        
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return line;
    }
    
    // Execute external command using fork/exec
    int executeExternal(const std::string& cmd) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            exit(1);
        } else if (pid > 0) {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            return WEXITSTATUS(status);
        }
        return -1;
    }

    // Track background jobs
    struct BackgroundJob {
        pid_t pid;
        std::string command;
        bool running;
    };
    std::vector<BackgroundJob> bg_jobs;

    // Execute with pipe: cmd1 | cmd2
    int executePipe(const std::string& cmd1, const std::string& cmd2) {
        int pipefd[2];
        if (pipe(pipefd) < 0) {
            std::cout << Colors::RED << "Pipe error" << Colors::RESET << "\n";
            return -1;
        }

        pid_t pid1 = fork();
        if (pid1 == 0) {
            // Child 1: write to pipe
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            execl("/bin/sh", "sh", "-c", cmd1.c_str(), nullptr);
            exit(1);
        }

        pid_t pid2 = fork();
        if (pid2 == 0) {
            // Child 2: read from pipe
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            execl("/bin/sh", "sh", "-c", cmd2.c_str(), nullptr);
            exit(1);
        }

        // Parent: close pipe, wait for both
        close(pipefd[0]);
        close(pipefd[1]);
        int status;
        waitpid(pid1, &status, 0);
        waitpid(pid2, &status, 0);
        return WEXITSTATUS(status);
    }

    // Execute with output redirection: cmd > file
    int executeRedirect(const std::string& cmd, const std::string& file, bool append) {
        pid_t pid = fork();
        if (pid == 0) {
            int fd;
            if (append) {
                fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (fd < 0) {
                std::cout << Colors::RED << "Cannot open file: " << file << Colors::RESET << "\n";
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WEXITSTATUS(status);
        }
        return -1;
    }

    // Execute command in background
    pid_t executeBackground(const std::string& cmd) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child: detach from terminal
            setsid();
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            exit(1);
        } else if (pid > 0) {
            return pid;
        }
        return -1;
    }
    
    // Password input with masking (using termios for Linux)
    std::string readPasswordPrompt(const std::string& prompt) {
        std::string password;
        std::cout << prompt;
        std::cout.flush();

        // Uložíme si původní nastavení terminálu
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;

        // Vypneme kanonický režim (čtení po řádcích) a echo (ozvěnu znaků)
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        char ch;
        while (true) {
            ch = std::cin.get();

            if (ch == '\n' || ch == '\r') {
                // Uživatel zmáčkl Enter, heslo je kompletní
                std::cout << std::endl;
                break;
            } 
            else if (ch == 127 || ch == 8) { 
                // Ošetření Backspace (mazání znaků)
                if (!password.empty()) {
                    password.pop_back();
                    // Vymažeme poslední hvězdičku z obrazovky (posun zpět, mezera, posun zpět)
                    std::cout << "\b \b";
                    std::cout.flush();
                }
            } 
            else {
                // Přidáme znak do hesla a na obrazovku vykreslíme hvězdičku!
                password += ch;
                std::cout << "*";
                std::cout.flush();
            }
        }

        // Vrátíme terminálu jeho původní nastavení (aby shell normálně vypisoval text)
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return password;
    }
    
    // Login function with 3 attempts and lockdown
    bool doLogin() {
        const char* correctPassword = "1234";
        int attempts = 3;
        bool accessGranted = false;

        while (attempts > 0) {
            std::string password = readPasswordPrompt("Enter KekeOS Access Token: ");
            
            if (strcmp_custom(password.c_str(), correctPassword) == 0) {
                std::cout << Colors::BRIGHT_GREEN << "\n[ ACCESS GRANTED ] Welcome back, keke!\n\n" << Colors::RESET;
                std::cout.flush();
                accessGranted = true;
                sleep(3);
                
                // Clear password from memory
                password.clear();
                break;
            } else {
                attempts--;
                std::cout << Colors::BRIGHT_RED << "\n[ ACCESS DENIED ] Invalid Token! Attempts left: " << attempts << "\n" << Colors::RESET;
                
                // Clear password from memory
                password.clear();
            }
        }

        return accessGranted;
    }

public:
    KekeShell() : current_dir("/mnt"), current_text_color(Colors::WHITE), current_bg_color(""), cursor_style(1), history_count(0), history_index(-1) {
        command_history.reserve(HISTORY_SIZE);
    }
    
    void run() {
        // Outer loop simulates hardware reboot
        while (true) {
            printWelcome();
            
            // Login with 3 attempts
            if (!doLogin()) {
                // Lockdown message after 3 failed attempts
                std::cout << Colors::RED << "\n========================================\n";
                std::cout << "  SYSTEM LOCKDOWN - INTRUDER DETECTED!  \n";
                std::cout << "        Soft-rebooting KekeOS...        \n";
                std::cout << "========================================\n" << Colors::RESET;
                
                sleep(3);
                std::cout << "\033[2J\033[H";
                continue; // Restart to beginning of outer loop
            }
            
            clearScreen();
            
            // Shell loop
            bool shellRunning = true;
            while (shellRunning) {
                printPrompt();
                
                std::string input = readLine();
                
                // Skip empty input
                if (input.empty()) continue;
                
                // Add to history
                addToHistory(input);
                
                // Parse command (first word)
                size_t space_pos = input.find(' ');
                std::string cmd = (space_pos == std::string::npos) ? input : input.substr(0, space_pos);
                std::string arg = (space_pos == std::string::npos) ? "" : input.substr(space_pos + 1);

                // Handle pipe: cmd1 | cmd2
                size_t pipe_pos = arg.find('|');
                if (pipe_pos != std::string::npos) {
                    std::string cmd1 = arg.substr(0, pipe_pos);
                    std::string cmd2 = arg.substr(pipe_pos + 1);
                    while (!cmd1.empty() && cmd1.back() == ' ') cmd1.pop_back();
                    while (!cmd2.empty() && cmd2.front() == ' ') cmd2.erase(cmd2.begin());
                    executePipe(cmd1, cmd2);
                    continue;
                }

                // Handle output redirect: cmd > file  or  cmd >> file
                size_t app_pos = arg.find(">>");
                size_t redir_pos = arg.find('>');
                if (app_pos != std::string::npos || (redir_pos != std::string::npos && redir_pos != (arg.length() - 1) && arg[redir_pos + 1] == '>')) {
                    size_t pos = (app_pos != std::string::npos) ? app_pos : redir_pos;
                    bool append = (arg[pos + 1] == '>');
                    std::string cmd_str = arg.substr(0, pos);
                    std::string file = arg.substr(pos + (append ? 2 : 1));
                    while (!cmd_str.empty() && cmd_str.back() == ' ') cmd_str.pop_back();
                    while (!file.empty() && file.front() == ' ') file.erase(file.begin());
                    while (!file.empty() && file.back() == ' ') file.pop_back();
                    executeRedirect(cmd_str, file, append);
                    continue;
                }
                else if (redir_pos != std::string::npos && redir_pos > 0 && arg[redir_pos - 1] != '>') {
                    std::string cmd_str = arg.substr(0, redir_pos);
                    std::string file = arg.substr(redir_pos + 1);
                    while (!cmd_str.empty() && cmd_str.back() == ' ') cmd_str.pop_back();
                    while (!file.empty() && file.front() == ' ') file.erase(file.begin());
                    while (!file.empty() && file.back() == ' ') file.pop_back();
                    executeRedirect(cmd_str, file, false);
                    continue;
                }

                // Handle background: cmd &
                bool background = false;
                if (!arg.empty() && arg.back() == '&') {
                    arg.pop_back();
                    while (!arg.empty() && arg.back() == ' ') arg.pop_back();
                    background = true;
                }

                // Built-in commands from kernel.c
                if (strcmp_custom(cmd.c_str(), "help") == 0) {
                    printInfo("Prikazy: help, cls, ver, calc, time, exit, reboot, cd, ls, mkdir, rm, touch, cat, cp, mv, chmod, find, grep, kpm, net, gui, color, cursor, origin, windows, keke_info, keketool, jobs, fg, bg");
                }
                else if (strcmp_custom(cmd.c_str(), "ver") == 0) {
                    std::cout << Colors::CYAN << "--------------------------------------------\n";
                    std::cout << "Keke Operating System [v2.7.5 - Stable Update]\n";
                    std::cout << "Build Date: Sunday, July 6, 2026\n";
                    std::cout << "Target HW: Intel UHD / Lenovo X380 Yoga\n";
                    std::cout << "Kernel: Custom Linux + Keke syscalls (x86_64)\n";
                    std::cout << "Languages: C++, C, Python, JavaScript, Purr++\n";
                    std::cout << "---------------------------------------------\n" << Colors::RESET;
                }
                else if (strcmp_custom(cmd.c_str(), "cls") == 0) {
                    clearScreen();
                }
                else if (strcmp_custom(cmd.c_str(), "calc") == 0) {
                    cmdCalc();
                    clearScreen();
                }
                else if (strcmp_custom(cmd.c_str(), "time") == 0) {
                    cmdTime();
                }
                else if (strcmp_custom(cmd.c_str(), "exit") == 0) {
                    std::cout << Colors::BRIGHT_YELLOW << "Ukoncuji QEMU simulaci...\n" << Colors::RESET;
                    return; // Exit entire run() function
                }
                else if (strcmp_custom(cmd.c_str(), "reboot") == 0) {
                    std::cout << Colors::BRIGHT_CYAN << "Provadim manualni restart shellu...\n" << Colors::RESET;
                    sleep(1);
                    std::cout << "\033[2J\033[H";
                    shellRunning = false; // Exit shell loop, outer loop will restart
                }
                else if (strcmp_custom(cmd.c_str(), "cd") == 0) {
                    cmdCd(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "ls") == 0) {
                    cmdLs();
                }
                else if (strcmp_custom(cmd.c_str(), "mkdir") == 0) {
                    cmdMkdir(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "rm") == 0) {
                    cmdRm(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "touch") == 0) {
                    cmdTouch(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "cp") == 0) {
                    if (arg.empty()) {
                        printError("Použití: cp <zdroj> <cíl>");
                    } else {
                        executeExternal("cp " + arg);
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "mv") == 0) {
                    if (arg.empty()) {
                        printError("Použití: mv <zdroj> <cíl>");
                    } else {
                        executeExternal("mv " + arg);
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "chmod") == 0) {
                    if (arg.empty()) {
                        printError("Použití: chmod <mood> <soubor>");
                    } else {
                        executeExternal("chmod " + arg);
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "find") == 0) {
                    executeExternal("find " + (arg.empty() ? "." : arg));
                }
                else if (strcmp_custom(cmd.c_str(), "grep") == 0) {
                    executeExternal("grep " + arg);
                }
                else if (strcmp_custom(cmd.c_str(), "jobs") == 0) {
                    if (bg_jobs.empty()) {
                        std::cout << Colors::CYAN << "Žádné běžící pozadí úlohy.\n" << Colors::RESET;
                    } else {
                        std::cout << Colors::CYAN << "Pozadí úlohy:\n" << Colors::RESET;
                        for (size_t i = 0; i < bg_jobs.size(); i++) {
                            const auto& job = bg_jobs[i];
                            std::cout << "  [" << (i + 1) << "]  " << job.pid << "  " << (job.running ? "běží" : "dokončeno") << "  " << job.command << "\n";
                        }
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "fg") == 0) {
                    if (bg_jobs.empty()) {
                        printError("Žádné pozadí úlohy.");
                    } else {
                        // Bring last job to foreground
                        auto& job = bg_jobs.back();
                        int status;
                        std::cout << Colors::YELLOW << "Foreground: " << job.command << " (PID " << job.pid << ")\n" << Colors::RESET;
                        waitpid(job.pid, &status, WUNTRACED);
                        job.running = false;
                        bg_jobs.erase(bg_jobs.end() - 1);
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "bg") == 0) {
                    if (bg_jobs.empty()) {
                        printError("Žádné pozadí úlohy.");
                    } else {
                        std::cout << Colors::YELLOW << "Všechny pozadí úlohy běží.\n" << Colors::RESET;
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "kpm") == 0) {
                    cmdKpm(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "net") == 0) {
                    cmdNet(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "gui") == 0) {
                    cmdGui();
                }
                else if (strcmp_custom(cmd.c_str(), "cursor") == 0) {
                    if (arg.empty()) {
                        std::cout << Colors::YELLOW << "Použití: cursor <arrow|paw>" << Colors::RESET << "\n";
                        std::cout << Colors::YELLOW << "Aktuální styl: " << (cursor_style == 0 ? "arrow" : "paw") << Colors::RESET << "\n";
                    }
                    else if (strcmp_custom(arg.c_str(), "arrow") == 0) {
                        cursor_style = 0;
                        printSuccess("Kurzor nastaven na šipku (arrow).");
                    }
                    else if (strcmp_custom(arg.c_str(), "paw") == 0) {
                        cursor_style = 1;
                        printSuccess("Kurzor nastaven na kočičí tlapičku (cat paw).");
                    }
                    else {
                        printError("Neznámý styl! Použijte 'cursor arrow' nebo 'cursor paw'.");
                    }
                }
                else if (cmd[0] == '.' && cmd[1] == '/') {
                    // File execution: ./filename
                    std::string filename = cmd.substr(2);
                    executeFile(filename);
                }
                else if (strcmp_custom(cmd.c_str(), "color") == 0) {
                    if (arg.empty()) {
                        std::cout << Colors::YELLOW << "Použití: color <barva> nebo color bg_<barva>" << Colors::RESET << "\n";
                        std::cout << Colors::YELLOW << "Dostupné texty: red, green, yellow, blue, magenta, cyan, white, reset" << Colors::RESET << "\n";
                        std::cout << Colors::YELLOW << "Dostupná pozadí: bg_black, bg_red, bg_green, bg_blue, bg_magenta, bg_cyan" << Colors::RESET << "\n";
                    }
                    else if (strcmp_custom(arg.c_str(), "reset") == 0) {
                        current_text_color = Colors::WHITE;
                        current_bg_color = "";
                        printSuccess("Barvy resetovány na výchozí hodnoty.");
                    }
                    else if (strcmp_custom(arg.c_str(), "red") == 0) {
                        current_text_color = Colors::RED;
                        printSuccess("Barva textu nastavena na červenou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "green") == 0) {
                        current_text_color = Colors::GREEN;
                        printSuccess("Barva textu nastavena na zelenou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "yellow") == 0) {
                        current_text_color = Colors::YELLOW;
                        printSuccess("Barva textu nastavena na žlutou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "blue") == 0) {
                        current_text_color = Colors::BLUE;
                        printSuccess("Barva textu nastavena na modrou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "magenta") == 0) {
                        current_text_color = Colors::MAGENTA;
                        printSuccess("Barva textu nastavena na purpurovou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "cyan") == 0) {
                        current_text_color = Colors::CYAN;
                        printSuccess("Barva textu nastavena na azurovou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "white") == 0) {
                        current_text_color = Colors::WHITE;
                        printSuccess("Barva textu nastavena na bílou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "bg_black") == 0) {
                        current_bg_color = Colors::BG_BLACK;
                        printSuccess("Barva pozadí nastavena na černou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "bg_red") == 0) {
                        current_bg_color = Colors::BG_RED;
                        printSuccess("Barva pozadí nastavena na červenou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "bg_green") == 0) {
                        current_bg_color = Colors::BG_GREEN;
                        printSuccess("Barva pozadí nastavena na zelenou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "bg_blue") == 0) {
                        current_bg_color = Colors::BG_BLUE;
                        printSuccess("Barva pozadí nastavena na modrou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "bg_magenta") == 0) {
                        current_bg_color = Colors::BG_MAGENTA;
                        printSuccess("Barva pozadí nastavena na purpurovou.");
                    }
                    else if (strcmp_custom(arg.c_str(), "bg_cyan") == 0) {
                        current_bg_color = Colors::BG_CYAN;
                        printSuccess("Barva pozadí nastavena na azurovou.");
                    }
                    else {
                        printError("Neznámá barva! Použijte 'color' pro nápovědu.");
                    }
                }
                else if (strcmp_custom(cmd.c_str(), "origin") == 0) {
                    std::cout << Colors::BRIGHT_MAGENTA;
                    std::cout << "Keke OS nevznikl u stolu psanim kodu. Vznikl v kvetnu 2026, kdy Keke zahral kombo Marshall na maximalni hlasitost a sekl do strun sve kytary. Elektromagneticky impulz a surova vibrace zvuku byly tak silne, ze v okolnim vesmiru doslo k mikro-ohnuti casoprostoru. V tu samou nanosekundu proletelo nedalekym ThinkPadem kosmicke zareni a prepisalo v pameti jedno kriticke 16-bitove cislo na 32-bitove. Notebook, do te doby polomrtvy, poprve zablinkal a na displeji se objevil napis Keke OS. Vedci tomu dodnes rikaji 'Rockovy zrod' a kod kvuli teto vesmirne anomalii odmitame kompilovat v tichu.\n";
                    std::cout << Colors::RESET;
                }
                else if (strcmp_custom(cmd.c_str(), "cat") == 0) {
                    cmdCat(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "windows") == 0) {
                    std::cout << Colors::BRIGHT_MAGENTA;
                    std::cout << "Kdyz Bill Gates v roce 1985 zakladal Windows, sedel v kancelari a dival se skrz realne sklenene okno na ulici. Videl lidi, jak spechaji do prace, a uvedomil si, ze lidstvo potrebuje system, ktery je bude neustale zdrzovat aktualizacemi, aby se v tom shonu na chvili zastavili. Byla to hlobaka myslenka. Keke OS tento paradox odmita. My se nedivame z okna, my se divame primo do kremiku. Keke OS nema okna, protoze okny utika teplo a rock'n'roll.\n";
                    std::cout << Colors::RESET;
                }
                else {
                    // Try to execute as external binary or script in standard paths
                    struct stat ext_st;
                    std::string paths[] = {
                        current_dir + "/" + cmd,
                        "/mnt/bin/" + cmd,
                        "/usr/bin/" + cmd,
                        "/bin/" + cmd,
                        "/mnt/" + cmd
                    };
                    bool found = false;
                    for (auto& p : paths) {
                        if (stat(p.c_str(), &ext_st) == 0 && S_ISREG(ext_st.st_mode) && (ext_st.st_mode & S_IXUSR)) {
                            if (background) {
                                pid_t bg_pid = executeBackground(p + " " + arg);
                                if (bg_pid > 0) {
                                    bg_jobs.push_back({bg_pid, p, true});
                                    std::cout << Colors::YELLOW << "[bg] " << bg_pid << " " << cmd << "\n" << Colors::RESET;
                                }
                            } else {
                                executeExternal(p);
                            }
                            found = true;
                            break;
                        }
                    }
                    // Also try cmd + arg as a single shell command
                    if (!found && !cmd.empty()) {
                        if (background) {
                            pid_t bg_pid = executeBackground(cmd + " " + arg);
                            if (bg_pid > 0) {
                                bg_jobs.push_back({bg_pid, cmd + " " + arg, true});
                                std::cout << Colors::YELLOW << "[bg] " << bg_pid << " " << cmd << "\n" << Colors::RESET;
                            }
                        } else {
                            executeExternal(cmd + " " + arg);
                        }
                        found = true;
                    }
                    if (!found) {
                        std::cout << Colors::RED << "-kekeShell: " << input << ": command not found\n" << Colors::RESET;
                    }
                }
                
                // Clear any temporary allocations
                input.clear();
                cmd.clear();
                arg.clear();

                // Clean up finished background jobs
                for (auto it = bg_jobs.begin(); it != bg_jobs.end();) {
                    int status;
                    pid_t result = waitpid(it->pid, &status, WNOHANG);
                    if (result > 0) {
                        it = bg_jobs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
};

int main() {
    // Mount devtmpfs to auto-populate /dev with device nodes
    // This creates /dev/fb0, /dev/sda, /dev/tty*, etc. automatically
    mkdir("/dev", 0755);
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, nullptr) == 0) {
        std::cout << Colors::GREEN << "[OK] Mounted devtmpfs on /dev" << Colors::RESET << "\n";
    } else {
        std::cout << Colors::YELLOW << "[WARNING] Could not mount devtmpfs, falling back to manual device nodes" << Colors::RESET << "\n";
        dev_t dev = makedev(8, 0);
        if (mknod("/dev/sda", S_IFBLK | 0660, dev) != 0) {
            std::cout << Colors::RED << "[WARNING] Failed to create /dev/sda device node" << Colors::RESET << "\n";
        } else {
            std::cout << Colors::GREEN << "[OK] Created /dev/sda device node" << Colors::RESET << "\n";
        }
        dev_t dev1 = makedev(8, 1);
        if (mknod("/dev/sda1", S_IFBLK | 0660, dev1) != 0) {
            std::cout << Colors::RED << "[WARNING] Failed to create /dev/sda1 device node" << Colors::RESET << "\n";
        } else {
            std::cout << Colors::GREEN << "[OK] Created /dev/sda1 device node" << Colors::RESET << "\n";
        }
    }

    // Load PS/2 mouse module
    if (loadKernelModule("/lib/modules/psmouse.ko") == 0) {
        std::cout << Colors::GREEN << "[OK] Loaded psmouse module (mouse support)" << Colors::RESET << "\n";
    } else {
        std::cout << Colors::YELLOW << "[WARNING] Failed to load psmouse.ko - mouse may not work" << Colors::RESET << "\n";
    }

    // Load Keke OS custom kernel module (/dev/kekeos)
    if (loadKernelModule("/lib/modules/kekeos-mod.ko") == 0) {
        std::cout << Colors::GREEN << "[OK] Keke OS module loaded (/dev/kekeos)" << Colors::RESET << "\n";
    } else {
        std::cout << Colors::YELLOW << "[WARNING] kekeos-mod.ko not found - Keke syscalls via /dev/kekeos unavailable" << Colors::RESET << "\n";
    }

    // Setup networking by enumerating interfaces dynamically
    // Reads /sys/class/net/ to find interfaces, skips loopback
    // Configures first non-loopback interface with static IP (QEMU: 10.0.2.15)
    int net_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (net_sock >= 0) {
        DIR *netdir = opendir("/sys/class/net");
        if (netdir) {
            struct dirent *entry;
            bool network_ok = false;
            while ((entry = readdir(netdir)) != nullptr) {
                // Skip loopback and special entries
                if (strcmp(entry->d_name, "lo") == 0) continue;
                if (entry->d_name[0] == '.') continue;

                // Skip if not a directory (symlinks are fine, check if it's an interface)
                char ifpath[256];
                snprintf(ifpath, sizeof(ifpath), "/sys/class/net/%s", entry->d_name);
                struct stat ifstat;
                if (stat(ifpath, &ifstat) != 0 || !S_ISDIR(ifstat.st_mode)) continue;

                // Found a real interface — bring it up and configure it
                std::cout << Colors::CYAN << "[NET] Found interface: " << entry->d_name << Colors::RESET << "\n";

                struct ifreq ifr;
                memset(&ifr, 0, sizeof(ifr));
                strncpy(ifr.ifr_name, entry->d_name, IFNAMSIZ - 1);

                // Read current flags
                if (ioctl(net_sock, SIOCGIFFLAGS, &ifr) < 0) {
                    std::cout << Colors::YELLOW << "[WARNING] Could not read flags for " << entry->d_name << Colors::RESET << "\n";
                    continue;
                }

                // Bring interface up
                ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
                if (ioctl(net_sock, SIOCSIFFLAGS, &ifr) < 0) {
                    std::cout << Colors::YELLOW << "[WARNING] Failed to bring up " << entry->d_name << Colors::RESET << "\n";
                    continue;
                }

                // Assign static IP 10.0.2.15/24 (QEMU user-mode networking)
                struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
                addr->sin_family = AF_INET;
                addr->sin_addr.s_addr = inet_addr("10.0.2.15");
                if (ioctl(net_sock, SIOCSIFADDR, &ifr) < 0) {
                    std::cout << Colors::YELLOW << "[WARNING] Could not set IP for " << entry->d_name << Colors::RESET << "\n";
                    continue;
                }

                // Set subnet mask 255.255.255.0
                addr->sin_addr.s_addr = inet_addr("255.255.255.0");
                 ioctl(net_sock, SIOCSIFNETMASK, &ifr);

                 // Setup default route via 10.0.2.2 (QEMU NAT gateway)
                 struct rtentry rt;
                 memset(&rt, 0, sizeof(rt));
                 struct sockaddr_in *rt_dest = (struct sockaddr_in *)&rt.rt_dst;
                 rt_dest->sin_family = AF_INET;
                 rt_dest->sin_addr.s_addr = inet_addr("0.0.0.0");
                 struct sockaddr_in *rt_gw = (struct sockaddr_in *)&rt.rt_gateway;
                 rt_gw->sin_family = AF_INET;
                 rt_gw->sin_addr.s_addr = inet_addr("10.0.2.2");
                 struct sockaddr_in *rt_genmask = (struct sockaddr_in *)&rt.rt_genmask;
                 rt_genmask->sin_family = AF_INET;
                 rt_genmask->sin_addr.s_addr = inet_addr("0.0.0.0");
                 rt.rt_flags = RTF_UP | RTF_GATEWAY;
                 if (ioctl(net_sock, SIOCADDRT, &rt) < 0) {
                     // Route setup failed — may still work if QEMU handles routing
                 } else {
                     std::cout << Colors::GREEN << "[OK] Default route via 10.0.2.2" << Colors::RESET << "\n";
                 }

                std::cout << Colors::GREEN << "[OK] Network configured: " << entry->d_name << "=10.0.2.15" << Colors::RESET << "\n";
                network_ok = true;
                break;  // Configure first real interface only for now
            }
            closedir(netdir);
            if (!network_ok) {
                std::cout << Colors::YELLOW << "[WARNING] No network interface found" << Colors::RESET << "\n";
            }
        } else {
            std::cout << Colors::YELLOW << "[WARNING] Could not enumerate /sys/class/net/ (kernel may not support it)" << Colors::RESET << "\n";
        }
        close(net_sock);

        // Setup DNS resolver (QEMU user-mode DNS = 10.0.2.3)
        mkdir("/etc", 0755);
        int resolv_fd = open("/etc/resolv.conf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (resolv_fd >= 0) {
            const char *dns = "nameserver 10.0.2.3\n";
            write(resolv_fd, dns, strlen(dns));
            close(resolv_fd);
            std::cout << Colors::GREEN << "[OK] DNS configured (10.0.2.3)" << Colors::RESET << "\n";
        }
    }

    // Mount disk partition to /mnt for file system access
    
    // Disk is partitioned — try /dev/sda1 first, then /dev/sda as fallback
    const char* mount_targets[] = {"/dev/sda1", "/dev/sda"};
    bool mounted = false;
    for (int t = 0; t < 2 && !mounted; t++) {
        if (mount(mount_targets[t], "/mnt", "ext4", 0, nullptr) == 0) {
            std::cout << Colors::GREEN << "[OK] Mounted " << mount_targets[t] << " to /mnt (ext4)" << Colors::RESET << "\n";
            mounted = true;
        } else if (mount(mount_targets[t], "/mnt", "vfat", 0, nullptr) == 0) {
            std::cout << Colors::GREEN << "[OK] Mounted " << mount_targets[t] << " to /mnt (vfat)" << Colors::RESET << "\n";
            mounted = true;
        } else if (mount(mount_targets[t], "/mnt", nullptr, 0, nullptr) == 0) {
            std::cout << Colors::GREEN << "[OK] Mounted " << mount_targets[t] << " to /mnt (auto-detect)" << Colors::RESET << "\n";
            mounted = true;
        }
    }
    if (!mounted) {
        std::cout << Colors::RED << "[WARNING] Failed to mount any disk partition to /mnt" << Colors::RESET << "\n";
        std::cout << Colors::YELLOW << "File system may not be available." << Colors::RESET << "\n";
    }
    
    KekeShell shell;
    shell.run();
    return 0;
}
