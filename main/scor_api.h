#ifndef SCOR_API_H
#define SCOR_API_H

#include <stdint.h>

typedef struct {
    void (*fill_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_image)(const char *path, int x, int y, int w, int h);
    void (*draw_text)(const char *text, int x, int y, uint32_t color);
    void (*clear)(uint32_t color);
    const uint8_t *(*asset_get)(const char *name, uint32_t *out_size);
    int (*asset_list)(char *out, int max_entries, int entry_size);
    void (*delay_ms)(int ms);
    void (*log)(const char *fmt, ...);
    uint32_t (*get_tick_ms)(void);
} scor_api_t;

// Lives at a fixed PSRAM address (see scor.ld)
// The firmware loader fills this in before calling scor_main()
extern scor_api_t scor_api;

// Entry point - every .scor must define this
void scor_main(void);

#endif   