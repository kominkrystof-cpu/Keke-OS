#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint32_t* addr;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint8_t   bpp;
    uint8_t   active;
} framebuffer_t;

void fb_init(void* mbi);
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_test(void);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch(void);
void*    fb_get_addr(void);

#endif
