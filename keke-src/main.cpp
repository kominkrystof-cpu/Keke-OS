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
#include <linux/fb.h>

// Framebuffer class for direct graphics rendering
class Framebuffer {
private:
    int fb_fd;
    unsigned char* fb_mem;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long screensize;
    int width, height, bpp;
    
public:
    Framebuffer() : fb_fd(-1), fb_mem(nullptr), screensize(0), width(0), height(0), bpp(0) {}
    
    bool init() {
        fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        // Tohle ti vypíše srozumitelnou chybu do terminálu
        std::cerr << "[DEBUG] Failed to open /dev/fb0. Error code (" << errno 
                  << "): " << std::strerror(errno) << std::endl;
        return false;
    }
        // Open framebuffer device
        fb_fd = open("/dev/fb0", O_RDWR);
        if (fb_fd < 0) {
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
        
        // Map framebuffer to memory
        fb_mem = (unsigned char*)mmap(nullptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        if (fb_mem == MAP_FAILED) {
            ::close(fb_fd);
            return false;
        }
        
        return true;
    }
    
    void close() {
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
    
    // Set pixel at (x, y) with RGB color
    void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        
        long location = (x + vinfo.xoffset) * (bpp / 8) + (y + vinfo.yoffset) * finfo.line_length;
        
        if (bpp == 32) {
            fb_mem[location] = b;
            fb_mem[location + 1] = g;
            fb_mem[location + 2] = r;
            fb_mem[location + 3] = 0; // Alpha
        } else if (bpp == 24) {
            fb_mem[location] = b;
            fb_mem[location + 1] = g;
            fb_mem[location + 2] = r;
        } else if (bpp == 16) {
            unsigned short color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            *((unsigned short*)(fb_mem + location)) = color;
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
        
        int char_index = c - 32;
        if (char_index < 0 || char_index >= 59) {
            char_index = 0; // Default to space for unknown chars
        }
        
        for (int row = 0; row < 8; row++) {
            unsigned char font_row = font[char_index][row];
            for (int col = 0; col < 8; col++) {
                if (font_row & (0x80 >> col)) {
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
        std::cout << Colors::CYAN << "Keke OS Terminal [Verze 2.7.5 - Stable Build 2026]\n";
        std::cout << "Copyright (c) 2026 Keke Corporation. Vsechna prava vyzrazena.\n";
        std::cout << "Licence: KekeOS Personal Edition - Aktivovano pro ThinkPad X380.\n";
        std::cout << "Nainstalujte si nejnovejsi verzi na: https://keke-os.com\n";
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
            std::cout << Colors::YELLOW << "Keke Package Manager v1.0\n";
            std::cout << Colors::WHITE << "Použití: kpm <příkaz> [argumenty]\n";
            std::cout << Colors::CYAN << "Příkazy:\n";
            std::cout << "  install <balík>  - Nainstalovat balík\n";
            std::cout << "  list             - Vypsat dostupné balíky\n";
            std::cout << "  remove <balík>   - Odstranit balík\n";
            std::cout << "  update           - Aktualizovat seznam balíků\n" << Colors::RESET;
            return;
        }
        
        size_t space_pos = arg.find(' ');
        std::string kpm_cmd = (space_pos == std::string::npos) ? arg : arg.substr(0, space_pos);
        std::string kpm_arg = (space_pos == std::string::npos) ? "" : arg.substr(space_pos + 1);
        
        if (strcmp_custom(kpm_cmd.c_str(), "list") == 0) {
            std::cout << Colors::CYAN << "Dostupné balíky:\n";
            std::cout << "  pong      - Hra Pong v Purr++\n";
            std::cout << "  hello     - Ukázkový program Hello World\n";
            std::cout << "  calc      - Pokročilá kalkulačka\n" << Colors::RESET;
        }
        else if (strcmp_custom(kpm_cmd.c_str(), "install") == 0) {
            if (kpm_arg.empty()) {
                std::cout << Colors::RED << "Zadejte název balíku" << Colors::RESET << "\n";
                return;
            }
            
            std::cout << Colors::YELLOW << "Stahuji balík: " << kpm_arg << "...\n" << Colors::RESET;
            
            // For now, simulate download (real implementation would use HTTP)
            std::cout << Colors::GREEN << "Balík " << kpm_arg << " úspěšně nainstalován do /mnt/" << Colors::RESET << "\n";
            
            // Create package file based on type
            std::string install_path = "/mnt/" + kpm_arg;
            int fd = open(install_path.c_str(), O_CREAT | O_WRONLY, 0755);
            if (fd >= 0) {
                if (strcmp_custom(kpm_arg.c_str(), "pong") == 0) {
                    // Pong game script with game loop and emergency brake
                    const char* pong_script = 
                        "#!/usr/bin/env purr++\n"
                        "# KekeOS Pong Game\n"
                        "print(\"=== KEKE PONG ===\")\n"
                        "print(\"Stiskněte Enter pro start...\")\n"
                        "input dummy\n"
                        "cls\n"
                        "set score = 0\n"
                        "set ball_x = 10\n"
                        "set paddle_x = 5\n"
                        ":game_loop\n"
                        "cls\n"
                        "color(\"green\")\n"
                        "print(\"Score: \")\n"
                        "print_var(score)\n"
                        "color(\"white\")\n"
                        "print(\"----------------\")\n"
                        "print(\"|              |\")\n"
                        "print(\"|              |\")\n"
                        "print(\"|      @       |\")\n"
                        "print(\"|              |\")\n"
                        "print(\"|    |===|     |\")\n"
                        "print(\"----------------\")\n"
                        "print(\"Zadejte 'q' pro ukončení...\")\n"
                        "input cmd\n"
                        "set quit_check = 113\n"
                        "if_eq cmd quit_check exit\n"
                        "add ball_x 1\n"
                        "add score 1\n"
                        "sleep(1)\n"
                        "goto game_loop\n";
                    write(fd, pong_script, strlen_custom(pong_script));
                }
                else if (strcmp_custom(kpm_arg.c_str(), "hello") == 0) {
                    // Hello World script
                    const char* hello_script = 
                        "#!/usr/bin/env purr++\n"
                        "# Hello World\n"
                        "print(\"Hello, KekeOS!\")\n"
                        "print(\"Vítej v systému!\")\n"
                        "color(\"cyan\")\n"
                        "print(\"KekeOS Shell v2.7.5\")\n"
                        "color(\"reset\")\n";
                    write(fd, hello_script, strlen_custom(hello_script));
                }
                else if (strcmp_custom(kpm_arg.c_str(), "calc") == 0) {
                    // Calculator script - using assembly-style add/sub
                    const char* calc_script = 
                        "#!/usr/bin/env purr++\n"
                        "# KekeOS Calculator\n"
                        "print(\"=== KEKE CALCULATOR ===\")\n"
                        "print(\"Zadejte první číslo:\")\n"
                        "input num1\n"
                        "print(\"Zadejte druhé číslo:\")\n"
                        "input num2\n"
                        "cls\n"
                        "print(\"Výsledky:\")\n"
                        "print(\"Součet: \")\n"
                        "set result = num1\n"
                        "add result num2\n"
                        "print_var(result)\n"
                        "print(\"Rozdíl: \")\n"
                        "set result = num1\n"
                        "sub result num2\n"
                        "print_var(result)\n"
                        "print(\"Stiskněte Enter pro návrat...\")\n"
                        "input dummy\n";
                    write(fd, calc_script, strlen_custom(calc_script));
                }
                else {
                    // Generic package
                    const char* content = "#!/usr/bin/env kekescript\n# KekeOS Package\nprint(\"Balík ";
                    write(fd, content, strlen_custom(content));
                    write(fd, kpm_arg.c_str(), kpm_arg.length());
                    write(fd, "\")\n", 3);
                }
                close(fd);
            }
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
            std::cout << Colors::YELLOW << "Aktualizuji seznam balíků...\n" << Colors::RESET;
            std::cout << Colors::GREEN << "Seznam balíků aktualizován" << Colors::RESET << "\n";
        }
        else {
            std::cout << Colors::RED << "Neznámý příkaz kpm" << Colors::RESET << "\n";
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
        
        // Check if it's executable or a script
        if (S_ISREG(st.st_mode)) {
            // Try to execute as Purr++
            std::cout << Colors::CYAN << "Spouštím Purr++: " << filename << "\n" << Colors::RESET;
            executeKekeScript(full_path);
        } else {
            std::cout << Colors::RED << "Soubor není spustitelný" << Colors::RESET << "\n";
        }
    }
    
    // Test framebuffer graphics
    void cmdGui() {
        Framebuffer fb;
        if (!fb.init()) {
            std::cout << Colors::RED << "Nepodařilo se inicializovat framebuffer (/dev/fb0)" << Colors::RESET << "\n";
            return;
        }
        
        std::cout << Colors::GREEN << "Framebuffer inicializován: " << fb.getWidth() << "x" << fb.getHeight() << " @ " << fb.getBpp() << "bpp" << Colors::RESET << "\n";
        std::cout << Colors::YELLOW << "Kreslení testovacího obrazu..." << Colors::RESET << "\n";
        
        // Clear screen to blue
        fb.clear(0, 0, 50);
        
        // Draw some shapes
        fb.drawRect(100, 100, 200, 150, 255, 255, 255); // White rectangle
        fb.drawRect(150, 150, 100, 100, 255, 0, 0); // Red square
        fb.drawLine(50, 50, 300, 300, 0, 255, 0); // Green line
        
        // Draw text
        fb.drawString(50, 400, "KekeOS Graphics", 255, 255, 0);
        fb.drawString(50, 420, "Framebuffer Test", 255, 255, 255);
        
        std::cout << Colors::GREEN << "Hotovo. Stiskněte Enter pro návrat do shellu..." << Colors::RESET << "\n";
        std::cin.get();
        
        fb.close();
        std::cout << Colors::GREEN << "Framebuffer uzavřen" << Colors::RESET << "\n";
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
                accessGranted = true;
                
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
    KekeShell() : current_dir("/mnt"), current_text_color(Colors::WHITE), current_bg_color(""), history_count(0), history_index(-1) {
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
                
                std::string input;
                if (!std::getline(std::cin, input)) {
                    shellRunning = false;
                    break;
                }
                
                // Handle arrow keys - detect ANSI escape sequence (27, 91, X)
                // If input starts with 27 (ESC), it's likely an arrow key sequence
                if (!input.empty() && input[0] == 27) {
                    // Arrow key detected, discard the input
                    continue;
                }
                
                // Skip empty input
                if (input.empty()) continue;
                
                // Add to history
                addToHistory(input);
                
                // Parse command (first word)
                size_t space_pos = input.find(' ');
                std::string cmd = (space_pos == std::string::npos) ? input : input.substr(0, space_pos);
                std::string arg = (space_pos == std::string::npos) ? "" : input.substr(space_pos + 1);
                
                // Built-in commands from kernel.c
                if (strcmp_custom(cmd.c_str(), "help") == 0) {
                    printInfo("Prikazy: help, cls, ver, calc, time, exit, reboot, cd, ls, mkdir, rm, touch, cat, kpm, gui, color, origin, windows");
                }
                else if (strcmp_custom(cmd.c_str(), "ver") == 0) {
                    std::cout << Colors::CYAN << "--------------------------------------------\n";
                    std::cout << "Keke Operating System [v2.7.5 - Stable Update]\n";
                    std::cout << "Build Date: Sunday, July 6, 2026\n";
                    std::cout << "Target HW: Intel UHD / Lenovo X380 Yoga\n";
                    std::cout << "Kernel: C++ (Linux-based)\n";
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
                else if (strcmp_custom(cmd.c_str(), "kpm") == 0) {
                    cmdKpm(arg);
                }
                else if (strcmp_custom(cmd.c_str(), "gui") == 0) {
                    cmdGui();
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
                    // Try to execute as external command
                    std::cout << Colors::RED << "-kekeShell: " << input << ": command not found\n" << Colors::RESET;
                }
                
                // Clear any temporary allocations
                input.clear();
                cmd.clear();
                arg.clear();
            }
        }
    }
};

int main() {
    // Create /dev directory for device nodes
    mkdir("/dev", 0755);
    
    // Create /dev/sda device node (SCSI/SATA hard drive)
    // Major: 8, Minor: 0 for sda
    dev_t dev = makedev(8, 0);
    if (mknod("/dev/sda", S_IFBLK | 0660, dev) != 0) {
        std::cout << Colors::RED << "[WARNING] Failed to create /dev/sda device node" << Colors::RESET << "\n";
    } else {
        std::cout << Colors::GREEN << "[OK] Created /dev/sda device node" << Colors::RESET << "\n";
    }
    
    // Mount /dev/sda to /mnt for file system access
    // First create the /mnt directory if it doesn't exist
    mkdir("/mnt", 0755);
    
    // Mount the QEMU virtual disk
    if (mount("/dev/sda", "/mnt", "ext4", 0, nullptr) != 0) {
        // If ext4 fails, try vfat
        if (mount("/dev/sda", "/mnt", "vfat", 0, nullptr) != 0) {
            // If vfat fails, try without filesystem type (auto-detect)
            if (mount("/dev/sda", "/mnt", nullptr, 0, nullptr) != 0) {
                std::cout << Colors::RED << "[WARNING] Failed to mount /dev/sda to /mnt" << Colors::RESET << "\n";
                std::cout << Colors::YELLOW << "File system may not be available." << Colors::RESET << "\n";
            } else {
                std::cout << Colors::GREEN << "[OK] Mounted /dev/sda to /mnt (auto-detect)" << Colors::RESET << "\n";
            }
        } else {
            std::cout << Colors::GREEN << "[OK] Mounted /dev/sda to /mnt (vfat)" << Colors::RESET << "\n";
        }
    } else {
        std::cout << Colors::GREEN << "[OK] Mounted /dev/sda to /mnt (ext4)" << Colors::RESET << "\n";
    }
    
    KekeShell shell;
    shell.run();
    return 0;
}
