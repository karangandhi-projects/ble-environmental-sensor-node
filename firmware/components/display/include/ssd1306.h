/**
 * @file ssd1306.h
 * @brief SSD1306 OLED driver — framebuffer + I2C flush.
 *
 * Drives a 0.42" SSD1306-based OLED panel via I2C (I2C_NUM_0). The physical
 * controller has a 128×64 pixel buffer but only a 72×40 window is visible
 * through the module's bezel, starting at column 28 (SSD1306_X_OFFSET).
 *
 * @par Coordinate system
 * All public functions use display coordinates: x ∈ [0, 71], y ∈ [0, 39].
 * The X_OFFSET translation to controller coordinates is applied internally
 * by ssd1306_flush().
 *
 * @par Framebuffer
 * A single static 5-page × 72-column framebuffer is maintained in RAM
 * (~360 bytes). Pixel writes are local; ssd1306_flush() transfers all 5
 * pages to the controller over I2C in one burst (~5 ms at 400 kHz).
 *
 * @par I2C
 * Initialised with SDA=GPIO5, SCL=GPIO6, 400 kHz. I2C_NUM_0 is used
 * exclusively by this driver; no other component should call i2c_master_*
 * on the same bus.
 */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief Visible display width in pixels (hardware window, not controller width). */
#define SSD1306_WIDTH    72
/** @brief Visible display height in pixels. */
#define SSD1306_HEIGHT   40
/** @brief Number of 8-pixel-tall pages (ceil(40/8) = 5). */
#define SSD1306_PAGES    5
/** @brief Column offset from controller origin to visible window start. */
#define SSD1306_X_OFFSET 28

/**
 * @brief Initialise I2C bus and send the SSD1306 configuration sequence.
 *
 * Sends the standard init sequence: display off, set clock, multiplex ratio,
 * display offset, start line, charge pump, addressing mode, segment remap,
 * COM output scan direction, COM pins hardware config, contrast, pre-charge,
 * V_COMH deselect, display on.
 *
 * @return ESP_OK on success; I2C error code if the device does not ACK.
 */
esp_err_t ssd1306_init(void);

/**
 * @brief Clear the in-RAM framebuffer to all-off pixels.
 *
 * Does not flush to the display — call ssd1306_flush() to make changes
 * visible. Typically called at the start of each render cycle.
 */
void ssd1306_clear(void);

/**
 * @brief Set or clear a single pixel in the framebuffer.
 *
 * Out-of-range coordinates (x ≥ 72 or y ≥ 40) are silently ignored,
 * so callers do not need to clip manually.
 *
 * @param x  Horizontal position in display coordinates [0, 71].
 * @param y  Vertical position in display coordinates [0, 39].
 * @param on true to turn the pixel on; false to turn it off.
 */
void ssd1306_set_pixel(uint8_t x, uint8_t y, bool on);

/**
 * @brief Draw one ASCII character using a column-major bitmap font.
 *
 * The font is stored column-major: for each glyph, font_w consecutive bytes
 * represent columns left→right; within each byte, bit 0 is the topmost pixel.
 * The character index into the font array is (c - 0x20) × font_w.
 *
 * @param x         Left edge of the character in display coordinates.
 * @param y         Top edge of the character.
 * @param c         ASCII character to draw (must be in 0x20–0x7E range).
 * @param font_data Pointer to the start of the font bitmap array.
 * @param font_w    Width of one glyph in columns (bytes per character).
 * @param font_h    Height of one glyph in rows (bits per column used).
 * @param scale     Pixel upscaling factor: 1 = native, 2 = 2× size.
 */
void ssd1306_draw_char(uint8_t x, uint8_t y, char c,
                       const uint8_t *font_data,
                       uint8_t font_w, uint8_t font_h, uint8_t scale);

/**
 * @brief Draw a null-terminated ASCII string.
 *
 * Calls ssd1306_draw_char() for each character and advances x by
 * font_w × scale pixels between characters.
 *
 * @param x         Left edge of the first character.
 * @param y         Top edge of the string.
 * @param str       Null-terminated ASCII string.
 * @param font_data Font bitmap array pointer.
 * @param font_w    Glyph width in columns.
 * @param font_h    Glyph height in rows.
 * @param scale     Pixel upscaling factor.
 * @return          x position immediately after the last character drawn.
 */
uint8_t ssd1306_draw_string(uint8_t x, uint8_t y, const char *str,
                             const uint8_t *font_data,
                             uint8_t font_w, uint8_t font_h, uint8_t scale);

/**
 * @brief Flush the in-RAM framebuffer to the SSD1306 over I2C.
 *
 * Sends all SSD1306_PAGES pages (5 × 72 bytes = 360 bytes) using the
 * horizontal addressing mode. Each page transfer is prefixed with a
 * control byte (0x40 = data stream). Column start is SSD1306_X_OFFSET (28)
 * to align with the visible window.
 *
 * @return ESP_OK on success; I2C error code on bus failure.
 * @note   Blocking call; takes ~5 ms at 400 kHz. Call only from the display
 *         timer callback, not from BLE callbacks.
 */
esp_err_t ssd1306_flush(void);
