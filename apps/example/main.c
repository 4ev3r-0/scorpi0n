#include "scor_api.h"

void scor_main(void)
{
    scor_api.clear(0x1e1e2e);
    scor_api.draw_text("Hello from .scor!", 50, 100, 0xffffff);
    scor_api.fill_rect(10, 10, 300, 5, 0x89b4fa);

    uint32_t size;
    const uint8_t *img = scor_api.asset_get("logo.png", &size);
    if (img) {
        scor_api.draw_image("logo.png", 50, 20, 220, 100);
    }

    scor_api.log("done");
}   