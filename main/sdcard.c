#include "sdcard.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static const char *TAG = "sdcard";

#define SD_MOUNT_POINT "/sdcard"
#define SD_PIN_CLK    GPIO_NUM_14
#define SD_PIN_MOSI   GPIO_NUM_15
#define SD_PIN_MISO   GPIO_NUM_2
#define SD_PIN_CS     GPIO_NUM_4

esp_err_t sdcard_init(void)
{
    // Use SPI3 (SPI2 is taken by the LCD)
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = SD_PIN_CLK,
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted. Size: %u MB", card->csd.capacity / (1024 * 1024));
    return ESP_OK;
}   