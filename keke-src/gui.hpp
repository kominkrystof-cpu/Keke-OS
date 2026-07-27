#ifndef KEKE_GUI_H
#define KEKE_GUI_H

#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

// ============================================================
// COLORS & THEME — Windows XP Luna-style
// ============================================================
namespace GuiTheme {
    // XP Blue taskbar
    constexpr unsigned char TB_R = 60, TB_G = 115, TB_B = 190;
    constexpr unsigned char TB_LIGHT_R = 85, TB_LIGHT_G = 140, TB_LIGHT_B = 220;
    constexpr unsigned char TB_DARK_R = 40, TB_DARK_G = 85, TB_DARK_B = 150;

    // Start button green
    constexpr unsigned char START_R = 60, START_G = 170, START_B = 50;
    constexpr unsigned char START_HOVER_R = 70, START_HOVER_G = 190, START_HOVER_B = 60;

    // Desktop blue
    constexpr unsigned char DESKTOP_R = 18, DESKTOP_G = 80, DESKTOP_B = 160;
    constexpr unsigned char DESKTOP_BOTTOM_R = 10, DESKTOP_BOTTOM_G = 45, DESKTOP_BOTTOM_B = 100;

    // Window title bar (active)
    constexpr unsigned char WIN_TITLE_R = 10, WIN_TITLE_G = 55, WIN_TITLE_B = 140;
    constexpr unsigned char WIN_TITLE_LIGHT_R = 30, WIN_TITLE_LIGHT_G = 95, WIN_TITLE_LIGHT_B = 190;

    // Window title bar (inactive)
    constexpr unsigned char WIN_INACTIVE_R = 120, WIN_INACTIVE_G = 120, WIN_INACTIVE_B = 130;

    // Window chrome
    constexpr unsigned char WIN_BG_R = 236, WIN_BG_G = 233, WIN_BG_B = 216;
    constexpr unsigned char WIN_BORDER_R = 180, WIN_BORDER_G = 180, WIN_BORDER_B = 180;

    // Close button
    constexpr unsigned char CLOSE_R = 220, CLOSE_G = 60, CLOSE_B = 50;
    constexpr unsigned char CLOSE_HOVER_R = 240, CLOSE_HOVER_G = 80, CLOSE_HOVER_B = 70;

    // Text
    constexpr unsigned char WHITE_R = 255, WHITE_G = 255, WHITE_B = 255;
    constexpr unsigned char BLACK_R = 0, BLACK_G = 0, BLACK_B = 0;

    // Sizes
    constexpr int TITLE_HEIGHT = 26;
    constexpr int TASKBAR_HEIGHT = 36;
    constexpr int START_WIDTH = 90;
    constexpr int BORDER = 2;
    constexpr int CLOSE_SIZE = 20;
    constexpr int CLOCK_WIDTH = 60;
    constexpr int MOUSE_W = 12, MOUSE_H = 18;
}

// ============================================================
// RECT
// ============================================================
struct GuiRect {
    int x, y, w, h;
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    void uniteWith(const GuiRect& other) {
        int x1 = (x < other.x) ? x : other.x;
        int y1 = (y < other.y) ? y : other.y;
        int x2 = ((x + w) > (other.x + other.w)) ? (x + w) : (other.x + other.w);
        int y2 = ((y + h) > (other.y + other.h)) ? (y + h) : (other.y + other.h);
        x = x1; y = y1; w = x2 - x1; h = y2 - y1;
    }
};

// ============================================================
// MOUSE CURSOR BITMAP (12x18 arrow)
// ============================================================
static const unsigned char MOUSE_CURSOR[18] = {
    0b10000000,
    0b11000000,
    0b11100000,
    0b11110000,
    0b11111000,
    0b11111100,
    0b11111110,
    0b11111111,
    0b11111111,
    0b11111100,
    0b11001100,
    0b10000110,
    0b00000110,
    0b00000011,
    0b00000011,
    0b00000001,
    0b00000001,
    0b00000000
};

// ============================================================
// CAT PAW CURSOR BITMAP (12x18) — color-indexed
//   0 = transparent
//   1 = orange fur  (255, 165, 0)
//   2 = white pad   (240, 240, 240)
//   3 = pink beans  (255, 150, 180)
// ============================================================
static const unsigned char CAT_PAW[18][12] = {
    {1,0,0,0,0,0,0,0,0,0,0,0}, // arm tip (hotspot)
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,0,0,0,0}, // arm widens into paw
    {1,3,1,3,1,3,1,3,1,0,0,0}, // 4 toe beans
    {1,1,3,3,3,3,3,1,1,0,0,0}, // bean fill
    {1,1,1,1,1,1,1,1,0,0,0,0}, // separator ring
    {1,1,2,2,2,2,2,1,0,0,0,0}, // main pad top
    {1,2,2,2,2,2,2,1,0,0,0,0}, // main pad
    {1,2,2,2,2,2,1,1,0,0,0,0}, // main pad narrows
    {0,1,2,2,2,1,1,0,0,0,0,0}, // pad bottom
    {0,0,1,1,1,1,0,0,0,0,0,0}, // fur tail
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0}
};

// RGB colors for cat paw indices: [0]=transparent(skipped), 1=orange, 2=white, 3=pink
static const unsigned char CAT_PAW_COLORS[4][3] = {
    {  0,   0,   0}, // 0 = transparent (not drawn)
    {255, 165,   0}, // 1 = orange fur
    {240, 240, 240}, // 2 = white pad
    {255, 150, 180}  // 3 = pink beans
};

// ============================================================
// BUTTON STATE
// ============================================================
enum class GuiButtonState { NORMAL, HOVER, PRESSED };

// ============================================================
// GUI MANAGER (forward decl)
// ============================================================
class GuiManager;

// ============================================================
// WINDOW
// ============================================================
class GuiWindow {
public:
    GuiRect rect;
    std::string title;
    bool visible = true;
    bool active = false;
    bool closed = false;
    unsigned char bg_r, bg_g, bg_b;
    GuiRect closeBtn;

    GuiWindow(int x, int y, int w, int h, const std::string& t)
        : rect{x, y, w, h}, title(t), bg_r(GuiTheme::WIN_BG_R), bg_g(GuiTheme::WIN_BG_G), bg_b(GuiTheme::WIN_BG_B) {
        closeBtn = {x + w - GuiTheme::CLOSE_SIZE - 4, y + 3, GuiTheme::CLOSE_SIZE, GuiTheme::CLOSE_SIZE};
        active = true;
    }

    void setPosition(int x, int y) {
        int dx = x - rect.x;
        int dy = y - rect.y;
        rect.x = x;
        rect.y = y;
        closeBtn.x += dx;
        closeBtn.y += dy;
    }

    bool isOnTitleBar(int mx, int my) const {
        return my >= rect.y && my < rect.y + GuiTheme::TITLE_HEIGHT
            && mx >= rect.x && mx < rect.x + rect.w;
    }

    bool isOnClose(int mx, int my) const {
        return closeBtn.contains(mx, my);
    }

    GuiRect bounds() const {
        return {rect.x - 4, rect.y - 4, rect.w + 8, rect.h + 8};
    }

    void draw(class GuiGraphics& g, Framebuffer& fb);
};

// ============================================================
// GRAPHICS PRIMITIVES
// ============================================================
class GuiGraphics {
public:
    Framebuffer* fb;
    int screen_w, screen_h;
    int cursor_style; // 0 = arrow, 1 = cat paw

    GuiGraphics(Framebuffer* f, int w, int h, int cs = 0) : fb(f), screen_w(w), screen_h(h), cursor_style(cs) {}

    void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        fb->setPixel(x, y, r, g, b);
    }

    void fillRect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b) {
        fb->drawRect(x, y, w, h, r, g, b);
    }

    void drawString(int x, int y, const std::string& text, unsigned char r, unsigned char g, unsigned char b) {
        fb->drawString(x, y, text, r, g, b);
    }

    void drawLine(int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b) {
        fb->drawLine(x1, y1, x2, y2, r, g, b);
    }

    // Vertical gradient (top to bottom)
    void fillGradientV(int x, int y, int w, int h,
                       unsigned char r1, unsigned char g1, unsigned char b1,
                       unsigned char r2, unsigned char g2, unsigned char b2) {
        if (h <= 0 || w <= 0) return;
        for (int j = 0; j < h; j++) {
            float t = (float)j / (h - 1);
            unsigned char cr = r1 + (unsigned char)((r2 - r1) * t);
            unsigned char cg = g1 + (unsigned char)((g2 - g1) * t);
            unsigned char cb = b1 + (unsigned char)((b2 - b1) * t);
            for (int i = 0; i < w; i++) {
                fb->setPixel(x + i, y + j, cr, cg, cb);
            }
        }
    }

    // Horizontal gradient (left to right)
    void fillGradientH(int x, int y, int w, int h,
                       unsigned char r1, unsigned char g1, unsigned char b1,
                       unsigned char r2, unsigned char g2, unsigned char b2) {
        if (h <= 0 || w <= 0) return;
        for (int i = 0; i < w; i++) {
            float t = (float)i / (w - 1);
            unsigned char cr = r1 + (unsigned char)((r2 - r1) * t);
            unsigned char cg = g1 + (unsigned char)((g2 - g1) * t);
            unsigned char cb = b1 + (unsigned char)((b2 - b1) * t);
            for (int j = 0; j < h; j++) {
                fb->setPixel(x + i, y + j, cr, cg, cb);
            }
        }
    }

    // Rectangle outline
    void drawRectOutline(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b) {
        drawLine(x, y, x+w-1, y, r, g, b);
        drawLine(x, y+h-1, x+w-1, y+h-1, r, g, b);
        drawLine(x, y, x, y+h-1, r, g, b);
        drawLine(x+w-1, y, x+w-1, y+h-1, r, g, b);
    }

    // 3D sunken border
    void drawSunkenBorder(int x, int y, int w, int h) {
        drawLine(x, y, x+w-1, y, 128, 128, 128);
        drawLine(x, y, x, y+h-1, 128, 128, 128);
        drawLine(x+1, y+1, x+w-2, y+1, 192, 192, 192);
        drawLine(x+1, y+1, x+1, y+h-2, 192, 192, 192);
        drawLine(x, y+h-1, x+w-1, y+h-1, 255, 255, 255);
        drawLine(x+w-1, y, x+w-1, y+h-1, 255, 255, 255);
    }

    // 3D raised border
    void drawRaisedBorder(int x, int y, int w, int h) {
        drawLine(x, y, x+w-1, y, 255, 255, 255);
        drawLine(x, y, x, y+h-1, 255, 255, 255);
        drawLine(x, y+h-1, x+w-1, y+h-1, 128, 128, 128);
        drawLine(x+w-1, y, x+w-1, y+h-1, 128, 128, 128);
    }

    // Window title bar gradient (active)
    void drawTitleBar(int x, int y, int w, bool active) {
        if (active) {
            fillGradientV(x, y, w, GuiTheme::TITLE_HEIGHT,
                          GuiTheme::WIN_TITLE_LIGHT_R, GuiTheme::WIN_TITLE_LIGHT_G, GuiTheme::WIN_TITLE_LIGHT_B,
                          GuiTheme::WIN_TITLE_R, GuiTheme::WIN_TITLE_G, GuiTheme::WIN_TITLE_B);
        } else {
            fillRect(x, y, w, GuiTheme::TITLE_HEIGHT,
                     GuiTheme::WIN_INACTIVE_R, GuiTheme::WIN_INACTIVE_G, GuiTheme::WIN_INACTIVE_B);
        }
    }

    // Close button (red X)
    void drawCloseButton(int x, int y, bool hover) {
        if (hover) {
            fillRect(x, y, GuiTheme::CLOSE_SIZE, GuiTheme::CLOSE_SIZE,
                     GuiTheme::CLOSE_HOVER_R, GuiTheme::CLOSE_HOVER_G, GuiTheme::CLOSE_HOVER_B);
        } else {
            fillRect(x, y, GuiTheme::CLOSE_SIZE, GuiTheme::CLOSE_SIZE,
                     GuiTheme::CLOSE_R, GuiTheme::CLOSE_G, GuiTheme::CLOSE_B);
        }
        // Draw X
        int cx = x + 6, cy = y + 4;
        drawLine(cx, cy, cx + 7, cy + 11, 255, 255, 255);
        drawLine(cx + 1, cy, cx + 8, cy + 11, 255, 255, 255);
        drawLine(cx + 7, cy, cx, cy + 11, 255, 255, 255);
        drawLine(cx + 6, cy, cx - 1, cy + 11, 255, 255, 255);
    }

    // Mouse cursor — supports arrow (0) and cat paw (1)
    void drawCursor(int mx, int my) {
        if (cursor_style == 1) {
            // Cat paw cursor (color-indexed)
            for (int j = 0; j < GuiTheme::MOUSE_H && my + j < screen_h; j++) {
                for (int i = 0; i < GuiTheme::MOUSE_W && mx + i < screen_w; i++) {
                    unsigned char idx = CAT_PAW[j][i];
                    if (idx > 0) {
                        fb->setPixel(mx + i, my + j,
                                     CAT_PAW_COLORS[idx][0],
                                     CAT_PAW_COLORS[idx][1],
                                     CAT_PAW_COLORS[idx][2]);
                    }
                }
            }
        } else {
            // Original arrow cursor (white)
            for (int j = 0; j < GuiTheme::MOUSE_H && my + j < screen_h; j++) {
                for (int i = 0; i < GuiTheme::MOUSE_W && mx + i < screen_w; i++) {
                    if (MOUSE_CURSOR[j] & (0x80 >> i)) {
                        fb->setPixel(mx + i, my + j, 255, 255, 255);
                    }
                }
            }
        }
    }

    // Taskbar
    void drawTaskbar(int y) {
        // Blue gradient background
        fillGradientV(0, y, screen_w, GuiTheme::TASKBAR_HEIGHT,
                      GuiTheme::TB_LIGHT_R, GuiTheme::TB_LIGHT_G, GuiTheme::TB_LIGHT_B,
                      GuiTheme::TB_DARK_R, GuiTheme::TB_DARK_G, GuiTheme::TB_DARK_B);
        // Top highlight line
        drawLine(0, y, screen_w, y, GuiTheme::TB_LIGHT_R, GuiTheme::TB_LIGHT_G, GuiTheme::TB_LIGHT_B);
    }

    // Start button
    void drawStartButton(int x, int y, bool hover, const std::string& label) {
        int w = GuiTheme::START_WIDTH;
        int h = GuiTheme::TASKBAR_HEIGHT - 6;

        // Button background with green gradient
        if (hover) {
            fillGradientV(x + 2, y + 2, w - 4, h - 4,
                          GuiTheme::START_HOVER_R, GuiTheme::START_HOVER_G, GuiTheme::START_HOVER_B,
                          GuiTheme::START_R, GuiTheme::START_G, GuiTheme::START_B);
        } else {
            fillGradientV(x + 2, y + 2, w - 4, h - 4,
                          GuiTheme::START_R, GuiTheme::START_G, GuiTheme::START_B,
                          GuiTheme::START_HOVER_R, GuiTheme::START_HOVER_G, GuiTheme::START_HOVER_B);
        }
        drawRaisedBorder(x + 1, y + 1, w - 2, h - 2);

        // Draw "Keke" orb/logo (a simple circle)
        int orb_x = x + 10, orb_y = y + 6;
        for (int j = -5; j <= 5; j++) {
            for (int i = -5; i <= 5; i++) {
                if (i*i + j*j <= 25) {
                    fb->setPixel(orb_x + i, orb_y + j, 255, 215, 0);
                }
            }
        }

        // Label
        drawString(x + 22, y + 6, label, 255, 255, 255);
    }

    // Start menu panel
    void drawStartMenu(int x, int y, int w, int h, const std::vector<std::string>& items) {
        // Draw panel
        fillRect(x, y, w, h, 236, 233, 216);
        drawRaisedBorder(x, y, w, h);

        // Left side brand stripe
        fillGradientV(x + 2, y + 2, 30, h - 4,
                      GuiTheme::DESKTOP_R, GuiTheme::DESKTOP_G, GuiTheme::DESKTOP_B,
                      GuiTheme::DESKTOP_BOTTOM_R, GuiTheme::DESKTOP_BOTTOM_G, GuiTheme::DESKTOP_BOTTOM_B);

        // "Keke OS" text on brand stripe (vertical, rotated text - just draw normally)
        drawString(x + 5, y + 10, "K", 255, 255, 150);
        drawString(x + 5, y + 22, "E", 255, 255, 150);
        drawString(x + 5, y + 34, "K", 255, 255, 150);
        drawString(x + 5, y + 46, "E", 255, 255, 150);

        // Menu items
        for (size_t i = 0; i < items.size(); i++) {
            int item_y = y + 10 + i * 28;
            // Icon placeholder (small colored square)
            fillRect(x + 38, item_y + 2, 22, 22, 200 + (i * 15), 180 - (i * 10), 220 - (i * 20));
            drawRaisedBorder(x + 38, item_y + 2, 22, 22);
            // Label
            drawString(x + 66, item_y + 6, items[i], 0, 0, 0);
        }

        // Bottom "Start" bar
        fillRect(x + 2, y + h - 22, w - 4, 20, 60, 115, 190);
        drawString(x + 10, y + h - 18, "Keke OS v2.7.5", 255, 255, 255);
    }

    // Desktop icon
    void drawDesktopIcon(int x, int y, const std::string& label, unsigned char ir, unsigned char ig, unsigned char ib) {
        // Icon box
        fillRect(x + 4, y + 4, 32, 32, ir, ig, ib);
        drawRaisedBorder(x + 4, y + 4, 32, 32);
        drawLine(x + 8, y + 8, x + 32, y + 8, ir+30, ig+30, ib+30);

        // Label (word wrap at ~10 chars — just draw what fits)
        drawString(x + 2, y + 40, label, 255, 255, 255);
    }

    // Desktop background
    void drawDesktop() {
        fillGradientV(0, 0, screen_w, screen_h - GuiTheme::TASKBAR_HEIGHT,
                      GuiTheme::DESKTOP_R, GuiTheme::DESKTOP_G, GuiTheme::DESKTOP_B,
                      GuiTheme::DESKTOP_BOTTOM_R, GuiTheme::DESKTOP_BOTTOM_G, GuiTheme::DESKTOP_BOTTOM_B);
    }
};

// ============================================================
// WINDOW DRAW IMPLEMENTATION
// ============================================================
void GuiWindow::draw(GuiGraphics& g, Framebuffer& fb) {
    if (!visible || closed) return;

    // Window shadow
    g.fillRect(rect.x + 3, rect.y + 3, rect.w, rect.h, 0, 0, 0);

    // Window border
    g.fillRect(rect.x, rect.y, rect.w, rect.h,
               GuiTheme::WIN_BORDER_R, GuiTheme::WIN_BORDER_G, GuiTheme::WIN_BORDER_B);

    // Client area
    int client_y = rect.y + GuiTheme::TITLE_HEIGHT + GuiTheme::BORDER;
    int client_h = rect.h - GuiTheme::TITLE_HEIGHT - GuiTheme::BORDER * 2;
    g.fillRect(rect.x + GuiTheme::BORDER, client_y, rect.w - GuiTheme::BORDER * 2, client_h,
               bg_r, bg_g, bg_b);

    // Title bar gradient
    g.drawTitleBar(rect.x + GuiTheme::BORDER, rect.y + GuiTheme::BORDER, rect.w - GuiTheme::BORDER * 2, active);

    // Title text
    int title_x = rect.x + GuiTheme::BORDER + 6;
    int title_y = rect.y + GuiTheme::BORDER + 5;
    g.drawString(title_x, title_y, title, 255, 255, 255);

    // Close button
    g.drawCloseButton(closeBtn.x, closeBtn.y, false);

    // Sunken inner border for client area
    g.drawSunkenBorder(rect.x + GuiTheme::BORDER, client_y,
                       rect.w - GuiTheme::BORDER * 2, client_h);
}

// ============================================================
// GUI MANAGER — event-driven with dirty-region tracking
//
// Architecture:
//   1. State model is mutated by processInput() and processLogic()
//   2. State changes call markDirty() to flag what needs redraw
//   3. render() draws to backbuffer only when dirty
//   4. One buffer flip per dirty cycle via swapBuffers()
//   5. Event loop uses select() to sleep until input arrives
// ============================================================
class GuiManager {
private:
    Framebuffer* fb;
    GuiGraphics* g;
    int screen_w, screen_h;

    // Mouse state
    int mouse_fd = -1;
    int mouse_x, mouse_y;
    int mouse_x_prev, mouse_y_prev;
    bool mouse_left = false;
    bool mouse_right = false;
    bool mouse_left_prev = false;

    // Cursor background save/restore for fast cursor-only updates
    unsigned char* cursor_bg = nullptr;
    int cursor_saved_x = 0, cursor_saved_y = 0;
    bool cursor_bg_valid = false;
    bool force_full_render = true;

    // Windows
    std::vector<GuiWindow*> windows;
    int drag_window = -1;
    int drag_off_x, drag_off_y;
    int active_window = -1;

    // Taskbar / Start
    bool start_hover = false;
    bool start_hover_prev = false;
    bool start_open = false;
    bool start_open_prev = false;
    GuiRect start_btn_rect;
    std::vector<std::string> start_items;

    // Desktop icons
    struct DesktopIcon {
        int x, y;
        std::string label;
        unsigned char r, g, b;
    };
    std::vector<DesktopIcon> desktop_icons;

    // Dirty region tracking
    bool dirty;
    GuiRect dirtyRegion;

    // Clock state (for change detection)
    int last_clock_minute;

    // Running state
    bool running = true;

    // Cursor style: 0 = arrow, 1 = cat paw
    int cursor_style;

public:
    GuiManager(Framebuffer* framebuf, int w, int h, int cs = 0)
        : fb(framebuf), screen_w(w), screen_h(h), cursor_style(cs),
          mouse_x(w/2), mouse_y(h/2),
          mouse_x_prev(w/2), mouse_y_prev(h/2),
          dirty(true), last_clock_minute(-1) {
        g = new GuiGraphics(fb, w, h, cursor_style);

        // Allocate cursor background save buffer
        cursor_bg = new unsigned char[GuiTheme::MOUSE_W * GuiTheme::MOUSE_H * (fb->getBpp() / 8)];

        // Full-screen initial dirty rect
        dirtyRegion = {0, 0, screen_w, screen_h};

        // Setup taskbar start button rect
        start_btn_rect = {4, screen_h - GuiTheme::TASKBAR_HEIGHT + 3, GuiTheme::START_WIDTH, GuiTheme::TASKBAR_HEIGHT - 6};

        // Start menu items
        start_items.push_back("Terminal");
        start_items.push_back("GUI Demo");
        start_items.push_back("Calculator");
        start_items.push_back("Paint");
        start_items.push_back("Notepad");

        // Desktop icons
        desktop_icons.push_back({30, 30, "My Computer", 0, 180, 0});
        desktop_icons.push_back({30, 110, "Recycle Bin", 200, 200, 200});
        desktop_icons.push_back({30, 190, "Keke Shell", 200, 150, 0});
        desktop_icons.push_back({30, 270, "Network", 0, 120, 200});

        // Open mouse device
        mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
        if (mouse_fd >= 0) {
            std::cout << "\033[32m[GUI] Mouse device opened (/dev/input/mice)\033[0m\n";
        } else {
            std::cout << "\033[33m[GUI] No mouse device (/dev/input/mice): " << strerror(errno) << "\033[0m\n";
        }
    }

    ~GuiManager() {
        if (mouse_fd >= 0) close(mouse_fd);
        for (auto w : windows) delete w;
        delete[] cursor_bg;
        delete g;
    }

    void addWindow(GuiWindow* win) {
        windows.push_back(win);
        active_window = windows.size() - 1;
        markDirty(win->bounds());
        force_full_render = true;
    }

    // ================================================================
    // EVENT-DRIVEN MAIN LOOP
    //
    // Uses select() to sleep until input arrives (mouse/keyboard)
    // or a 1-second timeout for clock updates. Only redraws when
    // state has actually changed (dirty flag).
    // ================================================================
    void run() {
        while (running) {
            fd_set fds;
            FD_ZERO(&fds);
            if (mouse_fd >= 0) FD_SET(mouse_fd, &fds);
            FD_SET(STDIN_FILENO, &fds);

            int maxfd = (mouse_fd > STDIN_FILENO ? mouse_fd : STDIN_FILENO) + 1;

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 1000;  // 1ms timeout for responsive mouse

            int ret = select(maxfd, &fds, nullptr, nullptr, &tv);

            // Save state before input for change detection
            bool prev_start_open = start_open;
            int prev_active = active_window;

            if (ret > 0) {
                processInput();
            }

            processClock();

            if (dirty) {
                bool mouse_moved = (mouse_x != mouse_x_prev || mouse_y != mouse_y_prev);
                bool ui_changed = (start_open != prev_start_open || active_window != prev_active || force_full_render);

                if (mouse_moved && !ui_changed) {
                    // Fast cursor-only path: save/restore cursor background
                    renderCursorOnly();
                    int sx = (mouse_x_prev < mouse_x) ? mouse_x_prev : mouse_x;
                    int sy = (mouse_y_prev < mouse_y) ? mouse_y_prev : mouse_y;
                    int ex = ((mouse_x_prev + GuiTheme::MOUSE_W) > (mouse_x + GuiTheme::MOUSE_W))
                             ? (mouse_x_prev + GuiTheme::MOUSE_W) : (mouse_x + GuiTheme::MOUSE_W);
                    int ey = ((mouse_y_prev + GuiTheme::MOUSE_H) > (mouse_y + GuiTheme::MOUSE_H))
                             ? (mouse_y_prev + GuiTheme::MOUSE_H) : (mouse_y + GuiTheme::MOUSE_H);
                    fb->swapBuffersRegion(sx, sy, ex - sx, ey - sy);
                } else {
                    // Full render path
                    render();
                    fb->swapBuffers();
                }
                dirty = false;
                force_full_render = false;
            }
        }
    }

private:
    // ================================================================
    // markDirty — state changes call this to flag regions for redraw
    // ================================================================
    void markDirty(const GuiRect& r) {
        if (!dirty) {
            dirtyRegion = r;
            dirty = true;
        } else {
            dirtyRegion.uniteWith(r);
        }
    }

    void markDirtyFull() {
        dirtyRegion = {0, 0, screen_w, screen_h};
        dirty = true;
    }

    // ================================================================
    // processInput — read all pending mouse/keyboard, mutate state
    // ================================================================
    void processInput() {
        // Save previous cursor position for dirty tracking
        mouse_x_prev = mouse_x;
        mouse_y_prev = mouse_y;
        mouse_left_prev = mouse_left;

        // ---- Read all pending mouse events ----
        if (mouse_fd >= 0) {
            unsigned char buf[3];
            while (read(mouse_fd, buf, 3) == 3) {
                int dx = (int)(char)buf[1];
                int dy = (int)(char)buf[2];
                mouse_x += dx;
                mouse_y -= dy;  // Y is inverted in PS/2 protocol

                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= screen_w) mouse_x = screen_w - 1;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= screen_h) mouse_y = screen_h - 1;

                mouse_left = (buf[0] & 0x01) != 0;
                mouse_right = (buf[0] & 0x02) != 0;
            }

            // Mark old + new cursor region dirty
            if (mouse_x != mouse_x_prev || mouse_y != mouse_y_prev) {
                GuiRect old_cursor = {mouse_x_prev, mouse_y_prev, GuiTheme::MOUSE_W, GuiTheme::MOUSE_H};
                GuiRect new_cursor = {mouse_x, mouse_y, GuiTheme::MOUSE_W, GuiTheme::MOUSE_H};
                markDirty(old_cursor);
                markDirty(new_cursor);
            }
        }

        // ---- Read keyboard ----
        fd_set kbd_fds;
        struct timeval kbd_tv = {0, 0};
        FD_ZERO(&kbd_fds);
        FD_SET(STDIN_FILENO, &kbd_fds);

        if (select(STDIN_FILENO + 1, &kbd_fds, nullptr, nullptr, &kbd_tv) > 0) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                if (ch == 27) {
                    char seq[2];
                    if (read(STDIN_FILENO, seq, 2) == 2) {
                        // Arrow key or other escape sequence, ignore
                    } else {
                        // Single ESC = exit
                        running = false;
                    }
                }
            }
        }

        // ---- Handle mouse click logic ----
        processClicks();

        // ---- Handle window dragging ----
        processDrag();

        // ---- Update hover states (check if changed) ----
        bool new_start_hover = start_btn_rect.contains(mouse_x, mouse_y);
        if (new_start_hover != start_hover) {
            start_hover = new_start_hover;
            markDirty(start_btn_rect);
        }
    }

    // ================================================================
    // processClicks — handle mouse button press logic
    // ================================================================
    void processClicks() {
        if (!mouse_left || mouse_left_prev) return;  // Only on press, not hold

        // Check start button
        if (start_btn_rect.contains(mouse_x, mouse_y)) {
            start_open = !start_open;
            force_full_render = true;
            // Mark start button + potential menu region dirty
            markDirty(start_btn_rect);
            if (start_open) {
                int menu_y = screen_h - GuiTheme::TASKBAR_HEIGHT - 250;
                markDirty({start_btn_rect.x, menu_y, 220, 250});
            } else {
                // Menu was open, now closed — redraw whole taskbar + menu area
                int menu_y = screen_h - GuiTheme::TASKBAR_HEIGHT - 250;
                markDirty({0, menu_y, screen_w, screen_h - menu_y});
            }
            return;
        }

        // If start menu is open, check if clicking outside or on an item
        if (start_open) {
            int menu_x = start_btn_rect.x;
            int menu_y = screen_h - GuiTheme::TASKBAR_HEIGHT - 250;
            GuiRect menu_rect = {menu_x, menu_y, 220, 250};
            if (!menu_rect.contains(mouse_x, mouse_y)) {
                start_open = false;
                force_full_render = true;
                markDirtyFull();
                return;
            }

            // Check menu item clicks
            for (size_t i = 0; i < start_items.size(); i++) {
                int item_y = menu_y + 10 + i * 28;
                GuiRect item_rect = {menu_x + 38, item_y, 170, 22};
                if (item_rect.contains(mouse_x, mouse_y)) {
                    launchApp(start_items[i]);
                    start_open = false;
                    force_full_render = true;
                    markDirtyFull();
                    return;
                }
            }
            return;
        }

        // Check windows (top to bottom)
        for (int i = (int)windows.size() - 1; i >= 0; i--) {
            auto* win = windows[i];
            if (!win->visible || win->closed) continue;

            // Check close button
            if (win->isOnClose(mouse_x, mouse_y)) {
                GuiRect old_bounds = win->bounds();
                win->closed = true;
                force_full_render = true;
                if (active_window == i) active_window = -1;
                markDirty(old_bounds);
                return;
            }

            // Check title bar for dragging
            if (win->isOnTitleBar(mouse_x, mouse_y)) {
                drag_window = i;
                drag_off_x = mouse_x - win->rect.x;
                drag_off_y = mouse_y - win->rect.y;
                // Bring to front
                if (i != (int)windows.size() - 1) {
                    auto* w = windows[i];
                    windows.erase(windows.begin() + i);
                    windows.push_back(w);
                    active_window = windows.size() - 1;
                    drag_window = windows.size() - 1;
                    force_full_render = true;
                    markDirtyFull();  // Z-order change affects everything
                }
                return;
            }

            // Check client area — bring to front
            if (mouse_x >= win->rect.x && mouse_x < win->rect.x + win->rect.w &&
                mouse_y >= win->rect.y && mouse_y < win->rect.y + win->rect.h) {
                if (i != (int)windows.size() - 1) {
                    auto* w = windows[i];
                    windows.erase(windows.begin() + i);
                    windows.push_back(w);
                    active_window = windows.size() - 1;
                    force_full_render = true;
                    markDirtyFull();
                }
                return;
            }
        }
    }

    // ================================================================
    // processDrag — handle window dragging
    // ================================================================
    void processDrag() {
        // Stop drag when button released
        if (!mouse_left && drag_window >= 0) {
            drag_window = -1;
        }

        if (mouse_left && drag_window >= 0 && drag_window < (int)windows.size()) {
            auto* win = windows[drag_window];
            force_full_render = true;

            // Mark old window position dirty
            markDirty(win->bounds());

            int new_x = mouse_x - drag_off_x;
            int new_y = mouse_y - drag_off_y;
            // Keep window at least partially visible
            if (new_x < -win->rect.w + 50) new_x = -win->rect.w + 50;
            if (new_x > screen_w - 50) new_x = screen_w - 50;
            if (new_y < -40) new_y = -40;
            if (new_y > screen_h - GuiTheme::TASKBAR_HEIGHT - 30) new_y = screen_h - GuiTheme::TASKBAR_HEIGHT - 30;
            win->setPosition(new_x, new_y);

            // Mark new window position dirty
            markDirty(win->bounds());
            // Also dirty taskbar (window button moves)
            markDirty({0, screen_h - GuiTheme::TASKBAR_HEIGHT, screen_w, GuiTheme::TASKBAR_HEIGHT});
        }
    }

    // ================================================================
    // processClock — check if clock minute changed, mark dirty
    // ================================================================
    void processClock() {
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        int current_minute = tm->tm_hour * 60 + tm->tm_min;

        if (current_minute != last_clock_minute) {
            last_clock_minute = current_minute;
            int clock_x = screen_w - GuiTheme::CLOCK_WIDTH - 8;
            int clock_y = screen_h - GuiTheme::TASKBAR_HEIGHT + 10;
            markDirty({clock_x - 4, clock_y - 4, GuiTheme::CLOCK_WIDTH + 8, 24});
        }
    }

    // ================================================================
    // renderCursorOnly — fast path: only update cursor in backbuffer
    // Saves background under old cursor, restores it, saves new area,
    // draws new cursor. No full desktop redraw needed.
    // ================================================================
    void renderCursorOnly() {
        int bpp = fb->getBpp() / 8;
        int line_len = fb->getLineLength();
        unsigned char* buf = fb->getBackbuffer();

        // 1. Restore old cursor background in backbuffer
        if (cursor_bg_valid) {
            for (int j = 0; j < GuiTheme::MOUSE_H; j++) {
                int y = cursor_saved_y + j;
                if (y >= screen_h) break;
                for (int i = 0; i < GuiTheme::MOUSE_W; i++) {
                    int x = cursor_saved_x + i;
                    if (x >= screen_w) break;
                    memcpy(buf + y * line_len + x * bpp,
                           cursor_bg + (j * GuiTheme::MOUSE_W + i) * bpp, bpp);
                }
            }
        }

        // 2. Save new cursor background from backbuffer
        cursor_saved_x = mouse_x;
        cursor_saved_y = mouse_y;
        for (int j = 0; j < GuiTheme::MOUSE_H; j++) {
            int y = cursor_saved_y + j;
            if (y >= screen_h) break;
            for (int i = 0; i < GuiTheme::MOUSE_W; i++) {
                int x = cursor_saved_x + i;
                if (x >= screen_w) break;
                memcpy(cursor_bg + (j * GuiTheme::MOUSE_W + i) * bpp,
                       buf + y * line_len + x * bpp, bpp);
            }
        }
        cursor_bg_valid = true;

        // 3. Draw new cursor on top
        g->drawCursor(mouse_x, mouse_y);
    }

    // ================================================================
    // launchApp — open an application window
    // ================================================================
    void launchApp(const std::string& app) {
        if (app == "Terminal") {
            auto* win = new GuiWindow(120, 60, 500, 320, "Terminal - Keke Shell");
            win->bg_r = 0; win->bg_g = 0; win->bg_b = 0;
            windows.push_back(win);
            active_window = windows.size() - 1;
            markDirty(win->bounds());
        } else if (app == "GUI Demo") {
            auto* win = new GuiWindow(200, 100, 400, 250, "GUI Demo");
            windows.push_back(win);
            active_window = windows.size() - 1;
            markDirty(win->bounds());
        } else if (app == "Calculator") {
            auto* win = new GuiWindow(150, 150, 300, 240, "Calculator");
            windows.push_back(win);
            active_window = windows.size() - 1;
            markDirty(win->bounds());
        } else if (app == "Notepad") {
            auto* win = new GuiWindow(180, 80, 450, 350, "Notepad");
            windows.push_back(win);
            active_window = windows.size() - 1;
            markDirty(win->bounds());
        } else if (app == "Paint") {
            auto* win = new GuiWindow(100, 50, 550, 400, "Keke Paint");
            win->bg_r = 255; win->bg_g = 255; win->bg_b = 255;
            windows.push_back(win);
            active_window = windows.size() - 1;
            markDirty(win->bounds());
        }
    }

    // ================================================================
    // render — draw to backbuffer (called only when dirty)
    // ================================================================
    void render() {
        // 1. Desktop background
        g->drawDesktop();

        // 2. Desktop icons
        for (auto& icon : desktop_icons) {
            g->drawDesktopIcon(icon.x, icon.y, icon.label, icon.r, icon.g, icon.b);
        }

        // 3. Windows (back to front)
        for (size_t i = 0; i < windows.size(); i++) {
            auto* win = windows[i];
            if (i == (size_t)active_window) {
                win->active = true;
            } else {
                win->active = false;
            }
            win->draw(*g, *fb);
        }

        // 4. Draw window content for special apps
        for (auto* win : windows) {
            if (win->closed || !win->visible) continue;
            int client_x = win->rect.x + GuiTheme::BORDER + 2;
            int client_y = win->rect.y + GuiTheme::TITLE_HEIGHT + GuiTheme::BORDER + 2;
            int client_w = win->rect.w - GuiTheme::BORDER * 2 - 4;
            int client_h = win->rect.h - GuiTheme::TITLE_HEIGHT - GuiTheme::BORDER * 2 - 4;

            if (win->title == "Terminal - Keke Shell") {
                g->drawString(client_x + 5, client_y + 5, "Keke OS v2.7.5", 0, 255, 0);
                g->drawString(client_x + 5, client_y + 18, "keke@os ~$ ", 0, 255, 0);
                g->drawString(client_x + 5, client_y + 35, "Welcome to Keke Terminal!", 180, 180, 180);
                g->drawString(client_x + 5, client_y + 52, "Type 'help' for commands.", 180, 180, 180);
            } else if (win->title == "Calculator") {
                g->fillRect(client_x, client_y, client_w, 40, 255, 255, 255);
                g->drawString(client_x + client_w - 60, client_y + 12, "0", 0, 0, 0);
                const char* buttons = "789/456*123-0.=+";
                for (int bi = 0; bi < 16; bi++) {
                    int bx = client_x + 5 + (bi % 4) * 35;
                    int by = client_y + 50 + (bi / 4) * 30;
                    g->fillRect(bx, by, 30, 25, 240, 240, 240);
                    g->drawRaisedBorder(bx, by, 30, 25);
                    char btn_str[2] = {buttons[bi], 0};
                    g->drawString(bx + 10, by + 6, btn_str, 0, 0, 0);
                }
            } else if (win->title == "Notepad") {
                g->drawString(client_x + 5, client_y + 5, "Keke Notepad", 0, 0, 200);
                for (int li = 0; li < 12; li++) {
                    int ly = client_y + 30 + li * 18;
                    g->drawLine(client_x + 5, ly + 16, client_x + client_w - 10, ly + 16,
                                200, 200, 255);
                }
            } else if (win->title == "GUI Demo") {
                g->drawString(client_x + 10, client_y + 10, "Keke OS GUI Demo", 0, 0, 0);
                g->drawString(client_x + 10, client_y + 28, "Welcome to the Keke GUI!", 0, 60, 180);
                g->fillRect(client_x + 20, client_y + 55, 40, 40, 255, 0, 0);
                g->fillRect(client_x + 70, client_y + 55, 40, 40, 0, 255, 0);
                g->fillRect(client_x + 120, client_y + 55, 40, 40, 0, 0, 255);
                g->fillRect(client_x + 170, client_y + 55, 40, 40, 255, 255, 0);
                g->drawString(client_x + 25, client_y + 100, "RGB Demo", 100, 100, 100);
            } else if (win->title == "Keke Paint") {
                g->fillRect(client_x + 5, client_y + 5, client_w - 10, 30, 180, 180, 180);
                unsigned char colors[][3] = {{0,0,0},{255,0,0},{0,255,0},{0,0,255},{255,255,0},{255,0,255},{0,255,255},{255,255,255}};
                for (int ci = 0; ci < 8; ci++) {
                    g->fillRect(client_x + 10 + ci * 28, client_y + 8, 24, 24, colors[ci][0], colors[ci][1], colors[ci][2]);
                    g->drawRaisedBorder(client_x + 10 + ci * 28, client_y + 8, 24, 24);
                }
                g->fillRect(client_x + 5, client_y + 40, client_w - 10, client_h - 50, 255, 255, 255);
                for (int di = 0; di < 20; di++) {
                    int dx = client_x + 30 + (di * 20);
                    int dy = client_y + 60 + (di * 10);
                    g->setPixel(dx, dy, 100 + di * 5, 50, 200 - di * 5);
                }
                int face_x = client_x + 200, face_y = client_y + 100;
                for (int a = 0; a < 360; a += 10) {
                    float rad = a * 3.14159f / 180.0f;
                    int fx = face_x + (int)(30 * cos(rad));
                    int fy = face_y + (int)(30 * sin(rad));
                    g->setPixel(fx, fy, 255, 200, 0);
                }
                g->fillRect(face_x - 10, face_y - 8, 6, 6, 0, 0, 0);
                g->fillRect(face_x + 6, face_y - 8, 6, 6, 0, 0, 0);
                for (int si = -12; si <= 12; si++) {
                    int sy = face_y + 6 + (si * si) / 18;
                    g->setPixel(face_x + si, sy, 0, 0, 0);
                }
            }
        }

        // 5. Start menu (if open)
        if (start_open) {
            int menu_x = start_btn_rect.x;
            int menu_y = screen_h - GuiTheme::TASKBAR_HEIGHT - 250;
            g->drawStartMenu(menu_x, menu_y, 220, 250, start_items);
        }

        // 6. Taskbar (drawn on top of windows)
        g->drawTaskbar(screen_h - GuiTheme::TASKBAR_HEIGHT);

        // Start button
        g->drawStartButton(start_btn_rect.x, screen_h - GuiTheme::TASKBAR_HEIGHT + 3,
                           start_hover, "start");

        // Taskbar window buttons
        int btn_x = start_btn_rect.x + start_btn_rect.w + 10;
        for (size_t i = 0; i < windows.size(); i++) {
            auto* win = windows[i];
            if (win->closed) continue;
            int bw = 130;
            if (btn_x + bw > screen_w - GuiTheme::CLOCK_WIDTH - 10) {
                bw = screen_w - GuiTheme::CLOCK_WIDTH - 10 - btn_x;
                if (bw < 20) break;
            }
            g->fillRect(btn_x, screen_h - GuiTheme::TASKBAR_HEIGHT + 4, bw, GuiTheme::TASKBAR_HEIGHT - 8,
                        80, 135, 210);
            if ((int)i == active_window) {
                g->drawSunkenBorder(btn_x, screen_h - GuiTheme::TASKBAR_HEIGHT + 4, bw, GuiTheme::TASKBAR_HEIGHT - 8);
            } else {
                g->drawRaisedBorder(btn_x, screen_h - GuiTheme::TASKBAR_HEIGHT + 4, bw, GuiTheme::TASKBAR_HEIGHT - 8);
            }
            std::string label = win->title;
            if ((int)label.length() > 18) label = label.substr(0, 16) + "..";
            g->drawString(btn_x + 5, screen_h - GuiTheme::TASKBAR_HEIGHT + 12, label, 255, 255, 255);
            btn_x += bw + 4;
        }

        // 7. Clock (simple HH:MM)
        time_t now = time(nullptr);
        struct tm* tm = localtime(&now);
        char time_str[6];
        time_str[0] = '0' + tm->tm_hour / 10;
        time_str[1] = '0' + tm->tm_hour % 10;
        time_str[2] = ':';
        time_str[3] = '0' + tm->tm_min / 10;
        time_str[4] = '0' + tm->tm_min % 10;
        time_str[5] = 0;

        int clock_x = screen_w - GuiTheme::CLOCK_WIDTH - 8;
        int clock_y = screen_h - GuiTheme::TASKBAR_HEIGHT + 10;
        g->drawRectOutline(clock_x - 2, clock_y - 2, GuiTheme::CLOCK_WIDTH + 4, 20, 120, 170, 220);
        g->drawString(clock_x + (GuiTheme::CLOCK_WIDTH - 40) / 2, clock_y, time_str, 255, 255, 255);

        // 8. Save cursor background (before drawing cursor, for ghost-free cursor-only updates)
        {
            int bpp = fb->getBpp() / 8;
            int line_len = fb->getLineLength();
            unsigned char* buf = fb->getBackbuffer();
            cursor_saved_x = mouse_x;
            cursor_saved_y = mouse_y;
            for (int j = 0; j < GuiTheme::MOUSE_H && cursor_saved_y + j < screen_h; j++) {
                for (int i = 0; i < GuiTheme::MOUSE_W && cursor_saved_x + i < screen_w; i++) {
                    int sy = cursor_saved_y + j;
                    int sx = cursor_saved_x + i;
                    memcpy(cursor_bg + (j * GuiTheme::MOUSE_W + i) * bpp,
                           buf + sy * line_len + sx * bpp, bpp);
                }
            }
            cursor_bg_valid = true;
        }

        // 9. Mouse cursor (always on top)
        g->drawCursor(mouse_x, mouse_y);
    }
};

#endif // KEKE_GUI_H
