#include <stdio.h>
#include <assert.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// LVGL
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "sdcard.h"
#include "scor_loader.h"

static const char *TAG = "main";

// Wiring
#define LCD_MOSI_PIN    GPIO_NUM_11
#define LCD_SCLK_PIN    GPIO_NUM_12
#define LCD_CS_PIN      GPIO_NUM_10
#define LCD_DC_PIN      GPIO_NUM_9
#define LCD_RST_PIN     GPIO_NUM_8
#define LCD_BL_PIN      GPIO_NUM_45

// Display specs
#define LCD_H_RES       320
#define LCD_V_RES       240
#define LCD_PIXEL_HZ    (10 * 1000 * 1000)

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io_handle;
static lv_display_t *s_disp;

static esp_err_t lcd_init(void)
{
    gpio_config_t bk_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_BL_PIN
    };
    ESP_ERROR_CHECK(gpio_config(&bk_cfg));
    gpio_set_level(LCD_BL_PIN, 0);

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = LCD_SCLK_PIN,
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 20 * 2 + 8
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_DC_PIN,
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = LCD_PIXEL_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_io_handle));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io_handle, &panel_cfg, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));

    gpio_set_level(LCD_BL_PIN, 1);
    return ESP_OK;
}

static esp_err_t lvgl_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_io_handle,
        .panel_handle = s_panel,
        .buffer_size = LCD_H_RES * 20,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        },
    };

    s_disp = lvgl_port_add_disp(&disp_cfg);
    assert(s_disp != NULL);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1e1e2e), LV_PART_MAIN);

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting...");

    lcd_init();
    lvgl_init();

    if (sdcard_init() == ESP_OK) {
        ESP_LOGI(TAG, "SD ready. Loading /apps/demo.scor");

        lv_obj_t *lbl = lv_label_create(lv_scr_act());
        lv_label_set_text(lbl, "Loading...");
        lv_obj_center(lbl);

        esp_err_t ret = scor_load_and_run("/sdcard/apps/demo.scor");
        if (ret != ESP_OK) {
            lv_label_set_text_fmt(lbl, "Failed: %s", esp_err_to_name(ret));
        } else {
            lv_obj_del(lbl);
        }
    } else {
        lv_obj_t *err = lv_label_create(lv_scr_act());
        lv_label_set_text(err, "SD card not found");
        lv_obj_center(err);
    }
}   