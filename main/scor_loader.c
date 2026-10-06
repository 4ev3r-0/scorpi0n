#include "scor_loader.h"
#include "scor_api.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include <stdio.h>
#include <stdarg.h>

static const char *TAG = "scor";

#define SCOR_MAGIC      0x524F4353  // "SCOR"
#define SCOR_VERSION    2
#define SCOR_BASE       0x3C100000
#define SCOR_MAX        (4 * 1024 * 1024)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t num_files;
    uint32_t table_off;
    uint32_t data_off;
    uint32_t names_off;
    uint32_t total_size;
    uint32_t checksum;
} __attribute__((packed)) scor_container_t;

typedef struct {
    uint32_t name_off;
    uint16_t name_len;
    uint16_t _pad;
    uint32_t data_off;
    uint32_t data_size;
} __attribute__((packed)) scor_file_entry_t;

// ---- State ----
static uint8_t *s_file_buf = NULL;
static uint32_t s_file_size = 0;
static scor_file_entry_t *s_entries = NULL;
static const char *s_names = NULL;
static uint32_t s_num_files = 0;

// ---- API implementations ----

void api_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    ESP_LOGI(TAG, "fill_rect(%d,%d,%d,%d,0x%06x)", x, y, w, h, color);
}

void api_draw_image(const char *path, int x, int y, int w, int h)
{
    uint32_t size;
    const uint8_t *data = api_asset_get(path, &size);
    if (data) {
        ESP_LOGI(TAG, "draw_image(%s, %ux%u, %d,%d)", path, size, w, h, x, y);
    } else {
        ESP_LOGE(TAG, "Asset not found: %s", path);
    }
}

void api_draw_text(const char *text, int x, int y, uint32_t color)
{
    ESP_LOGI(TAG, "draw_text(%s, %d,%d)", text, x, y);
}

void api_clear(uint32_t color)
{
    ESP_LOGI(TAG, "clear(0x%06x)", color);
}

const uint8_t *api_asset_get(const char *name, uint32_t *out_size)
{
    if (!s_file_buf) return NULL;

    for (uint32_t i = 0; i < s_num_files; i++) {
        const char *fname = s_names + s_entries[i].name_off;
        if (strncmp(fname, name, s_entries[i].name_len) == 0 &&
            name[s_entries[i].name_len] == '\0') {
            *out_size = s_entries[i].data_size;
            return s_file_buf + s_entries[i].data_off;
        }
    }
    return NULL;
}

int api_asset_list(char *out, int max_entries, int entry_size)
{
    if (!s_file_buf) return 0;
    int count = 0;
    for (uint32_t i = 0; i < s_num_files && count < max_entries; i++) {
        const char *fname = s_names + s_entries[i].name_off;
        int len = (int)s_entries[i].name_len;
        if (len >= entry_size) len = entry_size - 1;
        memcpy(out + count * entry_size, fname, len);
        out[count * entry_size + len] = '\0';
        count++;
    }
    return count;
}

void api_delay_ms(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void api_log(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ESP_LOGI(TAG, "[scor] %s", buf);
}

uint32_t api_get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// ---- Loader ----

esp_err_t scor_load_and_run(const char *path)
{
    scor_unload();

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > SCOR_MAX) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    s_file_buf = malloc(fsize);
    if (!s_file_buf) { fclose(f); return ESP_ERR_NO_MEM; }
    if (fread(s_file_buf, 1, fsize, f) != (size_t)fsize) {
        free(s_file_buf); s_file_buf = NULL;
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    fclose(f);
    s_file_size = (uint32_t)fsize;

    // Parse container header
    scor_container_t *hdr = (scor_container_t *)s_file_buf;
    if (hdr->magic != SCOR_MAGIC) {
        ESP_LOGE(TAG, "Bad magic: 0x%08x", hdr->magic);
        scor_unload();
        return ESP_ERR_INVALID_ARG;
    }
    if (hdr->version != SCOR_VERSION) {
        ESP_LOGE(TAG, "Unsupported version: %u", hdr->version);
        scor_unload();
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_num_files = hdr->num_files;
    s_entries = (scor_file_entry_t *)(s_file_buf + hdr->table_off);
    s_names = (const char *)(s_file_buf + hdr->names_off);

    ESP_LOGI(TAG, "Container: %u files, %u bytes total", s_num_files, s_file_size);

    // Find the "code" entry
    const uint8_t *code_data = NULL;
    uint32_t code_size = 0;
    for (uint32_t i = 0; i < s_num_files; i++) {
        const char *fname = s_names + s_entries[i].name_off;
        if (strncmp(fname, "code", 4) == 0 && fname[4] == '\0') {
            code_data = s_file_buf + s_entries[i].data_off;
            code_size = s_entries[i].data_size;
            break;
        }
    }

    if (!code_data) {
        ESP_LOGE(TAG, "No 'code' entry found");
        scor_unload();
        return ESP_ERR_NOT_FOUND;
    }

    // Code blob layout:
    //   [entry_off:u32][api_off:u32][text][rodata][data][bss_size:u32]
    uint32_t entry_off = *(uint32_t *)(code_data);
    uint32_t api_off   = *(uint32_t *)(code_data + 4);

    // Actual binary starts after the 8-byte header
    const uint8_t *binary = code_data + 8;
    uint32_t binary_size = code_size - 8;

    // bss_size is the last 4 bytes of the blob
    uint32_t bss_size = *(uint32_t *)(code_data + code_size - 4);
    uint32_t load_size = binary_size - 4;  // exclude the bss_size trailer

    // Copy to PSRAM
    memcpy((void *)SCOR_BASE, binary, load_size);

    // Zero BSS
    uint8_t *bss_start = (uint8_t *)SCOR_BASE + load_size;
    memset(bss_start, 0, bss_size);

    // Fill in the vtable
    scor_api_t *api = (scor_api_t *)(SCOR_BASE + api_off);
    api->fill_rect   = api_fill_rect;
    api->draw_image  = api_draw_image;
    api->draw_text   = api_draw_text;
    api->clear       = api_clear;
    api->asset_get   = api_asset_get;
    api->asset_list  = api_asset_list;
    api->delay_ms    = api_delay_ms;
    api->log         = api_log;
    api->get_tick_ms = api_get_tick_ms;

    ESP_LOGI(TAG, "Executing (entry=0x%06x, api=0x%06x, load=%u, bss=%u)",
             entry_off, api_off, load_size, bss_size);

    // Jump
    typedef void (*scor_main_fn)(void);
    scor_main_fn entry = (scor_main_fn)(SCOR_BASE + entry_off);
    entry();

    ESP_LOGI(TAG, "Returned from .scor");
    return ESP_OK;
}

void scor_unload(void)
{
    if (s_file_buf) {
        free(s_file_buf);
        s_file_buf = NULL;
    }
    s_file_size = 0;
    s_entries = NULL;
    s_names = NULL;
    s_num_files = 0;
}   