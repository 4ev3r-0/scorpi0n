#include <stdint.h>

#define GPIO_OUT_W1TS    0x60004008
#define GPIO_OUT_W1TC    0x6000400C
#define GPIO_ENABLE_W1TS 0x60004024

#define GPIO_FUNC_BASE   0x60004554
#define IO_MUX_BASE     0x60009000

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define SCK 12
#define MOSI 11
#define DC 9
#define RST 10
#define CS 8

static void gpio_set(int pin)
{
    REG32(GPIO_OUT_W1TS) = 1u << pin;
}

static void gpio_clear(int pin)
{
    REG32(GPIO_OUT_W1TC) = 1u << pin;
}

static void gpio_output(int pin)
{
    REG32(GPIO_ENABLE_W1TS) = 1u << pin;
}

static void delay(volatile uint32_t count)
{
    while (count--)
        __asm__ volatile ("nop");
}

static void spi_write_bit(int bit)
{
    if (bit)
        gpio_set(MOSI);
    else
        gpio_clear(MOSI);

    gpio_set(SCK);
    delay(20);
    gpio_clear(SCK);
    delay(20);
}

static void spi_write(uint8_t value)
{
    for (int i = 7; i >= 0; i--)
        spi_write_bit((value >> i) & 1);
}

static void command(uint8_t value)
{
    gpio_clear(DC);
    gpio_clear(CS);

    spi_write(value);

    gpio_set(CS);
}

static void data(uint8_t value)
{
    gpio_set(DC);
    gpio_clear(CS);

    spi_write(value);

    gpio_set(CS);
}

static void data16(uint16_t value)
{
    data(value >> 8);
    data(value & 0xff);
}

static void st7789_init(void)
{
    gpio_clear(RST);
    delay(1000000);

    gpio_set(RST);
    delay(2000000);

    command(0x01);
    delay(3000000);

    command(0x11);
    delay(3000000);

    command(0x36);
    data(0x60);

    command(0x3A);
    data(0x55);

    command(0x21);

    command(0x13);

    command(0x29);
    delay(1000000);
}

static void set_window(void)
{
    command(0x2A);

    data(0x00);
    data(0x00);
    data(0x01);
    data(0x3F);

    command(0x2B);

    data(0x00);
    data(0x00);
    data(0x00);
    data(0xEF);

    command(0x2C);
}

static void fill(uint16_t color)
{
    set_window();

    gpio_set(DC);
    gpio_clear(CS);

    for (int i = 0; i < 320 * 240; i++)
        data16(color);

    gpio_set(CS);
}

static void configure_gpio(int pin)
{
    gpio_output(pin);

    REG32(GPIO_FUNC_BASE + pin * 4) = 256;

    REG32(IO_MUX_BASE + pin * 4) = 0;
}

void main(void)
{
    configure_gpio(SCK);
    configure_gpio(MOSI);
    configure_gpio(DC);
    configure_gpio(RST);
    configure_gpio(CS);

    gpio_clear(SCK);
    gpio_clear(MOSI);
    gpio_set(CS);

    st7789_init();

    fill(0xF800);

    while (1)
        delay(1000000);
}
