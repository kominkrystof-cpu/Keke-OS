/* kernel.c - Keke OS Native (C# Cosmos port) */
#include <stdint.h>
#include "reboot.h"
#include "font.h"
#include "fb_text.h"
#include "ata.h"
// ============================================================
// MULTIBOOT INFO STRUKTURA
// ============================================================
typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint8_t  syms[16];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;   // fyzická adresa bufferu
    uint32_t framebuffer_pitch;  // bajtů na řádek
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;    // bitů na pixel
    uint8_t  framebuffer_type;   // 1 = RGB, 2 = textový
    uint8_t  color_info[6];
} multiboot_info_t;
// ============================================================
// FRAMEBUFFER DRIVER
// ============================================================
typedef struct {
    uint32_t* addr;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint8_t   bpp;
    uint8_t   active;
} framebuffer_t;

static framebuffer_t fb = {0};

// ============================================================
// AHCI STRUCTURES
// ============================================================
/* HBA Port Register Set */
typedef struct __attribute__((packed)) {
    uint32_t clb;        // 0x00: Command List Base Address
    uint32_t clbu;       // 0x04: Command List Base Address Upper 32 bits
    uint32_t fb;         // 0x08: FIS Base Address
    uint32_t fbu;        // 0x0C: FIS Base Address Upper 32 bits
    uint32_t is;         // 0x10: Interrupt Status
    uint32_t ie;         // 0x14: Interrupt Enable
    uint32_t cmd;        // 0x18: Command and Status
    uint32_t reserved0;  // 0x1C: Reserved
    uint32_t tfd;        // 0x20: Task File Data
    uint32_t sig;        // 0x24: Signature
    uint32_t ssts;       // 0x28: SATA Status (SCR0: SStatus)
    uint32_t sctl;       // 0x2C: SATA Control (SCR2: SControl)
    uint32_t serr;       // 0x30: SATA Error (SCR1: SError)
    uint32_t sact;       // 0x34: SATA Active (SCR3: SActive)
    uint32_t ci;         // 0x38: Command Issue
    uint32_t sntf;       // 0x3C: SATA Notification (SCR4: SNotification)
    uint32_t fbs;        // 0x40: FIS-Based Switching Control
    uint32_t reserved1[11]; // 0x44-0x6F: Reserved
    uint32_t vendor[4];  // 0x70-0x7F: Vendor Specific
} hba_port_t;

/* FIS Type Definitions */
#define FIS_TYPE_REG_H2D 0x27
#define FIS_TYPE_REG_D2H 0x34
#define FIS_TYPE_DMA_ACT 0x39
#define FIS_TYPE_DMA_SETUP 0x41
#define FIS_TYPE_DATA 0x46
#define FIS_TYPE_BIST 0x58
#define FIS_TYPE_PIO_SETUP 0x5F
#define FIS_TYPE_DEV_BITS 0xA1

/* Register Host-to-Device FIS */
typedef struct __attribute__((packed)) {
    uint8_t fis_type;    // 0x00: FIS Type (0x27 for REG_H2D)
    uint8_t pm_port_n;   // 0x01: Port multiplier
    uint8_t c;           // 0x02: Command register update
    uint8_t command;     // 0x03: ATA Command
    uint8_t features_l;  // 0x04: Features (low byte)
    uint8_t features_h;  // 0x05: Features (high byte)
    uint8_t lba0;        // 0x06: LBA low byte
    uint8_t lba1;        // 0x07: LBA mid byte
    uint8_t lba2;        // 0x08: LBA high byte
    uint8_t device;      // 0x09: Device register
    uint8_t lba3;        // 0x0A: LBA (exp)
    uint8_t lba4;        // 0x0B: LBA (exp)
    uint8_t lba5;        // 0x0C: LBA (exp)
    uint8_t count_l;     // 0x0D: Sector count (low)
    uint8_t count_h;     // 0x0E: Sector count (high)
    uint8_t icc;         // 0x0F: Interface control
    uint8_t control;     // 0x10: Control
    uint8_t reserved[4]; // 0x11-0x14: Reserved
} fis_reg_h2d_t;

/* Register Device-to-Host FIS */
typedef struct __attribute__((packed)) {
    uint8_t fis_type;    // 0x00: FIS Type (0x34 for REG_D2H)
    uint8_t pm_port_n;   // 0x01: Port multiplier
    uint8_t interrupt;    // 0x02: Interrupt bit
    uint8_t status;      // 0x03: Status register
    uint8_t error;       // 0x04: Error register
    uint8_t lba0;        // 0x05: LBA low byte
    uint8_t lba1;        // 0x06: LBA mid byte
    uint8_t lba2;        // 0x07: LBA high byte
    uint8_t device;      // 0x08: Device register
    uint8_t lba3;        // 0x09: LBA (exp)
    uint8_t lba4;        // 0x0A: LBA (exp)
    uint8_t lba5;        // 0x0B: LBA (exp)
    uint8_t count_l;     // 0x0C: Sector count (low)
    uint8_t count_h;     // 0x0D: Sector count (high)
    uint8_t reserved;    // 0x0E: Reserved
    uint8_t e_status;    // 0x0F: Extended status
    uint8_t reserved2[4]; // 0x10-0x13: Reserved
    uint32_t reserved3;  // 0x14-0x17: Reserved
} fis_reg_d2h_t;

/* Command Header */
typedef struct __attribute__((packed)) {
    uint8_t cfl;         // 0x00: Command FIS length (in DWORDs)
    uint8_t a;           // 0x01: ATAPI
    uint8_t w;           // 0x02: Write
    uint8_t p;           // 0x03: Prefetchable
    uint8_t r;           // 0x04: Reset
    uint8_t b;           // 0x05: BIST
    uint8_t c;           // 0x06: Clear busy upon R_OK
    uint8_t rsv0;        // 0x07: Reserved
    uint16_t prdtl;      // 0x08-0x09: Physical Region Descriptor Table Length
    uint16_t v;          // 0x0A-0x0B: Vendor specific
    uint32_t prdbc;      // 0x0C-0x0F: Physical Region Descriptor Byte Count
    uint64_t ctba;       // 0x10-0x17: Command Table Base Address
    uint32_t ctbau;      // 0x18-0x1B: Command Table Base Address Upper 32 bits
    uint32_t rsv1[4];    // 0x1C-0x2B: Reserved
} hba_cmd_header_t;

/* PRDT (Physical Region Descriptor Table) Entry */
typedef struct __attribute__((packed)) {
    uint32_t dba;        // 0x00: Data Base Address
    uint32_t dbau;       // 0x04: Data Base Address Upper 32 bits
    uint32_t reserved;   // 0x08: Reserved
    uint32_t dbc;        // 0x0C: Data Byte Count (bit 0 = interrupt on completion)
} hba_prdt_entry_t;

/* Command Table */
typedef struct __attribute__((packed)) {
    fis_reg_h2d_t cfis;   // 0x00: Command FIS
    uint8_t acmd[16];     // 0x40: ATAPI Command (12 or 16 bytes)
    uint8_t reserved[32]; // 0x50: Reserved
    hba_prdt_entry_t prdt; // 0x80: PRDT entries
} hba_cmd_table_t;

/* HBA Memory Register Set */
typedef struct __attribute__((packed)) {
    uint32_t cap;        // 0x00: Host Capabilities
    uint32_t ghc;        // 0x04: Global HBA Control
    uint32_t is;         // 0x08: Interrupt Status
    uint32_t pi;         // 0x0C: Ports Implemented
    uint32_t vs;         // 0x10: AHCI Version
    uint32_t ccc_ctl;    // 0x14: Command Completion Coalescing Control
    uint32_t ccc_pts;   // 0x18: Command Completion Coalescing Ports
    uint32_t em_loc;     // 0x1C: Enclosure Management Location
    uint32_t em_ctl;     // 0x20: Enclosure Management Control
    uint32_t cap2;       // 0x24: Host Capabilities Extended
    uint32_t bohc;       // 0x28: BIOS/OS Handoff Control and Status
    uint8_t  rsv[116];   // 0x2C-0x9F: Reserved
    uint8_t  vendor[96]; // 0xA0-0xFF: Vendor Specific
    hba_port_t ports[32]; // 0x100-0x10FF: Port Control Registers
} hba_mem_t;

void fb_init(multiboot_info_t* mbi) {
    // Bit 12 = GRUB předal framebuffer info
    if (!(mbi->flags & (1 << 12))) return;
    // type 1 = RGB linear framebuffer
    if (mbi->framebuffer_type != 1) return;

    fb.addr   = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
    fb.width  = mbi->framebuffer_width;
    fb.height = mbi->framebuffer_height;
    fb.pitch  = mbi->framebuffer_pitch;
    fb.bpp    = mbi->framebuffer_bpp;
    fb.active = 1;

    // Vyčisti obrazovku na černo
    for (uint32_t y = 0; y < fb.height; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)fb.addr + y * fb.pitch);
        for (uint32_t x = 0; x < fb.width; x++) {
            row[x] = 0x00000000;
        }
    }
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.active) return;
    if (x >= fb.width || y >= fb.height) return;
    uint32_t* pixel = (uint32_t*)((uint8_t*)fb.addr + y * fb.pitch + x * (fb.bpp / 8));
    *pixel = color;
}

void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t dy = 0; dy < h; dy++)
        for (uint32_t dx = 0; dx < w; dx++)
            put_pixel(x + dx, y + dy, color);
}

// Testovací gradient – vizuální důkaz že FB funguje
void fb_test(void) {
    for (uint32_t y = 0; y < fb.height; y++)
        for (uint32_t x = 0; x < fb.width; x++)
            put_pixel(x, y, (x * 255 / fb.width) |
                            ((y * 255 / fb.height) << 8) |
                            (0x80 << 16));
}
// Getter funkce pro fb_text.c
static inline uint32_t fb_get_width(void)  { return fb.width; }
static inline uint32_t fb_get_height(void) { return fb.height; }
static inline uint32_t fb_get_pitch(void)  { return fb.pitch; }
static inline void*    fb_get_addr(void)   { return fb.addr; }
/* VGA textový buffer na adrese 0xB8000 */
volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;

/* VGA rozměry */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Barvy */
enum VGA_COLOR {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW = 14,
    VGA_COLOR_WHITE = 15,
};

/* Vytvoření VGA entry (znak + barva) */
uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

/* Pozice kurzoru */
int cursor_row = 0;
int cursor_col = 0;

/* Výpis znaku na aktuální pozici kurzoru */
static uint32_t vga_color_to_rgb(uint8_t color) {
    switch (color) {
        case VGA_COLOR_BLACK: return 0x000000;
        case VGA_COLOR_BLUE: return 0x0000AA;
        case VGA_COLOR_GREEN: return 0x00AA00;
        case VGA_COLOR_CYAN: return 0x00AAAA;
        case VGA_COLOR_RED: return 0xAA0000;
        case VGA_COLOR_MAGENTA: return 0xAA00AA;
        case VGA_COLOR_BROWN: return 0xAA5500;
        case VGA_COLOR_LIGHT_GREY: return 0xAAAAAA;
        case VGA_COLOR_DARK_GREY: return 0x555555;
        case VGA_COLOR_LIGHT_BLUE: return 0x5555FF;
        case VGA_COLOR_LIGHT_GREEN: return 0x00FF00;
        case VGA_COLOR_LIGHT_CYAN: return 0x55FFFF;
        case VGA_COLOR_LIGHT_RED: return 0xFF5555;
        case VGA_COLOR_LIGHT_MAGENTA: return 0xFF55FF;
        case VGA_COLOR_YELLOW: return 0xFFFF55;
        case VGA_COLOR_WHITE: return 0xFFFFFF;
        default: return 0xFFFFFF;
    }
}

void putchar(char c, uint8_t color) {
    if (fb.active) {
        fb_putc(c, vga_color_to_rgb(color));
        return;
    }

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) {
            scroll();
        }
        update_cursor(cursor_row, cursor_col);
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', VGA_COLOR_BLACK);
            update_cursor(cursor_row, cursor_col);
        }
        return;
    }

    vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, color);
    cursor_col++;

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) {
            scroll();
        }
    }
    update_cursor(cursor_row, cursor_col);
}

/* Výpis řetězce */
void print(const char* str, uint8_t color) {
    int i = 0;
    while (str[i] != 0) {
        putchar(str[i], color);
        i++;
    }
}

/* Scrollování obrazovky o jeden řádek nahoru */
void scroll() {
    // Pokud je framebuffer aktivni, preskoc VGA scroll
    if (fb.active) return;

    /* Bezpečnostní kontrola - ověření VGA bufferu */
    if (vga_buffer == 0) return;

    /* Posun všech řádků o jeden nahoru (každý řádek je 80 znaků = 160 bajtů) */
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    }

    /* Vyčištění posledního řádku */
    int last_line_start = (VGA_HEIGHT - 1) * VGA_WIDTH;
    for (int i = 0; i < VGA_WIDTH; i++) {
        vga_buffer[last_line_start + i] = vga_entry(' ', VGA_COLOR_BLACK);
    }

    /* Kurzor zůstane na posledním řádku */
    cursor_row = VGA_HEIGHT - 1;
}

/* Vyčištění obrazovky */
void clear_screen() {
    if (fb.active) {
        fb_clear(0x000000);
        return;
    }

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', VGA_COLOR_BLACK);
    }
    cursor_row = 0;
    cursor_col = 0;
    update_cursor(0, 0);
}

/* Čtení z I/O portu */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Zápis do I/O portu */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/* Čtení 32-bit hodnoty z I/O portu */
static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Zápis 32-bit hodnoty do I/O portu */
static inline void outl(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

/* Aktualizace hardwarového kurzoru */
void update_cursor(int row, int col) {
    // Pokud je framebuffer aktivni, preskoc VGA kurzor
    if (fb.active) return;

    uint16_t position = (row * 80) + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
}

/* PCI Configuration Space Read */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint32_t tmp = 0;
    
    /* Vytvoření adresy pro CONFIG_ADDRESS */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    /* Zápis adresy do CONFIG_ADDRESS portu */
    outl(0xCF8, address);
    
    /* Čtení z CONFIG_DATA portu */
    tmp = inl(0xCFC);
    
    return tmp;
}

/* Přečtení 16-bit hodnoty z PCI config space */
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    outl(0xCF8, address);
    
    uint32_t tmp = inl(0xCFC);
    
    return (uint16_t)((tmp >> ((offset & 3) * 8)) & 0xFFFF);
}

/* Přečtení 8-bit hodnoty z PCI config space */
uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    outl(0xCF8, address);
    
    uint32_t tmp = inl(0xCFC);
    
    return (uint8_t)((tmp >> ((offset & 3) * 8)) & 0xFF);
}

/* Čtení z RTC registru */
uint8_t rtc_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

/* Konverze BCD na decimal */
uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/* Vlastní k_rand() funkce - LCG generátor */
unsigned long int next = 1;
int k_rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

/* Scancode to ASCII mapping (US QWERTY) */
const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

/* Scancode to ASCII mapping s SHIFT */
const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

/* Stav klávesnice */
int shift_pressed = 0;
int ctrl_pressed = 0;

/* Přečtení znaku z klávesnice */
char getchar() {
    while (1) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            uint8_t scancode = inb(0x60);
            
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
                continue;
            }
            if (scancode == 0xAA || scancode == 0xB6) {
                shift_pressed = 0;
                continue;
            }
            
            if (scancode == 0x1D) {
                ctrl_pressed = 1;
                continue;
            }
            if (scancode == 0x9D) {
                ctrl_pressed = 0;
                continue;
            }
            
            if (scancode & 0x80) {
                continue;
            }
            
            /* Ctrl+C - scancode pro 'C' je 0x2E */
            if (scancode == 0x2E && ctrl_pressed) {
                return -3;  // Speciální kód pro Ctrl+C
            }
            
            /* Šipka nahoru - scancode 0x48 */
            if (scancode == 0x48) {
                return -1;
            }
            
            /* Šipka dolů - scancode 0x50 */
            if (scancode == 0x50) {
                return -2;
            }
            
            if (scancode < sizeof(scancode_to_ascii)) {
                if (shift_pressed) {
                    return scancode_to_ascii_shift[scancode];
                } else {
                    return scancode_to_ascii[scancode];
                }
            }
        }
    }
}

/* Porovnání řetězců */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const uint8_t*)s1 - *(const uint8_t*)s2;
}

/* Délka řetězce */
int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

/* Kopírování řetězce */
void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

/* Převod řetězce na integer */
int atoi(const char* s) {
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

/* Buffer pro vstup */
char input_buffer[256];
int input_index = 0;

/* Aktuální adresář */
char current_dir[64] = "~";

/* Funkce pro sestavení promptu */
void build_prompt(char* prompt_buffer) {
    int i = 0;
    
    // "keke@os "
    const char* user = "keke@os ";
    while (*user) {
        prompt_buffer[i++] = *user++;
    }
    
    // "MSYS "
    const char* msys = "MSYS ";
    while (*msys) {
        prompt_buffer[i++] = *msys++;
    }
    
    // current_dir
    const char* dir = current_dir;
    while (*dir) {
        prompt_buffer[i++] = *dir++;
    }
    
    // "$ "
    prompt_buffer[i++] = '$';
    prompt_buffer[i++] = ' ';
    prompt_buffer[i] = 0;
}

/* Historie příkazů */
#define HISTORY_SIZE 5
char command_history[HISTORY_SIZE][256];
int history_count = 0;
int history_index = -1;

/* Načtení řádku */
void readline(const char* prompt, int hide_input) {
    // Prompt is already printed by caller with colors
    input_index = 0;
    history_index = -1;
    
    while (1) {
        char c = getchar();
        
        /* Ctrl+C - přerušení */
        if (c == -3) {
            putchar('\n', VGA_COLOR_WHITE);
            input_index = 0;
            // Redraw prompt with colors
            print("keke@os ", VGA_COLOR_YELLOW);
            print("MSYS ", VGA_COLOR_MAGENTA);
            print(current_dir, VGA_COLOR_MAGENTA);
            print("$ ", VGA_COLOR_WHITE);
            continue;
        }
        
        /* Šipka nahoru - načtení předchozího příkazu z historie */
        if (c == -1) {
            if (history_count > 0) {
                /* Smaž aktuální řádek */
                while (input_index > 0) {
                    putchar('\b', VGA_COLOR_WHITE);
                    input_index--;
                }
                
                /* Posun indexu v historii */
                if (history_index < history_count - 1) {
                    history_index++;
                } else {
                    history_index = history_count - 1;
                }
                
                /* Zkopíruj příkaz z historie do bufferu */
                strcpy(input_buffer, command_history[history_index]);
                input_index = strlen(input_buffer);
                
                /* Vypiš příkaz */
                for (int i = 0; i < input_index; i++) {
                    if (hide_input) {
                        putchar('*', VGA_COLOR_WHITE);
                    } else {
                        putchar(input_buffer[i], VGA_COLOR_WHITE);
                    }
                }
            }
        }
        /* Šipka dolů - načtení dalšího příkazu z historie */
        else if (c == -2) {
            if (history_index >= 0) {
                /* Smaž aktuální řádek */
                while (input_index > 0) {
                    putchar('\b', VGA_COLOR_WHITE);
                    input_index--;
                }
                
                /* Posun indexu v historii */
                history_index--;
                
                if (history_index >= 0) {
                    /* Zkopíruj příkaz z historie do bufferu */
                    strcpy(input_buffer, command_history[history_index]);
                    input_index = strlen(input_buffer);
                    
                    /* Vypiš příkaz */
                    for (int i = 0; i < input_index; i++) {
                        if (hide_input) {
                            putchar('*', VGA_COLOR_WHITE);
                        } else {
                            putchar(input_buffer[i], VGA_COLOR_WHITE);
                        }
                    }
                } else {
                    /* Prázdný řádek */
                    input_index = 0;
                }
            }
        }
        else if (c == '\n') {
            putchar('\n', VGA_COLOR_WHITE);
            input_buffer[input_index] = 0;
            
            /* Ulož příkaz do historie (pokud není prázdný) */
            if (input_index > 0) {
                /* Posun starší příkazy */
                for (int i = HISTORY_SIZE - 1; i > 0; i--) {
                    strcpy(command_history[i], command_history[i - 1]);
                }
                /* Ulož nový příkaz */
                strcpy(command_history[0], input_buffer);
                
                if (history_count < HISTORY_SIZE) {
                    history_count++;
                }
            }
            
            return;
        } else if (c == '\b') {
            if (input_index > 0) {
                input_index--;
                putchar('\b', VGA_COLOR_WHITE);
            }
        } else if (c >= 32 && c < 127 && input_index < 255) {
            input_buffer[input_index] = c;
            input_index++;
            if (hide_input) {
                putchar('*', VGA_COLOR_WHITE);
            } else {
                putchar(c, VGA_COLOR_WHITE);
            }
        }
    }
}

/* PrintHeader - vypíše hlavičku OS */
void PrintHeader() {
    if (fb.active) {
        fb_puts("Keke OS Terminal [Verze 2.7.5 - Stable Build 2026]\n", 0x0000FFFF);
        fb_puts("Copyright (c) 2026 Keke Corporation. Vsechna prava vyzrazena.\n", 0x0000FFFF);
        fb_puts("Licence: KekeOS Personal Edition - Aktivovano pro ThinkPad X380.\n", 0x0000FFFF);
        fb_puts("Nainstalujte si nejnovejsi verzi na: https://keke-os.com\n", 0x0000FFFF);
        fb_puts("\n", 0x00FFFFFF);
    } else {
        print("Keke OS Terminal [Verze 2.7.5 - Stable Build 2026]\n", VGA_COLOR_CYAN);
        print("Copyright (c) 2026 Keke Corporation. Vsechna prava vyzrazena.\n", VGA_COLOR_CYAN);
        print("Licence: KekeOS Personal Edition - Aktivovano pro ThinkPad X380.\n", VGA_COLOR_CYAN);
        print("Nainstalujte si nejnovejsi verzi na: https://keke-os.com\n", VGA_COLOR_CYAN);
        print("\n", VGA_COLOR_WHITE);
    }
}

/* ReadPassword - čtení hesla s maskováním */
void ReadPassword(char* buffer, int max_len) {
    int index = 0;
    
    while (1) {
        char c = getchar();
        
        if (c == '\n') {
            putchar('\n', VGA_COLOR_WHITE);
            buffer[index] = 0;
            return;
        } else if (c == '\b') {
            if (index > 0) {
                index--;
                putchar('\b', VGA_COLOR_WHITE);
            }
        } else if (c >= 32 && c < 127 && index < max_len - 1) {
            buffer[index] = c;
            index++;
            putchar('*', VGA_COLOR_WHITE);
        }
    }
}

/* Jednoduchá kalkulačka */
void cmd_calc() {
    print("\n=== Keke OS Kalkulacka v1.0 ===\n", VGA_COLOR_YELLOW);
    print("Pro ukonceni napiste 'exit' misto cisla.\n\n", VGA_COLOR_WHITE);
    
    print("Cislo 1: ", VGA_COLOR_WHITE);
    readline("", 0);
    if (strcmp(input_buffer, "exit") == 0) return;
    int a = atoi(input_buffer);
    
    print("Operace (+,-,*,/): ", VGA_COLOR_WHITE);
    readline("", 0);
    char op = input_buffer[0];
    
    print("Cislo 2: ", VGA_COLOR_WHITE);
    readline("", 0);
    int b = atoi(input_buffer);
    
    print("\n", VGA_COLOR_WHITE);
    if (op == '+') {
        print("Vysledek: ", VGA_COLOR_GREEN);
        int result = a + b;
        
        /* Převod int na string */
        char temp[16];
        int i = 0;
        if (result == 0) {
            temp[i++] = '0';
        } else {
            if (result < 0) {
                putchar('-', VGA_COLOR_GREEN);
                result = -result;
            }
            int digits[10];
            int d = 0;
            while (result > 0) {
                digits[d++] = result % 10;
                result /= 10;
            }
            for (int j = d - 1; j >= 0; j--) {
                putchar('0' + digits[j], VGA_COLOR_GREEN);
            }
        }
        putchar('\n', VGA_COLOR_GREEN);
    } else if (op == '-') {
        print("Vysledek: ", VGA_COLOR_GREEN);
        int result = a - b;
        if (result < 0) {
            putchar('-', VGA_COLOR_GREEN);
            result = -result;
        }
        if (result == 0) {
            putchar('0', VGA_COLOR_GREEN);
        } else {
            int digits[10];
            int d = 0;
            while (result > 0) {
                digits[d++] = result % 10;
                result /= 10;
            }
            for (int j = d - 1; j >= 0; j--) {
                putchar('0' + digits[j], VGA_COLOR_GREEN);
            }
        }
        putchar('\n', VGA_COLOR_GREEN);
    } else if (op == '*') {
        print("Vysledek: ", VGA_COLOR_GREEN);
        int result = a * b;
        if (result < 0) {
            putchar('-', VGA_COLOR_GREEN);
            result = -result;
        }
        if (result == 0) {
            putchar('0', VGA_COLOR_GREEN);
        } else {
            int digits[10];
            int d = 0;
            while (result > 0) {
                digits[d++] = result % 10;
                result /= 10;
            }
            for (int j = d - 1; j >= 0; j--) {
                putchar('0' + digits[j], VGA_COLOR_GREEN);
            }
        }
        putchar('\n', VGA_COLOR_GREEN);
    } else if (op == '/') {
        if (b == 0) {
            print("Chyba: Deleni nulou!\n", VGA_COLOR_RED);
        } else {
            print("Vysledek: ", VGA_COLOR_GREEN);
            int result = a / b;
            if (result < 0) {
                putchar('-', VGA_COLOR_GREEN);
                result = -result;
            }
            if (result == 0) {
                putchar('0', VGA_COLOR_GREEN);
            } else {
                int digits[10];
                int d = 0;
                while (result > 0) {
                    digits[d++] = result % 10;
                    result /= 10;
                }
                for (int j = d - 1; j >= 0; j--) {
                    putchar('0' + digits[j], VGA_COLOR_GREEN);
                }
            }
            putchar('\n', VGA_COLOR_GREEN);
        }
    } else {
        print("Neznama operace!\n", VGA_COLOR_RED);
    }
    
    print("\nStisknete Enter pro navrat do Shellu...\n", VGA_COLOR_WHITE);
    readline("", 0);
}

/* AHCI inicializace - přečte BAR5 a vypíše ho */
void ahci_init() {
    print("AHCI Inicializace...\n", VGA_COLOR_WHITE);

    /* Přečtení BAR5 (offset 0x24) z PCI config space pro zařízení 0:31:2 */
    uint32_t ahci_bar5 = pci_config_read(0, 31, 2, 0x24);

    print("AHCI Base Address (BAR5): 0x", VGA_COLOR_LIGHT_GREEN);
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (ahci_bar5 >> i) & 0xF;
        if (nibble < 10) {
            putchar('0' + nibble, VGA_COLOR_LIGHT_GREEN);
        } else {
            putchar('A' + nibble - 10, VGA_COLOR_LIGHT_GREEN);
        }
    }
    putchar('\n', VGA_COLOR_LIGHT_GREEN);

    /* Kontrola zda je BAR5 validní (ne 0xFFFFFFFF pro QEMU) */
    if (ahci_bar5 == 0xFFFFFFFF) {
        print("[Info] AHCI radic neni v tomto prostredi dostupny.\n", VGA_COLOR_WHITE);
        return;
    }

    /* Namapování BAR5 na AHCI strukturu */
    hba_mem_t* abu = (hba_mem_t*)ahci_bar5;

    /* Probuzení řadiče - zapnutí AHCI režimu (bit 31 GHC) */
    abu->ghc |= (1U << 31);

    /* Diagnostický výpis globálních registrů */
    print("AHCI CAP (Vlastnosti): 0x", VGA_COLOR_WHITE);
    uint32_t cap = abu->cap;
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (cap >> i) & 0xF;
        if (nibble < 10) {
            putchar('0' + nibble, VGA_COLOR_WHITE);
        } else {
            putchar('A' + nibble - 10, VGA_COLOR_WHITE);
        }
    }
    putchar('\n', VGA_COLOR_WHITE);

    print("AHCI PI (Implementovane porty): 0x", VGA_COLOR_WHITE);
    uint32_t pi = abu->pi;
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (pi >> i) & 0xF;
        if (nibble < 10) {
            putchar('0' + nibble, VGA_COLOR_WHITE);
        } else {
            putchar('A' + nibble - 10, VGA_COLOR_WHITE);
        }
    }
    putchar('\n', VGA_COLOR_WHITE);

    /* Projdi všech 32 portů podle bitmasky Ports Implemented */
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            /* Port je implementován, zkontroluj jeho stav */
            uint32_t cmd = abu->ports[i].cmd;
            uint32_t ssts = abu->ports[i].ssts;
            uint32_t sig = abu->ports[i].sig;

            /* Výpis CMD registru */
            print("Port ", VGA_COLOR_WHITE);
            int port_num = i;
            if (port_num == 0) putchar('0', VGA_COLOR_WHITE);
            else {
                int digits[2];
                int d = 0;
                while (port_num > 0) {
                    digits[d++] = port_num % 10;
                    port_num /= 10;
                }
                for (int j = d - 1; j >= 0; j--) {
                    putchar('0' + digits[j], VGA_COLOR_WHITE);
                }
            }
            print(" CMD: 0x", VGA_COLOR_WHITE);
            for (int j = 28; j >= 0; j -= 4) {
                uint8_t nibble = (cmd >> j) & 0xF;
                if (nibble < 10) {
                    putchar('0' + nibble, VGA_COLOR_WHITE);
                } else {
                    putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                }
            }
            putchar('\n', VGA_COLOR_WHITE);

            /* Inicializační impuls - nastavení POD (bit 2) a SUD (bit 1) */
            abu->ports[i].cmd |= (1 << 2) | (1 << 1);

            /* Reset port_num pro další výpis */
            port_num = i;

            /* Detailní diagnostika portu */
            print("Port ", VGA_COLOR_WHITE);
            if (port_num == 0) putchar('0', VGA_COLOR_WHITE);
            else {
                int digits[2];
                int d = 0;
                while (port_num > 0) {
                    digits[d++] = port_num % 10;
                    port_num /= 10;
                }
                for (int j = d - 1; j >= 0; j--) {
                    putchar('0' + digits[j], VGA_COLOR_WHITE);
                }
            }
            print(" -> SSTS: 0x", VGA_COLOR_WHITE);
            for (int j = 28; j >= 0; j -= 4) {
                uint8_t nibble = (ssts >> j) & 0xF;
                if (nibble < 10) {
                    putchar('0' + nibble, VGA_COLOR_WHITE);
                } else {
                    putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                }
            }
            print(", SIG: 0x", VGA_COLOR_WHITE);
            for (int j = 28; j >= 0; j -= 4) {
                uint8_t nibble = (sig >> j) & 0xF;
                if (nibble < 10) {
                    putchar('0' + nibble, VGA_COLOR_WHITE);
                } else {
                    putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                }
            }
            putchar('\n', VGA_COLOR_WHITE);

            /* Kontrola paměťové adresy pro Port 0 */
            if (i == 0) {
                print("Port 0 SSTS adresa: 0x", VGA_COLOR_WHITE);
                uint32_t ssts_addr = (uint32_t)&abu->ports[0].ssts;
                for (int j = 28; j >= 0; j -= 4) {
                    uint8_t nibble = (ssts_addr >> j) & 0xF;
                    if (nibble < 10) {
                        putchar('0' + nibble, VGA_COLOR_WHITE);
                    } else {
                        putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                    }
                }
                putchar('\n', VGA_COLOR_WHITE);
            }

            /* Detekce zařízení: (ssts & 0xF) == 3 (device present) */
            /* Aktivní rozhraní: ((ssts >> 8) & 0xF) == 1 */
            if (((ssts & 0xF) == 3) && (((ssts >> 8) & 0xF) == 1)) {
                /* Zjisti typ zařízení podle signatury */
                
                if (sig == 0x00000101) {
                    print("Nalezen SATA disk na portu ", VGA_COLOR_LIGHT_GREEN);
                    
                    /* Výpis čísla portu */
                    port_num = i;
                    if (port_num == 0) putchar('0', VGA_COLOR_LIGHT_GREEN);
                    else {
                        int digits[2];
                        int d = 0;
                        while (port_num > 0) {
                            digits[d++] = port_num % 10;
                            port_num /= 10;
                        }
                        for (int j = d - 1; j >= 0; j--) {
                            putchar('0' + digits[j], VGA_COLOR_LIGHT_GREEN);
                        }
                    }
                    putchar('!', VGA_COLOR_LIGHT_GREEN);
                    putchar('\n', VGA_COLOR_LIGHT_GREEN);
                }
            }
        }
    }
}

/* Static buffers for AHCI command structures */
static hba_cmd_header_t cmd_list[32];
static hba_cmd_table_t cmd_table;
static uint8_t identify_data[512];

/* AHCI IDENTIFY DEVICE command */
int ahci_port_command_identify(int port_num) {
    print("AHCI IDENTIFY DEVICE na portu ", VGA_COLOR_WHITE);
    if (port_num == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        int pn = port_num;
        while (pn > 0) {
            digits[d++] = pn % 10;
            pn /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    putchar('\n', VGA_COLOR_WHITE);

    /* Přečtení BAR5 a inicializace portů */
    uint32_t ahci_bar5 = pci_config_read(0, 31, 2, 0x24);
    hba_mem_t* abu = (hba_mem_t*)ahci_bar5;
    hba_port_t* port = &abu->ports[port_num];

    /* Stop command engine - vypni ST a FRE */
    print("Zastavuji command engine...\n", VGA_COLOR_YELLOW);
    port->cmd &= ~(1 << 0);  // Clear ST (Start)
    port->cmd &= ~(1 << 4);  // Clear FRE (FIS Receive Enable)

    /* Čekej na zastavení - počkej až CR a FR budou 0 */
    int stop_timeout = 10000000;
    while (stop_timeout-- > 0) {
        uint32_t cmd_status = port->cmd;
        if ((cmd_status & ((1 << 15) | (1 << 14))) == 0) {
            break;
        }
    }
    if (stop_timeout <= 0) {
        print("WARNING: Command engine se nepodarilo zastavit!\n", VGA_COLOR_RED);
        return -1;
    }

    /* Nastav Command List Base Address */
    port->clb = (uint32_t)&cmd_list;
    port->clbu = 0;

    /* Nastav FIS Base Address */
    port->fb = (uint32_t)&identify_data;
    port->fbu = 0;

    /* Zapni command engine - FRE a ST */
    print("Spoustim command engine...\n", VGA_COLOR_YELLOW);
    port->cmd |= (1 << 4);  // Set FRE (FIS Receive Enable)
    port->cmd |= (1 << 0);  // Set ST (Start)

    /* Příprava Command Header (slot 0) */
    hba_cmd_header_t* cmd_hdr = &cmd_list[0];
    cmd_hdr->cfl = sizeof(fis_reg_h2d_t) / 4;  // FIS length in DWORDs
    cmd_hdr->a = 0;  // Not ATAPI
    cmd_hdr->w = 0;  // Read
    cmd_hdr->p = 0;  // Not prefetchable
    cmd_hdr->r = 0;  // No reset
    cmd_hdr->b = 0;  // No BIST
    cmd_hdr->c = 0;  // No clear busy
    cmd_hdr->rsv0 = 0;
    cmd_hdr->prdtl = 1;  // 1 PRDT entry
    cmd_hdr->v = 0;
    cmd_hdr->prdbc = 0;
    cmd_hdr->ctba = (uint64_t)(uint32_t)&cmd_table;
    cmd_hdr->ctbau = 0;
    for (int i = 0; i < 4; i++) cmd_hdr->rsv1[i] = 0;

    /* Příprava Command Table */
    /* Vyčisti CFIS */
    for (unsigned int i = 0; i < sizeof(fis_reg_h2d_t); i++) {
        ((uint8_t*)&cmd_table.cfis)[i] = 0;
    }

    /* Nastav FIS pro IDENTIFY DEVICE */
    cmd_table.cfis.fis_type = FIS_TYPE_REG_H2D;
    cmd_table.cfis.pm_port_n = 0;
    cmd_table.cfis.c = 1;  // Update command register
    cmd_table.cfis.command = 0xEC;  // IDENTIFY DEVICE
    cmd_table.cfis.features_l = 0;
    cmd_table.cfis.features_h = 0;
    cmd_table.cfis.lba0 = 0;
    cmd_table.cfis.lba1 = 0;
    cmd_table.cfis.lba2 = 0;
    cmd_table.cfis.device = 0;  // Master
    cmd_table.cfis.lba3 = 0;
    cmd_table.cfis.lba4 = 0;
    cmd_table.cfis.lba5 = 0;
    cmd_table.cfis.count_l = 0;
    cmd_table.cfis.count_h = 0;
    cmd_table.cfis.icc = 0;
    cmd_table.cfis.control = 0;
    for (int i = 0; i < 4; i++) cmd_table.cfis.reserved[i] = 0;

    /* Příprava PRDT */
    cmd_table.prdt.dba = (uint32_t)&identify_data;
    cmd_table.prdt.dbau = 0;
    cmd_table.prdt.reserved = 0;
    cmd_table.prdt.dbc = 511 | (1 << 31);  // 512 bytes, interrupt on completion

    /* Vyčisti ATAPI a reserved */
    for (int i = 0; i < 16; i++) cmd_table.acmd[i] = 0;
    for (int i = 0; i < 32; i++) cmd_table.reserved[i] = 0;

    /* Issue command (set bit 0 in CI) - kritická sekce */
    __asm__ volatile("cli");  // Vypni přerušení
    port->ci = 1;

    /* Počkej na dokončení (BSY bit v TFD se vynuluje) */
    print("Cekam na disk...\n", VGA_COLOR_WHITE);
    int timeout = 100000000;  // Zvýšeno na 100 milionů iterací
    while (timeout-- > 0) {
        uint32_t tfd = port->tfd;
        if ((tfd & 0x80) == 0) {  // BSY bit cleared
            print("IDENTIFY dokoncen!\n", VGA_COLOR_LIGHT_GREEN);

            /* Přečti a vypiš název disku z IDENTIFY dat */
            /* Model string začíná na offsetu 54 (word 27), délka 40 bajtů (20 slov) */
            print("Disk Model: ", VGA_COLOR_LIGHT_CYAN);
            for (int i = 0; i < 20; i++) {
                uint16_t word = ((uint16_t*)identify_data)[27 + i];
                /* ATA používá byte-swapped words - prohoď bajty */
                uint8_t byte1 = word & 0xFF;
                uint8_t byte2 = (word >> 8) & 0xFF;
                if (byte1 != 0 && byte1 != ' ') putchar(byte1, VGA_COLOR_LIGHT_CYAN);
                if (byte2 != 0 && byte2 != ' ') putchar(byte2, VGA_COLOR_LIGHT_CYAN);
            }
            putchar('\n', VGA_COLOR_LIGHT_CYAN);
            __asm__ volatile("sti");  // Zapni přerušení
            return 0;  // Úspěch
        }
    }

    /* Timeout - vypiš TFD pro diagnostiku */
    print("TIMEOUT: Disk neodpovida, TFD = 0x", VGA_COLOR_RED);
    uint32_t tfd_final = port->tfd;
    for (int j = 28; j >= 0; j -= 4) {
        uint8_t nibble = (tfd_final >> j) & 0xF;
        if (nibble < 10) {
            putchar('0' + nibble, VGA_COLOR_RED);
        } else {
            putchar('A' + nibble - 10, VGA_COLOR_RED);
        }
    }
    putchar('\n', VGA_COLOR_RED);

    /* Vypiš detaily TFD */
    if (tfd_final & 0x80) print("  BSY bit set!\n", VGA_COLOR_RED);
    if (tfd_final & 0x40) print("  DRQ bit set!\n", VGA_COLOR_RED);
    if (tfd_final & 0x01) print("  ERR bit set!\n", VGA_COLOR_RED);

    __asm__ volatile("sti");  // Zapni přerušení
    return -1;  // Chybový kód - disk neodpovídá
}

/* Příkaz time - čtení reálného času z RTC */
void cmd_time() {
    print("Aktualni cas: ", VGA_COLOR_WHITE);
    
    /* Čtení hodnot z RTC (BCD formát) */
    uint8_t seconds = bcd_to_dec(rtc_read(0x00));
    uint8_t minutes = bcd_to_dec(rtc_read(0x02));
    uint8_t hours = bcd_to_dec(rtc_read(0x04));
    uint8_t day = bcd_to_dec(rtc_read(0x07));
    uint8_t month = bcd_to_dec(rtc_read(0x08));
    uint8_t year = bcd_to_dec(rtc_read(0x09));
    
    /* Výpis hodin */
    if (hours < 10) putchar('0', VGA_COLOR_WHITE);
    int h = hours;
    if (h == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        while (h > 0) {
            digits[d++] = h % 10;
            h /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    
    putchar(':', VGA_COLOR_WHITE);
    
    /* Výpis minut */
    if (minutes < 10) putchar('0', VGA_COLOR_WHITE);
    int m = minutes;
    if (m == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        while (m > 0) {
            digits[d++] = m % 10;
            m /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    
    putchar(':', VGA_COLOR_WHITE);
    
    /* Výpis sekund */
    if (seconds < 10) putchar('0', VGA_COLOR_WHITE);
    int s = seconds;
    if (s == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        while (s > 0) {
            digits[d++] = s % 10;
            s /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    
    putchar(' ', VGA_COLOR_WHITE);
    
    /* Výpis dne */
    if (day < 10) putchar('0', VGA_COLOR_WHITE);
    int d_day = day;
    if (d_day == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        while (d_day > 0) {
            digits[d++] = d_day % 10;
            d_day /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    
    putchar('.', VGA_COLOR_WHITE);
    
    /* Výpis měsíce */
    if (month < 10) putchar('0', VGA_COLOR_WHITE);
    int mo = month;
    if (mo == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        while (mo > 0) {
            digits[d++] = mo % 10;
            mo /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    
    putchar('.', VGA_COLOR_WHITE);
    
    /* Výpis roku (předpokládáme 20xx) */
    putchar('2', VGA_COLOR_WHITE);
    putchar('0', VGA_COLOR_WHITE);
    if (year < 10) putchar('0', VGA_COLOR_WHITE);
    int y = year;
    if (y == 0) putchar('0', VGA_COLOR_WHITE);
    else {
        int digits[2];
        int d = 0;
        while (y > 0) {
            digits[d++] = y % 10;
            y /= 10;
        }
        for (int j = d - 1; j >= 0; j--) {
            putchar('0' + digits[j], VGA_COLOR_WHITE);
        }
    }
    
    putchar('\n', VGA_COLOR_WHITE);
}

/* Login funkce - vrací 1 při úspěšném přihlášení */
int do_login(void) {
    char Password[] = "1234";
    char password_buffer[256];
    int IsLoggedIn = 0;

    clear_screen();
    PrintHeader();

    while (!IsLoggedIn) {
        print("Zadejte heslo: ", VGA_COLOR_WHITE);
        ReadPassword(password_buffer, 256);

        if (strcmp(password_buffer, Password) == 0) {
            IsLoggedIn = 1;
            print("[OK] Pristup povolen. Vitejte v Keke OS.\n", VGA_COLOR_GREEN);

            for (volatile int i = 0; i < 50000000; i++);

            clear_screen();
            if (fb.active) fb_text_init();
            PrintHeader();
        } else {
            print("[FAIL] Spatne heslo.\n", VGA_COLOR_RED);
        }
    }

    return 1;
}

/* Hlavní funkce kernelu */
void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // Zakázat framebuffer inicializaci na X240
    // fb_init(mbi);
    // if (fb.active) {
    //     fb_text_init();
    //     fb_clear(0x00000000);
    //     fb_puts("KekeOS Boot OK\n", 0x0000FF00);
    // }

    // Použít VGA text mode
    print("KekeOS Boot OK - VGA Mode\n", VGA_COLOR_GREEN);

// Diagnostika - vypíše co GRUB předal
    if (mbi->flags & (1 << 12)) {
        print("FB bit12: OK, type: ", VGA_COLOR_WHITE);
        if (mbi->framebuffer_type == 1) print("RGB\n", VGA_COLOR_GREEN);
        else if (mbi->framebuffer_type == 2) print("TEXT\n", VGA_COLOR_RED);
        else print("UNKNOWN\n", VGA_COLOR_RED);
    } else {
        print("FB bit12: CHYBI\n", VGA_COLOR_RED);
    }

    /* Hlavní smyčka OS - login + shell */
    while (1) {
        if (!do_login()) {
            continue;
        }

        int prompt_drawn = 0;

        /* Hlavní shell smyčka */
        while (1) {
            char prompt_buffer[128];
            build_prompt(prompt_buffer);
            // Print prompt with colors
            print("keke@os ", VGA_COLOR_YELLOW);
            print("MSYS ", VGA_COLOR_MAGENTA);
            print(current_dir, VGA_COLOR_MAGENTA);
            print("$ ", VGA_COLOR_WHITE);
            readline(prompt_buffer, 0);

            if (strcmp(input_buffer, "help") == 0) {
                print("Prikazy: help, cls, ver, calc, time, exit, reboot, gfxreset, disktest, cd, lspci, origin, cat, windows\n", VGA_COLOR_WHITE);
            } else if (strcmp(input_buffer, "ver") == 0) {
                print("--------------------------------------------\n", VGA_COLOR_CYAN);
                print("Keke Operating System [v2.7.5 - Stable Update]\n", VGA_COLOR_CYAN);
                print("Build Date: Sunday, May 17, 2026\n", VGA_COLOR_CYAN);
                print("Target HW: Intel UHD / Lenovo X380 Yoga\n", VGA_COLOR_CYAN);
                print("Kernel: Bare-metal C (no libc)\n", VGA_COLOR_CYAN);
                print("---------------------------------------------\n", VGA_COLOR_CYAN);
            } else if (strcmp(input_buffer, "cls") == 0) {
                clear_screen();
                if (fb.active) fb_text_init();
                PrintHeader();
            } else if (strcmp(input_buffer, "calc") == 0) {
                cmd_calc();
                clear_screen();
                if (fb.active) fb_text_init();
                PrintHeader();
            } else if (strcmp(input_buffer, "time") == 0) {
                cmd_time();
            } else if (strcmp(input_buffer, "exit") == 0) {
                print("Odhlasovani...\n", VGA_COLOR_YELLOW);
                break;  // Výstup z shell smyčky -> zpět na login
            } else if (strcmp(input_buffer, "reboot") == 0) {
                print("Restartovani systemu...\n", VGA_COLOR_YELLOW);
                reboot();
            } else if (strcmp(input_buffer, "gfxreset") == 0) {
                if (fb.active) {
                    fb_clear(0x00000000);
                    fb_puts("keke@os ", 0x00FFFF00);
                    fb_puts("MSYS ", 0x00FF00FF);
                    fb_puts(current_dir, 0x00FF00FF);
                    fb_puts("$ ", 0x00FFFFFF);
                    prompt_drawn = 1;
                    continue;
                } else {
                    print("Framebuffer neni aktivni.\n", VGA_COLOR_RED);
                }
            } else if (strcmp(input_buffer, "cd") == 0 || 
                       (input_buffer[0] == 'c' && input_buffer[1] == 'd' && input_buffer[2] == ' ')) {
                // cd command - only takes first word as argument
                if (input_index == 2 || strcmp(input_buffer, "cd") == 0) {
                    // cd without argument -> go to home
                    strcpy(current_dir, "~");
                } else {
                    // cd <directory> - extract only first word
                    int dir_index = 3;
                    int i = 0;
                    current_dir[0] = '/';
                    i = 1;
                    // Skip leading spaces
                    while (input_buffer[dir_index] == ' ' && dir_index < 255) {
                        dir_index++;
                    }
                    // Copy only first word (until space or end)
                    while (input_buffer[dir_index] != 0 && input_buffer[dir_index] != ' ' && i < 62) {
                        current_dir[i] = input_buffer[dir_index];
                        i++;
                        dir_index++;
                    }
                    current_dir[i] = 0;
                }
            } else if (strcmp(input_buffer, "gui") == 0) {
                print("GUI v nativnim rezimu neni podporovan.\n", VGA_COLOR_RED);
            } else if (strcmp(input_buffer, "disktest") == 0) {
                // Nejprve inicializujeme AHCI a přečteme BAR5
                ahci_init();

                // Po detekci disku spusť IDENTIFY DEVICE
                int identify_result = ahci_port_command_identify(0);
                if (identify_result != 0) {
                    print("Disk test selhal - vracim se do shellu.\n", VGA_COLOR_RED);
                    continue;  // Vrátí se do hlavní smyčky shellu
                }

                print("Cteni MBR sektoru 0...\n", VGA_COLOR_WHITE);
                uint16_t buffer[256];  // 512 bajtů
                // Inicializace bufferu nulami pro detekci selhání čtení
                for (int i = 0; i < 256; i++) {
                    buffer[i] = 0;
                }
                // ata_read_sector(0, buffer);  // DOČASNĚ ZAKOMENTOVÁNO PRO TEST QEMU

                // Kontrola zda buffer obsahuje samé nuly (indikuje žádný disk)
                int all_zeros = 1;
                for (int i = 0; i < 256; i++) {
                    if (buffer[i] != 0) {
                        all_zeros = 0;
                        break;
                    }
                }

                if (all_zeros) {
                    print("[Chyba] Disk neodpovida nebo neni pripojen!\n", VGA_COLOR_RED);
                } else {
                    // Zkontrolujeme MBR signaturu (bajty 510-511)
                    uint16_t signature = buffer[255];  // Poslední slovo (bajty 510-511)
                    if (signature == 0xAA55) {
                        print("[OK] SSD Detekovano! Signatura: 0xAA55\n", VGA_COLOR_LIGHT_GREEN);
                    } else {
                        print("[FAIL] Chybna MBR signatura: 0x", VGA_COLOR_RED);
                        // Výpis hex hodnoty
                        uint16_t sig = signature;
                        for (int i = 12; i >= 0; i -= 4) {
                            uint8_t nibble = (sig >> i) & 0xF;
                            if (nibble < 10) {
                                putchar('0' + nibble, VGA_COLOR_RED);
                            } else {
                                putchar('A' + nibble - 10, VGA_COLOR_RED);
                            }
                        }
                        putchar('\n', VGA_COLOR_RED);
                    }
                }
            } else if (strcmp(input_buffer, "lspci") == 0) {
                print("Prohledavam PCI sbernici...\n", VGA_COLOR_WHITE);
                
                /* DEBUG: Výpis surové hodnoty z 0:0:0 */
                uint32_t raw_reg0 = pci_config_read(0, 0, 0, 0);
                print("DEBUG 0:0:0 RAW REG0: 0x", VGA_COLOR_YELLOW);
                for (int i = 28; i >= 0; i -= 4) {
                    uint8_t nibble = (raw_reg0 >> i) & 0xF;
                    if (nibble < 10) {
                        putchar('0' + nibble, VGA_COLOR_YELLOW);
                    } else {
                        putchar('A' + nibble - 10, VGA_COLOR_YELLOW);
                    }
                }
                putchar('\n', VGA_COLOR_YELLOW);
                
                int device_count = 0;
                
                for (uint16_t bus = 0; bus < 256; bus++) {
                    for (uint8_t slot = 0; slot < 32; slot++) {
                        for (uint8_t func = 0; func < 8; func++) {
                            // Přečti Vendor ID a Device ID (offset 0x00)
                            uint32_t vendor_device = pci_config_read(bus, slot, func, 0x00);
                            uint16_t vendor_id = vendor_device & 0xFFFF;
                            
                            // Pokud Vendor ID == 0xFFFF, zařízení neexistuje
                            if (vendor_id == 0xFFFF) continue;
                            
                            device_count++;
                            uint16_t device_id = (vendor_device >> 16) & 0xFFFF;
                            
                            // Přečti Class Code (offset 0x08 - bajt 2 = class, bajt 1 = subclass)
                            uint32_t class_reg = pci_config_read(bus, slot, func, 0x08);
                            uint8_t class_code = (class_reg >> 24) & 0xFF;
                            uint8_t subclass = (class_reg >> 16) & 0xFF;
                            
                            // Zkontroluj zda je to SATA AHCI Controller
                            if (class_code == 0x01 && subclass == 0x06) {
                                // Zvýrazněný výpis pro AHCI řadič
                                print("[NALEZEN AHCI RADIC] ", VGA_COLOR_YELLOW);
                                
                                // Výpis Bus
                                print("Bus:", VGA_COLOR_YELLOW);
                                int bus_num = bus;
                                if (bus_num == 0) putchar('0', VGA_COLOR_YELLOW);
                                else {
                                    int digits[3];
                                    int d = 0;
                                    while (bus_num > 0) {
                                        digits[d++] = bus_num % 10;
                                        bus_num /= 10;
                                    }
                                    for (int j = d - 1; j >= 0; j--) {
                                        putchar('0' + digits[j], VGA_COLOR_YELLOW);
                                    }
                                }
                                
                                print(", Slot:", VGA_COLOR_YELLOW);
                                int slot_num = slot;
                                if (slot_num == 0) putchar('0', VGA_COLOR_YELLOW);
                                else {
                                    int digits[2];
                                    int d = 0;
                                    while (slot_num > 0) {
                                        digits[d++] = slot_num % 10;
                                        slot_num /= 10;
                                    }
                                    for (int j = d - 1; j >= 0; j--) {
                                        putchar('0' + digits[j], VGA_COLOR_YELLOW);
                                    }
                                }
                                
                                print(", Func:", VGA_COLOR_YELLOW);
                                putchar('0' + func, VGA_COLOR_YELLOW);
                                putchar('\n', VGA_COLOR_YELLOW);
                            } else {
                                // Běžný výpis zařízení
                                print("  ", VGA_COLOR_WHITE);
                                
                                // Výpis Bus
                                int bus_num = bus;
                                if (bus_num == 0) putchar('0', VGA_COLOR_WHITE);
                                else {
                                    int digits[3];
                                    int d = 0;
                                    while (bus_num > 0) {
                                        digits[d++] = bus_num % 10;
                                        bus_num /= 10;
                                    }
                                    for (int j = d - 1; j >= 0; j--) {
                                        putchar('0' + digits[j], VGA_COLOR_WHITE);
                                    }
                                }
                                print(":", VGA_COLOR_WHITE);
                                
                                // Výpis Slot
                                int slot_num = slot;
                                if (slot_num == 0) putchar('0', VGA_COLOR_WHITE);
                                else {
                                    int digits[2];
                                    int d = 0;
                                    while (slot_num > 0) {
                                        digits[d++] = slot_num % 10;
                                        slot_num /= 10;
                                    }
                                    for (int j = d - 1; j >= 0; j--) {
                                        putchar('0' + digits[j], VGA_COLOR_WHITE);
                                    }
                                }
                                print(":", VGA_COLOR_WHITE);
                                putchar('0' + func, VGA_COLOR_WHITE);
                                
                                print(" Vendor:", VGA_COLOR_WHITE);
                                // Hex výpis Vendor ID
                                for (int i = 12; i >= 0; i -= 4) {
                                    uint8_t nibble = (vendor_id >> i) & 0xF;
                                    if (nibble < 10) {
                                        putchar('0' + nibble, VGA_COLOR_WHITE);
                                    } else {
                                        putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                                    }
                                }
                                
                                print(" Device:", VGA_COLOR_WHITE);
                                // Hex výpis Device ID
                                for (int i = 12; i >= 0; i -= 4) {
                                    uint8_t nibble = (device_id >> i) & 0xF;
                                    if (nibble < 10) {
                                        putchar('0' + nibble, VGA_COLOR_WHITE);
                                    } else {
                                        putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                                    }
                                }
                                
                                print(" Class:", VGA_COLOR_WHITE);
                                // Hex výpis Class
                                for (int i = 4; i >= 0; i -= 4) {
                                    uint8_t nibble = (class_code >> i) & 0xF;
                                    if (nibble < 10) {
                                        putchar('0' + nibble, VGA_COLOR_WHITE);
                                    } else {
                                        putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                                    }
                                }
                                
                                print(" Subclass:", VGA_COLOR_WHITE);
                                // Hex výpis Subclass
                                for (int i = 4; i >= 0; i -= 4) {
                                    uint8_t nibble = (subclass >> i) & 0xF;
                                    if (nibble < 10) {
                                        putchar('0' + nibble, VGA_COLOR_WHITE);
                                    } else {
                                        putchar('A' + nibble - 10, VGA_COLOR_WHITE);
                                    }
                                }
                                
                                putchar('\n', VGA_COLOR_WHITE);
                            }
                        }
                    }
                }
                
                print("Nalezeno celkem ", VGA_COLOR_WHITE);
                int count = device_count;
                if (count == 0) putchar('0', VGA_COLOR_WHITE);
                else {
                    int digits[3];
                    int d = 0;
                    while (count > 0) {
                        digits[d++] = count % 10;
                        count /= 10;
                    }
                    for (int j = d - 1; j >= 0; j--) {
                        putchar('0' + digits[j], VGA_COLOR_WHITE);
                    }
                }
                print(" PCI zarizeni.\n", VGA_COLOR_WHITE);
            } else if (strcmp(input_buffer, "origin") == 0) {
                print("Keke OS nevznikl u stolu psanim kodu. Vznikl v kvetnu 2026, kdy Keke zahral kombo Marshall na maximalni hlasitost a sekl do strun sve kytary. Elektromagneticky impulz a surova vibrace zvuku byly tak silne, ze v okolnim vesmiru doslo k mikro-ohnuti casoprostoru. V tu samou nanosekundu proletelo nedalekym ThinkPadem kosmicke zareni a prepisalo v pameti jedno kriticke 16-bitove cislo na 32-bitove. Notebook, do te doby polomrtvy, poprve zablikal a na displeji se objevil napis Keke OS. Vedci tomu dodnes rikaji 'Rockovy zrod' a kod kvuli teto vesmirne anomalii odmitame kompilovat v tichu.\n", VGA_COLOR_LIGHT_MAGENTA);
            } else if (strcmp(input_buffer, "cat") == 0) {
                print("Mnozi se ptaji, proc PCI scanner v Keke OS funguje tak stabilne. Pravda je takova, ze hlavni architekt systemu nesedi u klavesnice, ale sleduje kod z nejvyssiho patra kociciho stromu v rohu mistnosti. Kdyz kod v noci nesel zkompilovat a Rax uz propadal zoufalstvi, prosla se po klavesnici tlapka. Smazala tri stredniky a zmenila uint16_t na uint32_t. Byl to akt ciste ritualni magie. Tento system nebyl naprogramovan, byl schvalen vyssi kocici inteligenci. Pokud ti kod pada, zapomnel jsi nasypat granule.\n", VGA_COLOR_LIGHT_MAGENTA);
            } else if (strcmp(input_buffer, "windows") == 0) {
                print("Kdyz Bill Gates v roce 1985 zakladal Windows, sedel v kancelari a dival se skrz realne sklenene okno na ulici. Videl lidi, jak spechaji do prace, a uvedomil si, ze lidstvo potrebuje system, ktery je bude neustale zdrzovat aktualizacemi, aby se v tom shonu na chvili zastavili. Byla to hloboka myslenka. Keke OS tento paradox odmita. My se nedivame z okna, my se divame primo do kremiku. Keke OS nema okna, protoze okny utika teplo a rock'n'roll.\n", VGA_COLOR_LIGHT_MAGENTA);
            } else if (input_index > 0) {
                print("-kekeShell: ", VGA_COLOR_RED);
                print(input_buffer, VGA_COLOR_RED);
                print(": command not found\n", VGA_COLOR_RED);
            }
        }
    }
}