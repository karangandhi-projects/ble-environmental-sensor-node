/**
 * @file ssd1306.c
 * @brief SSD1306 OLED driver — I2C init, framebuffer, and flush.
 *
 * Maintains a static 5×128-byte framebuffer (640 bytes in RAM). Pixel writes
 * are local operations; only ssd1306_flush() sends data over I2C. The
 * 128-wide framebuffer covers the full controller address space; the visible
 * 72-column window starts at column 28 (SSD1306_X_OFFSET) applied during flush.
 *
 * @note The I2C driver is initialised on I2C_NUM_0 by ssd1306_init(). No
 *       other component should access I2C_NUM_0 directly after this point.
 */
#include "ssd1306.h"
#include "driver/i2c.h"
#include "app_config.h"

/* Framebuffer: 5 pages x 128 columns (full controller address space).
   Display pixels use columns 0..71 in the framebuffer (x-offset applied only during flush). */
static uint8_t s_fb[SSD1306_PAGES][128];

/* -------------------------------------------------------------------------
 * ssd1306_init
 * ------------------------------------------------------------------------- */
esp_err_t ssd1306_init(void)
{
    /* Configure I2C master on I2C_NUM_0. */
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = BLE_ENV_I2C_SDA_GPIO,
        .scl_io_num       = BLE_ENV_I2C_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BLE_ENV_I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    /* SSD1306 initialization sequence sent in a single I2C transaction.
     * First byte 0x00 = control byte: Co=0, D/C#=0 → all following bytes are commands. */
    static const uint8_t init_seq[] = {
        0x00,       /* control byte: command stream */
        0xAE,       /* display off */
        0xD5, 0x80, /* set display clock divide ratio / oscillator frequency */
        0xA8, 0x27, /* set multiplex ratio = 40 (0x27 = 39) */
        0xD3, 0x00, /* set display offset = 0 */
        0x40,       /* set display start line = 0 */
        0x8D, 0x14, /* enable charge pump */
        0x20, 0x02, /* set memory addressing mode: page addressing */
        0xA1,       /* set segment re-map (col 127 -> SEG0) */
        0xC8,       /* set COM output scan direction: remapped */
        0xDA, 0x12, /* set COM pins hardware configuration: sequential */
        0x81, 0xCF, /* set contrast */
        0xD9, 0xF1, /* set pre-charge period */
        0xDB, 0x40, /* set VCOMH deselect level */
        0xA4,       /* entire display ON: output follows RAM content */
        0xA6,       /* set normal display (not inverted) */
        0xAF,       /* display on */
    };

    ret = i2c_master_write_to_device(I2C_NUM_0,
                                     BLE_ENV_OLED_I2C_ADDR,
                                     init_seq,
                                     sizeof(init_seq),
                                     pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        return ret;
    }

    /* Blank the screen. */
    ssd1306_clear();
    return ssd1306_flush();
}

/* -------------------------------------------------------------------------
 * ssd1306_clear
 * ------------------------------------------------------------------------- */
void ssd1306_clear(void)
{
    for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
        for (uint8_t col = 0; col < 128; col++) {
            s_fb[page][col] = 0x00;
        }
    }
}

/* -------------------------------------------------------------------------
 * ssd1306_set_pixel
 * x: 0-based display coordinate (0 = leftmost visible column, maps to controller col 28).
 * y: 0-based display coordinate (0 = topmost row).
 * ------------------------------------------------------------------------- */
void ssd1306_set_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }

    uint8_t page = y / 8;
    uint8_t bit  = y % 8;

    if (on) {
        s_fb[page][x] |=  (1u << bit);
    } else {
        s_fb[page][x] &= ~(1u << bit);
    }
}

/* -------------------------------------------------------------------------
 * ssd1306_flush
 * Sends all 5 pages to the display. Each page covers 72 visible columns
 * starting at controller column 28 (SSD1306_X_OFFSET).
 * ------------------------------------------------------------------------- */
esp_err_t ssd1306_flush(void)
{
    for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
        /* Set page address and column start (col 28 = 0x1C).
         * Lower nibble command: 0x0C  (lower nibble of 28 = 0xC -> 0x00|0x0C)
         * Upper nibble command: 0x11  (upper nibble 0x10 | (28>>4)=0x01) */
        uint8_t cmd_buf[4] = {
            0x00,           /* control byte: command stream */
            (uint8_t)(0xB0 | page), /* set page start address */
            0x0C,           /* set lower column start address (col 28 lower nibble = 0xC) */
            0x11,           /* set upper column start address (col 28 upper nibble = 1, 0x10|0x01) */
        };

        esp_err_t ret = i2c_master_write_to_device(I2C_NUM_0,
                                                   BLE_ENV_OLED_I2C_ADDR,
                                                   cmd_buf,
                                                   sizeof(cmd_buf),
                                                   pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            return ret;
        }

        /* Send data: control byte 0x40 followed by 72 pixel bytes for this page.
         * Framebuffer columns 0..71 map directly to display columns 28..99. */
        uint8_t data_buf[73];
        data_buf[0] = 0x40; /* control byte: data stream */
        for (uint8_t col = 0; col < SSD1306_WIDTH; col++) {
            data_buf[1 + col] = s_fb[page][col];
        }

        ret = i2c_master_write_to_device(I2C_NUM_0,
                                         BLE_ENV_OLED_I2C_ADDR,
                                         data_buf,
                                         sizeof(data_buf),
                                         pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * ssd1306_draw_char
 * Renders a single ASCII character from a column-major font.
 * font_data layout: (c - 0x20) * font_w bytes per glyph, column-major,
 * bit 0 = topmost pixel of the column.
 * ------------------------------------------------------------------------- */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c,
                       const uint8_t *font_data,
                       uint8_t font_w, uint8_t font_h, uint8_t scale)
{
    if (c < 0x20 || c > 0x7E) {
        c = ' ';
    }

    const uint8_t *glyph = font_data + (c - 0x20) * font_w;

    for (uint8_t col = 0; col < font_w; col++) {
        for (uint8_t row = 0; row < font_h; row++) {
            if ((glyph[col] >> row) & 1) {
                for (uint8_t sy = 0; sy < scale; sy++) {
                    for (uint8_t sx = 0; sx < scale; sx++) {
                        ssd1306_set_pixel(x + col * scale + sx,
                                          y + row * scale + sy,
                                          true);
                    }
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * ssd1306_draw_string
 * Renders a null-terminated ASCII string.
 * Returns the x position immediately after the last character drawn.
 * ------------------------------------------------------------------------- */
uint8_t ssd1306_draw_string(uint8_t x, uint8_t y, const char *str,
                             const uint8_t *font_data,
                             uint8_t font_w, uint8_t font_h, uint8_t scale)
{
    while (*str) {
        ssd1306_draw_char(x, y, *str, font_data, font_w, font_h, scale);
        x += font_w * scale;
        str++;
    }

    return x;
}
