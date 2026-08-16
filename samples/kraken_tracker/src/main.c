/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <date_time.h>


#include <zephyr/drivers/i2c.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem_at.h>
#include <nrf_modem_gnss.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <date_time.h>

#include <zephyr/net/socket.h>
#include <zephyr/kernel.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/crc.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/reboot.h>
#include <time.h>
#include <hal/nrf_gpio.h>

#include "battery.h"
#include "spectre_logo.h"



LOG_MODULE_REGISTER(gnss_sample, CONFIG_GNSS_SAMPLE_LOG_LEVEL);

static void log_modem_state(const char *stage)
{
	char response[512];
	int err;

	LOG_INF("%s antenna config: COEX0=%s", stage, CONFIG_MODEM_ANTENNA_AT_COEX0);

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XSYSTEMMODE?");
	if (err == 0) {
		LOG_INF("%s XSYSTEMMODE: %s", stage, response);
	} else {
		LOG_WRN("%s XSYSTEMMODE query failed: %d", stage, err);
	}

	err = nrf_modem_at_cmd(response, sizeof(response), "AT+CFUN?");
	if (err == 0) {
		LOG_INF("%s CFUN: %s", stage, response);
	} else {
		LOG_WRN("%s CFUN query failed: %d", stage, err);
	}

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XCOEX0?");
	if (err == 0) {
		LOG_INF("%s XCOEX0: %s", stage, response);
	} else {
		LOG_WRN("%s XCOEX0 query failed: %d", stage, err);
	}

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XMAGPIO?");
	if (err == 0) {
		LOG_INF("%s XMAGPIO: %s", stage, response);
	} else {
		LOG_WRN("%s XMAGPIO query failed: %d", stage, err);
	}

	err = nrf_modem_at_cmd(response, sizeof(response), "AT%%XMONITOR");
	if (err == 0) {
		LOG_INF("%s XMONITOR: %s", stage, response);
	} else {
		LOG_WRN("%s XMONITOR query failed: %d", stage, err);
	}
}



#define SSD1306_I2C_ADDR 0x3D
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES (SSD1306_HEIGHT / 8)

// Fonction pour envoyer une commande à l'écran
static int ssd1306_write_cmd(const struct device *i2c_dev, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd}; // 0x00 = Control byte pour commande
    return i2c_write(i2c_dev, buf, 2, SSD1306_I2C_ADDR);
}

// Fonction pour envoyer des données (pixels)
static int ssd1306_write_data(const struct device *i2c_dev, uint8_t data) {
    uint8_t buf[2] = {0x40, data}; // 0x40 = Control byte pour data
    return i2c_write(i2c_dev, buf, 2, SSD1306_I2C_ADDR);
}

static int ssd1306_write_data_block(const struct device *i2c_dev, const uint8_t *data, size_t len)
{
	uint8_t buf[1 + 6];

	if (len > 6) {
		return -EINVAL;
	}

	buf[0] = 0x40;
	memcpy(&buf[1], data, len);
	return i2c_write(i2c_dev, buf, len + 1, SSD1306_I2C_ADDR);
}


static uint8_t framebuffer[SSD1306_WIDTH][SSD1306_PAGES] = {0};
static struct k_mutex display_mutex;

void clear_display(const struct device *i2c_dev);



void allumer_pixel(const struct device *i2c_dev, uint8_t x, uint8_t y) {
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
	k_mutex_lock(&display_mutex, K_FOREVER);
    //extern uint8_t framebuffer[128][4]; // ou déclare-le globalement
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    framebuffer[x][page] |= (1 << bit);

    // Positionne le curseur sur la colonne et la page
    ssd1306_write_cmd(i2c_dev, 0x21); // Set column address
    ssd1306_write_cmd(i2c_dev, x);    // Start column
    ssd1306_write_cmd(i2c_dev, x);    // End column
    ssd1306_write_cmd(i2c_dev, 0x22); // Set page address
    ssd1306_write_cmd(i2c_dev, page); // Start page
    ssd1306_write_cmd(i2c_dev, page); // End page

    // Écrit la page entière (tous les bits de la colonne pour cette page)
    ssd1306_write_data(i2c_dev, framebuffer[x][page]);
	k_mutex_unlock(&display_mutex);
}

void eteindre_pixel(const struct device *i2c_dev, uint8_t x, uint8_t y) {
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
	k_mutex_lock(&display_mutex, K_FOREVER);
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    framebuffer[x][page] &= ~(1 << bit);

    ssd1306_write_cmd(i2c_dev, 0x21);
    ssd1306_write_cmd(i2c_dev, x);
    ssd1306_write_cmd(i2c_dev, x);
    ssd1306_write_cmd(i2c_dev, 0x22);
    ssd1306_write_cmd(i2c_dev, page);
    ssd1306_write_cmd(i2c_dev, page);

    ssd1306_write_data(i2c_dev, framebuffer[x][page]);
	k_mutex_unlock(&display_mutex);
}

static void ssd1306_init(const struct device *i2c_dev) {
	k_mutex_lock(&display_mutex, K_FOREVER);
    ssd1306_write_cmd(i2c_dev, 0xAE); // Display OFF
    ssd1306_write_cmd(i2c_dev, 0xD5); // Set display clock divide ratio/oscillator freq
    ssd1306_write_cmd(i2c_dev, 0x80); // Default setting for display clock divide ratio
	ssd1306_write_cmd(i2c_dev, 0xA8); // Set multiplex ratio
	ssd1306_write_cmd(i2c_dev, 0x3F); // 1/64 duty
    ssd1306_write_cmd(i2c_dev, 0xD3); // Set display offset
    ssd1306_write_cmd(i2c_dev, 0x00); // No offset
    ssd1306_write_cmd(i2c_dev, 0x40); // Set start line at 0
    ssd1306_write_cmd(i2c_dev, 0x8D); // Enable charge pump regulator
    ssd1306_write_cmd(i2c_dev, 0x14);
    ssd1306_write_cmd(i2c_dev, 0x20); // Set memory addressing mode
    ssd1306_write_cmd(i2c_dev, 0x00); // Horizontal addressing mode
    ssd1306_write_cmd(i2c_dev, 0xA0); // Set segment re-map 0 to 127
    ssd1306_write_cmd(i2c_dev, 0xC0); // Set COM output scan direction remapped
	ssd1306_write_cmd(i2c_dev, 0xDA); // Set COM pins hardware configuration
	ssd1306_write_cmd(i2c_dev, 0x12); // COM config for 128x64
    ssd1306_write_cmd(i2c_dev, 0x81); // Set contrast control
    ssd1306_write_cmd(i2c_dev, 0xCF);
    ssd1306_write_cmd(i2c_dev, 0xD9); // Set pre-charge period
    ssd1306_write_cmd(i2c_dev, 0xF1);
    ssd1306_write_cmd(i2c_dev, 0xDB); // Set VCOMH deselect level
    ssd1306_write_cmd(i2c_dev, 0x40);
    ssd1306_write_cmd(i2c_dev, 0xA4); // Entire display ON (resume)
    ssd1306_write_cmd(i2c_dev, 0xA6); // Set normal display (not inverted)
    ssd1306_write_cmd(i2c_dev, 0xAF); // Display ON
	k_mutex_unlock(&display_mutex);
}

static void draw_startup_logo(const struct device *i2c_dev)
{
	uint8_t page_buf[SSD1306_WIDTH];
	uint8_t src_x;
	uint8_t src_page;
	const uint8_t startup_logo_shift_up_pages = 0;

	memset(framebuffer, 0, sizeof(framebuffer));
	clear_display(i2c_dev);
	for (src_page = 0; src_page < SSD1306_PAGES; src_page++) {
		if (src_page < startup_logo_shift_up_pages) {
			continue;
		}

		for (src_x = 0; src_x < SSD1306_WIDTH; src_x++) {
			uint8_t column = spectre_logo_128x64[(src_page * SSD1306_WIDTH) + src_x];
			framebuffer[src_x][src_page - startup_logo_shift_up_pages] = column;
		}
	}

	k_mutex_lock(&display_mutex, K_FOREVER);
	for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
		memset(page_buf, 0, sizeof(page_buf));
		for (uint8_t x = 0; x < SSD1306_WIDTH; x++) {
			page_buf[x] = framebuffer[x][page];
		}
		ssd1306_write_cmd(i2c_dev, 0x21);
		ssd1306_write_cmd(i2c_dev, 0);
		ssd1306_write_cmd(i2c_dev, SSD1306_WIDTH - 1);
		ssd1306_write_cmd(i2c_dev, 0x22);
		ssd1306_write_cmd(i2c_dev, page);
		ssd1306_write_cmd(i2c_dev, page);
		for (uint8_t x = 0; x < SSD1306_WIDTH; x += 6) {
			size_t len = (SSD1306_WIDTH - x >= 6) ? 6 : (SSD1306_WIDTH - x);
			ssd1306_write_data_block(i2c_dev, &page_buf[x], len);
		}
	}

	k_mutex_unlock(&display_mutex);
}

// Table ASCII '0'-'9', 'A'-'Z' (0-9 puis A-Z)
static const uint8_t font_5x7[96][5] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00 },  // 20  32
	{ 0x00, 0x00, 0x4F, 0x00, 0x00 },  // 21  33  !
	{ 0x00, 0x07, 0x00, 0x07, 0x00 },  // 22  34  "
	{ 0x14, 0x7F, 0x14, 0x7F, 0x14 },  // 23  35  #
	{ 0x24, 0x2A, 0x7F, 0x2A, 0x12 },  // 24  36  $
	{ 0x23, 0x13, 0x08, 0x64, 0x62 },  // 25  37  %
	{ 0x36, 0x49, 0x55, 0x22, 0x50 },  // 26  38  &
	{ 0x00, 0x05, 0x03, 0x00, 0x00 },  // 27  39  '
	{ 0x00, 0x1C, 0x22, 0x41, 0x00 },  // 28  40  (
	{ 0x00, 0x41, 0x22, 0x1C, 0x00 },  // 29  41  )
	{ 0x14, 0x08, 0x3E, 0x08, 0x14 },  // 2A  42  *
	{ 0x08, 0x08, 0x3E, 0x08, 0x08 },  // 2B  43  +
	{ 0x00, 0x50, 0x30, 0x00, 0x00 },  // 2C  44  ,
	{ 0x08, 0x08, 0x08, 0x08, 0x08 },  // 2D  45  -
	{ 0x00, 0x60, 0x60, 0x00, 0x00 },  // 2E  46  .
	{ 0x20, 0x10, 0x08, 0x04, 0x02 },  // 2F  47  /
	{ 0x3E, 0x51, 0x49, 0x45, 0x3E },  // 30  48  0
	{ 0x00, 0x42, 0x7F, 0x40, 0x00 },  // 31  49  1
	{ 0x42, 0x61, 0x51, 0x49, 0x46 },  // 32  50  2
	{ 0x21, 0x41, 0x45, 0x4B, 0x31 },  // 33  51  3
	{ 0x18, 0x14, 0x12, 0x7F, 0x10 },  // 34  52  4
	{ 0x27, 0x45, 0x45, 0x45, 0x39 },  // 35  53  5
	{ 0x3C, 0x4A, 0x49, 0x49, 0x30 },  // 36  54  6
	{ 0x03, 0x01, 0x71, 0x09, 0x07 },  // 37  55  7
	{ 0x36, 0x49, 0x49, 0x49, 0x36 },  // 38  56  8
	{ 0x06, 0x49, 0x49, 0x29, 0x1E },  // 39  57  9
	{ 0x00, 0x36, 0x36, 0x00, 0x00 },  // 3A  58  :
	{ 0x00, 0x56, 0x36, 0x00, 0x00 },  // 3B  59  ;
	{ 0x08, 0x14, 0x22, 0x41, 0x00 },  // 3C  60  <
	{ 0x14, 0x14, 0x14, 0x14, 0x14 },  // 3D  61  =
	{ 0x00, 0x41, 0x22, 0x14, 0x08 },  // 3E  62  >
	{ 0x02, 0x01, 0x51, 0x09, 0x06 },  // 3F  63  ?
	{ 0x32, 0x49, 0x79, 0x41, 0x3E },  // 40  64  @
	{ 0x7E, 0x11, 0x11, 0x11, 0x7E },  // 41  65  A
	{ 0x7F, 0x49, 0x49, 0x49, 0x36 },  // 42  66  B
	{ 0x3E, 0x41, 0x41, 0x41, 0x22 },  // 43  67  C
	{ 0x7F, 0x41, 0x41, 0x22, 0x1C },  // 44  68  D
	{ 0x7F, 0x49, 0x49, 0x49, 0x41 },  // 45  69  E
	{ 0x7F, 0x09, 0x09, 0x09, 0x01 },  // 46  70  F
	{ 0x3E, 0x41, 0x49, 0x49, 0x7A },  // 47  71  G
	{ 0x7F, 0x08, 0x08, 0x08, 0x7F },  // 48  72  H
	{ 0x00, 0x41, 0x7F, 0x41, 0x00 },  // 49  73  I
	{ 0x20, 0x40, 0x41, 0x3F, 0x01 },  // 4A  74  J
	{ 0x7F, 0x08, 0x14, 0x22, 0x41 },  // 4B  75  K
	{ 0x7F, 0x40, 0x40, 0x40, 0x40 },  // 4C  76  L
	{ 0x7F, 0x02, 0x0C, 0x02, 0x7F },  // 4D  77  M
	{ 0x7F, 0x04, 0x08, 0x10, 0x7F },  // 4E  78  N
	{ 0x3E, 0x41, 0x41, 0x41, 0x3E },  // 4F  79  O
	{ 0x7F, 0x09, 0x09, 0x09, 0x06 },  // 50  80  P
	{ 0x3E, 0x41, 0x51, 0x21, 0x5E },  // 51  81  Q
	{ 0x7F, 0x09, 0x19, 0x29, 0x46 },  // 52  82  R
	{ 0x46, 0x49, 0x49, 0x49, 0x31 },  // 53  83  S
	{ 0x01, 0x01, 0x7F, 0x01, 0x01 },  // 54  84  T
	{ 0x3F, 0x40, 0x40, 0x40, 0x3F },  // 55  85  U
	{ 0x1F, 0x20, 0x40, 0x20, 0x1F },  // 56  86  V
	{ 0x3F, 0x40, 0x38, 0x40, 0x3F },  // 57  87  W
	{ 0x63, 0x14, 0x08, 0x14, 0x63 },  // 58  88  X
	{ 0x07, 0x08, 0x70, 0x08, 0x07 },  // 59  89  Y
	{ 0x61, 0x51, 0x49, 0x45, 0x43 },  // 5A  90  Z
	{ 0x7F, 0x41, 0x41, 0x00, 0x00 },  // 5B  91  [
	{ 0x15, 0x16, 0x7C, 0x16, 0x15 },  // 5C  92  '\'
	{ 0x00, 0x41, 0x41, 0x7F, 0x00 },  // 5D  93  ]
	{ 0x04, 0x02, 0x01, 0x02, 0x04 },  // 5E  94  ^
	{ 0x40, 0x40, 0x40, 0x40, 0x40 },  // 5F  95  _
	{ 0x00, 0x01, 0x02, 0x04, 0x00 },  // 60  96  `
	{ 0x20, 0x54, 0x54, 0x54, 0x78 },  // 61  97  a
	{ 0x7F, 0x48, 0x44, 0x44, 0x38 },  // 62  98  b
	{ 0x38, 0x44, 0x44, 0x44, 0x20 },  // 63  99  c
	{ 0x38, 0x44, 0x44, 0x48, 0x7F },  // 64 100  d
	{ 0x38, 0x54, 0x54, 0x54, 0x18 },  // 65 101  e
	{ 0x08, 0x7E, 0x09, 0x01, 0x02 },  // 66 102  f
	{ 0x0C, 0x52, 0x52, 0x52, 0x3E },  // 67 103  g
	{ 0x7F, 0x08, 0x04, 0x04, 0x78 },  // 68 104  h
	{ 0x00, 0x44, 0x7D, 0x40, 0x00 },  // 69 105  i
	{ 0x20, 0x40, 0x44, 0x3D, 0x00 },  // 6A 106  j
	{ 0x7F, 0x10, 0x28, 0x44, 0x00 },  // 6B 107  k
	{ 0x00, 0x41, 0x7F, 0x40, 0x00 },  // 6C 108  l
	{ 0x7C, 0x04, 0x18, 0x04, 0x78 },  // 6D 109  m
	{ 0x7C, 0x08, 0x04, 0x04, 0x78 },  // 6E 110  n
	{ 0x38, 0x44, 0x44, 0x44, 0x38 },  // 6F 111  o
	{ 0x7C, 0x14, 0x14, 0x14, 0x08 },  // 70 112  p
	{ 0x08, 0x14, 0x14, 0x18, 0x7C },  // 71 113  q
	{ 0x7C, 0x08, 0x04, 0x04, 0x08 },  // 72 114  r
	{ 0x48, 0x54, 0x54, 0x54, 0x20 },  // 73 115  s
	{ 0x04, 0x3F, 0x44, 0x40, 0x20 },  // 74 116  t
	{ 0x3C, 0x40, 0x40, 0x20, 0x7C },  // 75 117  u
	{ 0x1C, 0x20, 0x40, 0x20, 0x1C },  // 76 118  v
	{ 0x3C, 0x40, 0x38, 0x40, 0x3C },  // 77 119  w
	{ 0x44, 0x28, 0x10, 0x28, 0x44 },  // 78 120  x
	{ 0x0C, 0x50, 0x50, 0x50, 0x3C },  // 79 121  y
	{ 0x44, 0x64, 0x54, 0x4C, 0x44 },  // 7A 122  z
	{ 0x00, 0x08, 0x36, 0x41, 0x00 },  // 7B 123  {
	{ 0x00, 0x00, 0x7F, 0x00, 0x00 },  // 7C 124  |
	{ 0x00, 0x41, 0x36, 0x08, 0x00 },  // 7D 125  }
	{ 0x08, 0x08, 0x2A, 0x1C, 0x08 },  // 7E 126  ~
	{ 0x08, 0x1C, 0x2A, 0x08, 0x08 }   // 7F 127
};

void dessiner_caractere(const struct device *i2c_dev, char c, uint8_t x, uint8_t y, bool erase) {
	if (x > (SSD1306_WIDTH - 6) || y > (SSD1306_HEIGHT - 7)) {
		return;
	}

	ARG_UNUSED(erase);

	/* Affiche tout caractère ASCII 32..127 (inclus). */
	if (c < 32 || c > 127) {
		return;
	}

	const uint8_t *bitmap = font_5x7[c - 32];

	/* Fast path: one page-aligned character can be pushed as one contiguous I2C block. */
	if ((y % 8U) == 0U) {
		uint8_t page = y / 8U;
		uint8_t glyph[6];

		for (uint8_t col = 0; col < 5; col++) {
			glyph[col] = bitmap[col];
		}
		glyph[5] = 0x00;

		k_mutex_lock(&display_mutex, K_FOREVER);
		for (uint8_t col = 0; col < 6; col++) {
			framebuffer[x + col][page] = glyph[col];
		}

		ssd1306_write_cmd(i2c_dev, 0x21);
		ssd1306_write_cmd(i2c_dev, x);
		ssd1306_write_cmd(i2c_dev, x + 5);
		ssd1306_write_cmd(i2c_dev, 0x22);
		ssd1306_write_cmd(i2c_dev, page);
		ssd1306_write_cmd(i2c_dev, page);
		ssd1306_write_data_block(i2c_dev, glyph, sizeof(glyph));
		k_mutex_unlock(&display_mutex);
		return;
	}

	/* Fallback for non-page-aligned rendering. */
	for (uint8_t col = 0; col < 5; col++) {
		uint8_t bits = bitmap[col];
		for (uint8_t row = 0; row < 7; row++) {
			if (bits & (1 << row)) {
				allumer_pixel(i2c_dev, x + col, y + row);
			} else {
				eteindre_pixel(i2c_dev, x + col, y + row);
			}
		}
	}

	/* Nettoie la colonne d'espacement pour eviter les residus entre caracteres. */
	for (uint8_t row = 0; row < 7; row++) {
		eteindre_pixel(i2c_dev, x + 5, y + row);
	}
}


// Rayon moyen de la Terre en mètres
#define EARTH_RADIUS 6371000.0

#define PI 3.14159265358979323846


void calcul_cap_distance_int(double lat1, double lon1, double lat2, double lon2, int *cap_deg, int *distance_m) {
    // Conversion degrés -> radians
    double lat1_rad = lat1 * PI / 180.0;
    double lon1_rad = lon1 * PI / 180.0;
    double lat2_rad = lat2 * PI / 180.0;
    double lon2_rad = lon2 * PI / 180.0;

    // Formule de Haversine pour la distance
    double dlat = lat2_rad - lat1_rad;
    double dlon = lon2_rad - lon1_rad;
    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    *distance_m = (int)round(EARTH_RADIUS * c);

    // Calcul du cap (azimut initial)
    double y = sin(dlon) * cos(lat2_rad);
    double x = cos(lat1_rad)*sin(lat2_rad) -
               sin(lat1_rad)*cos(lat2_rad)*cos(dlon);
    double cap_rad = atan2(y, x);
    *cap_deg = (int)round(fmod((cap_rad * 180.0 / PI) + 360.0, 360.0));
}


#define EARTH_RADIUS_METERS (6371.0 * 1000.0)

struct shared_state {
	struct k_mutex mutex;
	struct k_condvar cond;
	bool ready;
};

static struct shared_state state;
K_SEM_DEFINE(time_sem, 0, 1);
static struct k_work agps_data_get_work;
static volatile bool requesting_assistance;
static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static volatile bool gnss_has_fix;
static bool ref_used;
static double ref_latitude;
static double ref_longitude;
static int64_t fix_timestamp;
static struct k_mutex lte_mutex;

static const char *gnss_antenna_mode(void)
{
#if defined(CONFIG_MODEM_ANTENNA_GNSS_EXTERNAL)
	return "external";
#else
	return "internal";
#endif
}

static const struct battery_level_point battery_curve[] = {
	{ 10000, 4200 },
	{ 9500, 4100 },
	{ 9000, 4050 },
	{ 8000, 3950 },
	{ 7000, 3900 },
	{ 6000, 3850 },
	{ 5000, 3800 },
	{ 4000, 3750 },
	{ 3000, 3700 },
	{ 2000, 3600 },
	{ 1000, 3500 },
	{ 0, 3300 },
};

static int board_battery_mv_get(int *millivolts)
{
	int rc = battery_measure_enable(true);

	if (rc < 0) {
		return rc;
	}

	rc = battery_sample();
	battery_measure_enable(false);

	if (rc < 0) {
		return rc;
	}

	*millivolts = rc;
	return 0;
}

static int board_battery_percent_get(uint8_t *percent, int *millivolts)
{
	int batt_mv = 0;
	int rc = board_battery_mv_get(&batt_mv);

	if (rc < 0) {
		return rc;
	}

	unsigned int pptt = battery_level_pptt((unsigned int)batt_mv, battery_curve);
	unsigned int pct = (pptt + 50U) / 100U;

	if (pct > 99U) {
		pct = 99U;
	}

	*percent = (uint8_t)pct;
	if (millivolts != NULL) {
		*millivolts = batt_mv;
	}
	return 0;
}

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) || defined(CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST)
static struct k_work_q gnss_work_q;

#define GNSS_WORKQ_THREAD_STACK_SIZE 2304
#define GNSS_WORKQ_THREAD_PRIORITY   5

K_THREAD_STACK_DEFINE(gnss_workq_stack_area, GNSS_WORKQ_THREAD_STACK_SIZE);
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE || CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST */

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)
#include "assistance.h"

static struct nrf_modem_gnss_agps_data_frame last_agps;

static void gnss_event_handler(int event)
{
	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		if (nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT) == 0) {
			k_mutex_lock(&state.mutex, K_FOREVER);
			state.ready = true;
			k_condvar_broadcast(&state.cond);
			k_mutex_unlock(&state.mutex);
		}
		break;

	case NRF_MODEM_GNSS_EVT_AGPS_REQ:
		if (nrf_modem_gnss_read(&last_agps, sizeof(last_agps), NRF_MODEM_GNSS_DATA_AGPS_REQ) == 0) {
			k_work_submit_to_queue(&gnss_work_q, &agps_data_get_work);
		}
		break;

	default:
		break;
	}
}

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
K_SEM_DEFINE(lte_ready, 0, 1);

static void lte_lc_event_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
		    (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			LOG_INF("Connected to LTE network");
			k_sem_give(&lte_ready);
		}
		break;

	default:
		break;
	}
}
#endif /* CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
void lte_connect(void)
{
	int err;

	LOG_INF("Connecting to LTE network...");

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_LTE);
	if (err) {
		LOG_ERR("Failed to activate LTE, error: %d", err);
		return;
	}

	k_sem_take(&lte_ready, K_FOREVER);

	/* Wait for a while, because with IPv4v6 PDN the IPv6 activation takes a bit more time. */
	k_sleep(K_SECONDS(1));
}

void lte_disconnect(void)
{
	int err;

	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	if (err) {
		LOG_ERR("Failed to deactivate LTE, error: %d", err);
		return;
	}

	LOG_INF("LTE disconnected");
}
#endif /* CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

static void agps_data_get_work_fn(struct k_work *item)
{
	ARG_UNUSED(item);

	int err;

#if defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_SUPL)
	/* SUPL doesn't usually provide NeQuick ionospheric corrections and satellite real time
	 * integrity information. If GNSS asks only for those, the request should be ignored.
	 */
	if (last_agps.sv_mask_ephe == 0 &&
	    last_agps.sv_mask_alm == 0 &&
	    (last_agps.data_flags & ~(NRF_MODEM_GNSS_AGPS_NEQUICK_REQUEST |
				      NRF_MODEM_GNSS_AGPS_INTEGRITY_REQUEST)) == 0) {
		LOG_INF("Ignoring assistance request for only NeQuick and/or integrity");
		return;
	}
#endif /* CONFIG_GNSS_SAMPLE_ASSISTANCE_SUPL */

#if defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_MINIMAL)
	/* With minimal assistance, the request should be ignored if no GPS time or position
	 * is requested.
	 */
	if (!(last_agps.data_flags & NRF_MODEM_GNSS_AGPS_SYS_TIME_AND_SV_TOW_REQUEST) &&
	    !(last_agps.data_flags & NRF_MODEM_GNSS_AGPS_POSITION_REQUEST)) {
		LOG_INF("Ignoring assistance request because no GPS time or position is requested");
		return;
	}
#endif /* CONFIG_GNSS_SAMPLE_ASSISTANCE_MINIMAL */

	requesting_assistance = true;

	LOG_INF("Assistance data needed, ephe 0x%08x, alm 0x%08x, flags 0x%02x",
		last_agps.sv_mask_ephe,
		last_agps.sv_mask_alm,
		last_agps.data_flags);

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
	k_mutex_lock(&lte_mutex, K_FOREVER);
	lte_connect();
#endif /* CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

	err = assistance_request(&last_agps);
	if (err) {
		LOG_ERR("Failed to request assistance data");
	}

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
	lte_disconnect();
	k_mutex_unlock(&lte_mutex);
#endif /* CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

	requesting_assistance = false;
}
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE */

	#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) && !defined(CONFIG_GNSS_SAMPLE_STRICT_GNSS_ONLY)
	static int startup_assistance_preload(void)
	{
		LOG_INF("A-GPS startup preload skipped");
		return 0;
	}
	#endif




static void date_time_evt_handler(const struct date_time_evt *evt)
{
	k_sem_give(&time_sem);
}

static int modem_init(void)
{
	if (IS_ENABLED(CONFIG_DATE_TIME)) {
		date_time_register_handler(date_time_evt_handler);
	}

	if (lte_lc_init() != 0) {
		LOG_ERR("Failed to initialize LTE link controller");
		return -1;
	}

#if defined(CONFIG_GNSS_SAMPLE_STRICT_GNSS_ONLY)
	LOG_INF("Strict GNSS-only test mode: leaving LTE deactivated");
	return 0;
#endif

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
	lte_lc_register_handler(lte_lc_event_handler);
	lte_lc_psm_req(true);

	if (lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_LTEM_GPS,
				   LTE_LC_SYSTEM_MODE_PREFER_LTEM) != 0) {
		LOG_ERR("Failed to set LTE/GPS system mode");
		return -1;
	}
#elif !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)
	lte_lc_psm_req(true);

	LOG_INF("Connecting to LTE network");

	if (lte_lc_connect() != 0) {
		LOG_ERR("Failed to connect to LTE network");
		return -1;
	}

	LOG_INF("Connected to LTE network");

	if (IS_ENABLED(CONFIG_DATE_TIME)) {
		LOG_INF("Waiting for current time");

		/* Wait for an event from the Date Time library. */
		k_sem_take(&time_sem, K_MINUTES(10));

		if (!date_time_is_valid()) {
			LOG_WRN("Failed to get current time, continuing anyway");
		}
	}
#endif

	return 0;
}

static int sample_init(void)
{
	int err = 0;

#if (!defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) && !defined(CONFIG_GNSS_SAMPLE_STRICT_GNSS_ONLY)) || \
	defined(CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST)
	struct k_work_queue_config cfg = {
		.name = "gnss_work_q",
		.no_yield = false
	};

	k_work_queue_start(
		&gnss_work_q,
		gnss_workq_stack_area,
		K_THREAD_STACK_SIZEOF(gnss_workq_stack_area),
		GNSS_WORKQ_THREAD_PRIORITY,
		&cfg);
	#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE || CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST */

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) && !defined(CONFIG_GNSS_SAMPLE_STRICT_GNSS_ONLY)
	k_work_init(&agps_data_get_work, agps_data_get_work_fn);

	err = assistance_init(&gnss_work_q);
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE */

	return err;
}

static int gnss_init_and_start(void)
{


#if defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) || defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
	/* Enable GNSS. */
	if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS) != 0) {
		LOG_ERR("Failed to activate GNSS functional mode");
		return -1;
	}
#endif /* CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE || CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

	/* Configure GNSS. */
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return -1;
	}

	/* NMEA output is disabled because this application does not consume it. */
	uint16_t nmea_mask = 0;

	if (nrf_modem_gnss_nmea_mask_set(nmea_mask) != 0) {
		LOG_ERR("Failed to set GNSS NMEA mask");
		return -1;
	}

	/* This use case flag should always be set. */
	uint8_t use_case = NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START;

	if (IS_ENABLED(CONFIG_GNSS_SAMPLE_MODE_PERIODIC) &&
	    !IS_ENABLED(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)) {
		/* Disable GNSS scheduled downloads when assistance is used. */
		use_case |= NRF_MODEM_GNSS_USE_CASE_SCHED_DOWNLOAD_DISABLE;
	}

	if (IS_ENABLED(CONFIG_GNSS_SAMPLE_LOW_ACCURACY)) {
		use_case |= NRF_MODEM_GNSS_USE_CASE_LOW_ACCURACY;
	}

	if (nrf_modem_gnss_use_case_set(use_case) != 0) {
		LOG_WRN("Failed to set GNSS use case");
	}

#if defined(CONFIG_NRF_CLOUD_AGPS_ELEVATION_MASK)
	if (nrf_modem_gnss_elevation_threshold_set(CONFIG_NRF_CLOUD_AGPS_ELEVATION_MASK) != 0) {
		LOG_ERR("Failed to set elevation threshold");
		return -1;
	}
	LOG_DBG("Set elevation threshold to %u", CONFIG_NRF_CLOUD_AGPS_ELEVATION_MASK);
#endif

#if defined(CONFIG_GNSS_SAMPLE_MODE_CONTINUOUS)
	/* Default to no power saving. */
	uint8_t power_mode = NRF_MODEM_GNSS_PSM_DISABLED;

#if defined(GNSS_SAMPLE_POWER_SAVING_MODERATE)
	power_mode = NRF_MODEM_GNSS_PSM_DUTY_CYCLING_PERFORMANCE;
#elif defined(GNSS_SAMPLE_POWER_SAVING_HIGH)
	power_mode = NRF_MODEM_GNSS_PSM_DUTY_CYCLING_POWER;
#endif

	if (nrf_modem_gnss_power_mode_set(power_mode) != 0) {
		LOG_ERR("Failed to set GNSS power saving mode");
		return -1;
	}
#endif /* CONFIG_GNSS_SAMPLE_MODE_CONTINUOUS */

	/* Default to continuous tracking. */
	uint16_t fix_retry = 0;
	uint16_t fix_interval = 1;

#if defined(CONFIG_GNSS_SAMPLE_MODE_PERIODIC)
	fix_retry = CONFIG_GNSS_SAMPLE_PERIODIC_TIMEOUT;
	fix_interval = CONFIG_GNSS_SAMPLE_PERIODIC_INTERVAL;
#elif defined(CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST)
	/* Single fix for TTFF test mode. */
	fix_retry = 0;
	fix_interval = 0;
#endif

	if (nrf_modem_gnss_fix_retry_set(fix_retry) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return -1;
	}

	if (nrf_modem_gnss_fix_interval_set(fix_interval) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return -1;
	}


#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) && !defined(CONFIG_GNSS_SAMPLE_STRICT_GNSS_ONLY)
	/* Preload assistance at startup to improve cold-start TTFF. */
	int preload_err = startup_assistance_preload();
	if (preload_err) {
		LOG_WRN("A-GPS startup preload returned error: %d", preload_err);
	} else {
		LOG_INF("A-GPS startup preload completed");
	}
#endif

	log_modem_state("Before GNSS start");

	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return -1;
	}


	return 0;
}























float targets[10];
const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
int targets_count = 0;

uint8_t last_tracked = 0;
uint8_t last_in_fix = 0;
uint8_t last_unhealthy = 0;

uint8_t gps_tracking = 0;
uint8_t gps_using = 0;
uint8_t gps_unk = 0;


double last_latitude = 0;
double last_longitude = 0;

double ref_latitude2 = 0;
double ref_longitude2 = 0;

int lte_wait = 0;
int lte_quota = 10;

volatile int position=0;
float lat[5];
float lng[5];
time_t ts[5];

#define HTTP_HOST "plongee.duckdns.org"
#define HTTP_PATH "/targets"
#define HTTP_PATH_LONG "/coords"
#define HTTP_PATH_MANUAL_POINT "/api/manual-point"
#define HTTP_PORT 8545
#define MAX_MTU_SIZE     2000
#define RECV_BUF_SIZE    2048
#define SEND_BUF_SIZE    MAX_MTU_SIZE
#define MAX_DISPLAY_TARGETS 4

static int displayed_targets_count = -1;
static int displayed_heading[MAX_DISPLAY_TARGETS] = { -1, -1, -1, -1 };
static int displayed_distance[MAX_DISPLAY_TARGETS] = { -1, -1, -1, -1 };

void write_line(const struct device *i2c_dev, const char *line, uint8_t x, uint8_t y, int erase);

static void reset_target_value_cache(void)
{
	for (int i = 0; i < MAX_DISPLAY_TARGETS; i++) {
		displayed_heading[i] = -1;
		displayed_distance[i] = -1;
	}
}

static void clear_target_rows(const struct device *i2c_dev, uint8_t base_y)
{
	char blank_row[23];
	memset(blank_row, ' ', sizeof(blank_row) - 1);
	blank_row[sizeof(blank_row) - 1] = '\0';

	for (int row = 0; row < MAX_DISPLAY_TARGETS; row++) {
		uint8_t y = base_y + (row * 8);
		write_line(i2c_dev, blank_row, 0, y, 0);
	}
}

static void draw_target_rows_layout(const struct device *i2c_dev, int count, uint8_t base_y)
{
	uint8_t y = base_y;
	char message[16];

	if (count < 0) {
		count = 0;
	}
	if (count > MAX_DISPLAY_TARGETS) {
		count = MAX_DISPLAY_TARGETS;
	}

	clear_target_rows(i2c_dev, base_y);

	for (int i = 0; i < count; i++) {
		if (y > (SSD1306_HEIGHT - 8)) {
			break;
		}

		snprintf(message, sizeof(message), "%1d:", i + 1);
		write_line(i2c_dev, message, 0, y, 0);
		write_line(i2c_dev, "DEG", 7 * 6, y, 0);
		write_line(i2c_dev, "|", 12 * 6, y, 0);
		write_line(i2c_dev, "M", 20 * 6, y, 0);

		y += 8;
	}

	displayed_targets_count = count;
	reset_target_value_cache();
}

#define JSON_TEMPLATE "%ld,%.9f,%.9f"
#define JSON_TEMPLATE_LONG "%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f"

char send_buf[2000];

bool gps_state = 1;
bool lte_state = 0;
time_t timestamp;
volatile float avg = 0;
float thresold = 19;
int reset_threshold = 20;

int refresh_rate = 1000;
int confirmation_rate = 20;

int menu = -1;

enum tap_direction {
	TAP_NONE = 0,
	TAP_UP,
	TAP_DOWN,
	TAP_LEFT,
	TAP_RIGHT,
};

static struct k_mutex tap_mutex;
static volatile enum tap_direction pending_tap = TAP_NONE;
static volatile bool wake_request = false;
static struct k_sem wake_sem;
static struct k_mutex motion_mutex;
static struct k_condvar motion_cond;
static bool motion_active = true;
static const struct device *accel_sensor;
static int64_t off_wake_armed_after_ms;
static volatile bool off_mode_active;
static volatile int64_t off_wake_first_motion_ms;
static volatile int64_t off_wake_last_motion_ms;
static volatile int off_wake_poll_streak;
static volatile bool off_wake_probe_active;
static volatile int64_t menu_wake_first_motion_ms;
static volatile int64_t menu_wake_last_motion_ms;
static volatile int menu_wake_poll_streak;
static volatile bool menu_wake_probe_active;
static const float off_wake_start_threshold_ms2 = 2.20f;
static const float off_wake_hold_threshold_ms2 = 1.20f;
static const int off_wake_hold_ms = 1300;
static const int off_wake_event_gap_ms = 180;
static const int off_wake_required_streak = 8;
static const int off_wake_idle_wait_ms = 250;
static const int off_wake_fast_wait_ms = 30;
static const int off_wake_odr_hz = 12;
static const int active_odr_hz = 100;
static const int logo_preview_ms = 5000;
/* SAFE guarantees wake without IRQ routing; ECO uses System OFF for best power. */
static bool off_use_system_off = false;
static const struct sensor_trigger wake_trigger = {
	.type = SENSOR_TRIG_DELTA,
	.chan = SENSOR_CHAN_ACCEL_XYZ,
};

/* Tune these values on hardware if needed. */
static float tap_delta_threshold = 1.45f;
static float tap_release_threshold = 0.50f;
static float tap_axis_dominance = 1.30f;
static float tap_direction_margin = 0.40f;
static int tap_direction_window_ms = 20;
static float tap_peak_weight = 0.60f;
static int tap_deadtime_ms = 250;
static int tap_boot_guard_ms = 7000;
static int tap_menu_confirm_guard_ms = 450;

/* Physical mounting orientation mapping. */
static bool top_is_negative_y = true;
static bool left_is_negative_x = true;

extern volatile float linear_x;
extern volatile float linear_y;
extern volatile float linear_z;

float sensor_value_to_float(const struct sensor_value *val);

static void off_wake_reset_motion(void);

static bool off_wake_register_motion(int64_t now_ms)
{
	int64_t previous_motion_ms;

	if (!off_mode_active || (now_ms < off_wake_armed_after_ms)) {
		return false;
	}

	previous_motion_ms = off_wake_last_motion_ms;
	if (off_wake_first_motion_ms == 0 ||
	    (now_ms - previous_motion_ms) > off_wake_event_gap_ms) {
		off_wake_first_motion_ms = now_ms;
		off_wake_last_motion_ms = now_ms;
		return false;
	}

	off_wake_last_motion_ms = now_ms;

	return (now_ms - off_wake_first_motion_ms) >= off_wake_hold_ms;
}

static void off_wake_reset_motion(void)
{
	off_wake_first_motion_ms = 0;
	off_wake_last_motion_ms = 0;
	off_wake_poll_streak = 0;
	off_wake_probe_active = false;
}

static bool menu_wake_register_motion(int64_t now_ms)
{
	int64_t previous_motion_ms;

	previous_motion_ms = menu_wake_last_motion_ms;
	if (menu_wake_first_motion_ms == 0 ||
	    (now_ms - previous_motion_ms) > off_wake_event_gap_ms) {
		menu_wake_first_motion_ms = now_ms;
		menu_wake_last_motion_ms = now_ms;
		return false;
	}

	menu_wake_last_motion_ms = now_ms;

	return (now_ms - menu_wake_first_motion_ms) >= off_wake_hold_ms;
}

static void menu_wake_reset_motion(void)
{
	menu_wake_first_motion_ms = 0;
	menu_wake_last_motion_ms = 0;
	menu_wake_poll_streak = 0;
	menu_wake_probe_active = false;
}

static bool menu_wake_poll_motion(int64_t now_ms)
{
	float linear_mag;
	float lx = linear_x;
	float ly = linear_y;
	float lz = linear_z;

	linear_mag = sqrtf((lx * lx) + (ly * ly) + (lz * lz));

	if (linear_mag < off_wake_start_threshold_ms2) {
		menu_wake_reset_motion();
		return false;
	}

	if (!menu_wake_probe_active) {
		menu_wake_probe_active = true;
	}

	if (linear_mag < off_wake_hold_threshold_ms2) {
		menu_wake_reset_motion();
		menu_wake_probe_active = true;
		return false;
	}

	if (++menu_wake_poll_streak < off_wake_required_streak) {
		return false;
	}

	return menu_wake_register_motion(now_ms);
}

static bool off_wake_poll_motion(int64_t now_ms)
{
	float linear_mag;
	float lx = linear_x;
	float ly = linear_y;
	float lz = linear_z;

	linear_mag = sqrtf((lx * lx) + (ly * ly) + (lz * lz));

	if (linear_mag < off_wake_start_threshold_ms2) {
		off_wake_reset_motion();
		return false;
	}

	if (!off_wake_probe_active) {
		off_wake_probe_active = true;
	}

	if (linear_mag < off_wake_hold_threshold_ms2) {
		off_wake_reset_motion();
		off_wake_probe_active = true;
		return false;
	}

	if (++off_wake_poll_streak < off_wake_required_streak) {
		return false;
	}

	return off_wake_register_motion(now_ms);
}

static void wake_trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	int64_t now_ms;

	ARG_UNUSED(dev);
	ARG_UNUSED(trig);

	now_ms = k_uptime_get();
	if (off_wake_register_motion(now_ms)) {
		wake_request = true;
		k_sem_give(&wake_sem);
	}
}

const char *menu_str[] = {
    "EXIT MENU",
	"GPS REACTIVATE",
	"RELOAD COORDINATES",
	"OFF"
};
int nb_menu = 4;

K_THREAD_STACK_DEFINE(accelerometer_stack, 1024);
K_THREAD_STACK_DEFINE(tap_stack, 1024);
K_THREAD_STACK_DEFINE(gps_stack, 1024);
K_THREAD_STACK_DEFINE(coordinates_stack, 2048);

struct k_thread accelerometer_data;
struct k_thread tap_data;
struct k_thread gps_data;
struct k_thread coordinates_data;

int measure_rate = 10;

float valuex = 0;
float valuey = 0;
float valuez = 0;
volatile float accel_x = 0.0f;
volatile float accel_y = 0.0f;
volatile float accel_z = 0.0f;
volatile float linear_x = 0.0f;
volatile float linear_y = 0.0f;
volatile float linear_z = 0.0f;

int tap_reset_rate = 20;
uint32_t last_fix = 0;
int gps_rate = 500;
volatile int first = 0;
uint32_t ref_lastfix = 0;
uint32_t last_gnss_diag = 0;
volatile bool gnss_display_dirty = true;
static int64_t tap_accept_after_ms;
static int64_t menu_confirm_after_ms;

enum gps_capture_mode {
	GPS_CAPTURE_DORMANT = 0,
	GPS_CAPTURE_REACTIVATING,
	GPS_CAPTURE_ACTIVE,
};

static volatile uint8_t gps_capture_mode = GPS_CAPTURE_REACTIVATING;
static bool active_point_valid;
static double active_point_lat;
static double active_point_lon;
static int64_t last_new_point_ms;
static bool gps_seen_fix_since_reactivate;
static volatile bool coordinates_fetch_running;

static void coordinates_thread(void *a, void *b, void *c);

#define GPS_NEW_POINT_MIN_DISTANCE_M 8
#define GPS_DORMANT_TIMEOUT_MS 120000

enum startup_stage {
	STARTUP_STAGE_GNSS_WARMUP = 0,
	STARTUP_STAGE_LTE_CONNECT,
	STARTUP_STAGE_DNS,
	STARTUP_STAGE_TCP_CONNECT,
	STARTUP_STAGE_HTTP_REQUEST,
	STARTUP_STAGE_WAIT_SERVER,
	STARTUP_STAGE_PARSE,
	STARTUP_STAGE_READY,
	STARTUP_STAGE_RETRY,
	STARTUP_STAGE_ERROR,
};

static volatile uint8_t startup_stage = STARTUP_STAGE_GNSS_WARMUP;

static const char *startup_stage_text(uint8_t stage)
{
	switch (stage) {
	case STARTUP_STAGE_GNSS_WARMUP:
		return "GNSS WARMUP";
	case STARTUP_STAGE_LTE_CONNECT:
		return "LTE CONNECT";
	case STARTUP_STAGE_DNS:
		return "DNS RESOLVE";
	case STARTUP_STAGE_TCP_CONNECT:
		return "TCP CONNECT";
	case STARTUP_STAGE_HTTP_REQUEST:
		return "HTTP REQUEST";
	case STARTUP_STAGE_WAIT_SERVER:
		return "WAIT SERVER";
	case STARTUP_STAGE_PARSE:
		return "PARSING DATA";
	case STARTUP_STAGE_READY:
		return "READY";
	case STARTUP_STAGE_RETRY:
		return "RETRY 60S";
	default:
		return "NET ERROR";
	}
}

static void show_startup_feedback_screen(const struct device *i2c_dev, uint8_t stage)
{
	char message[64];

	draw_target_rows_layout(i2c_dev, 0, 32);
	write_line(i2c_dev, "                ", 0, 24, 1);
	snprintf(message, sizeof(message), "%s", startup_stage_text(stage));
	write_line(i2c_dev, message, 0, 24, 1);
}





void clear_display(const struct device *i2c_dev) {
	k_mutex_lock(&display_mutex, K_FOREVER);
	// 1. Efface le framebuffer RAM
	memset(framebuffer, 0, sizeof(framebuffer));

	// 2. Pour chaque page, envoie 128 zéros d'un coup
	for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
		ssd1306_write_cmd(i2c_dev, 0x21); // Set column address
		ssd1306_write_cmd(i2c_dev, 0);    // Start column
		ssd1306_write_cmd(i2c_dev, SSD1306_WIDTH - 1);  // End column
		ssd1306_write_cmd(i2c_dev, 0x22); // Set page address
		ssd1306_write_cmd(i2c_dev, page); // Start page
		ssd1306_write_cmd(i2c_dev, page); // End page

		// Envoie 128 octets d'un coup (plus rapide que 128 appels séparés)
		uint8_t buf[129];
		buf[0] = 0x40; // Control byte for data
		memset(&buf[1], 0, SSD1306_WIDTH);
		i2c_write(i2c_dev, buf, sizeof(buf), SSD1306_I2C_ADDR);
	}
	k_mutex_unlock(&display_mutex);
}

void print_satellite_stats(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	uint8_t tracked = 0;
	uint8_t in_fix = 0;
	uint8_t unhealthy = 0;

	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
		if (pvt_data->sv[i].sv <= 0) {
			continue;
		}

		tracked++;
		if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) {
			in_fix++;
		}
		if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) {
			unhealthy++;
		}
	}

	gps_tracking = tracked;
	gps_using = in_fix;
	gps_unk = unhealthy;

	LOG_INF("GNSS searching: tracked=%u used=%u unhealthy=%u", tracked, in_fix, unhealthy);
}

void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	struct tm tm = { 0 };

	tm.tm_year = pvt_data->datetime.year - 1900;
	tm.tm_mon = pvt_data->datetime.month - 1;
	tm.tm_mday = pvt_data->datetime.day;
	tm.tm_hour = pvt_data->datetime.hour;
	tm.tm_min = pvt_data->datetime.minute;
	tm.tm_sec = pvt_data->datetime.seconds;
	tm.tm_isdst = 0;

	timestamp = mktime(&tm);
	last_latitude = pvt_data->latitude;
	last_longitude = pvt_data->longitude;
}

void write_line(const struct device *i2c_dev, const char *line, uint8_t x, uint8_t y, int erase);

int blocking_send(int fd, uint8_t *buf, uint32_t size, uint32_t flags)
{
	int err;

	do {
		err = send(fd, buf, size, flags);
	} while (err < 0 && errno == EAGAIN);

	return err;
}

static int send_all(int fd, const uint8_t *buf, size_t len)
{
	size_t offset = 0;

	while (offset < len) {
		int sent = blocking_send(fd, (uint8_t *)buf + offset, len - offset, 0);

		if (sent < 0) {
			return sent;
		}
		if (sent == 0) {
			return -EIO;
		}

		offset += (size_t)sent;
	}

	return 0;
}

int blocking_connect(int fd, struct sockaddr *local_addr, socklen_t len)
{
	int err;

	do {
		err = connect(fd, local_addr, len);
	} while (err < 0 && errno == EAGAIN);

	return err;
}

void initial_connexion(){
	LOG_INF("initial_connexion: begin fetching targets");
	startup_stage = STARTUP_STAGE_DNS;

	int err = 0;

	struct sockaddr_in local_addr;
    struct addrinfo *res;
    int send_data_len;
    int num_bytes;
   
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0);
    local_addr.sin_addr.s_addr = 0;


	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
        .ai_next = NULL,
        .ai_addr = NULL,
		.ai_protocol = 0,
	};

    err = getaddrinfo(HTTP_HOST, NULL, &hints, &res);
    LOG_INF("getaddrinfo err: %d", err);
	if (err != 0) {
		startup_stage = STARTUP_STAGE_ERROR;
		return;
	}
	
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);
   

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    LOG_INF("client_fd: %d", client_fd);
	if (client_fd < 0) {
		startup_stage = STARTUP_STAGE_ERROR;
		freeaddrinfo(res);
		return;
	}
    err = bind(client_fd, (struct sockaddr *)&local_addr,sizeof(local_addr));
    LOG_INF("bind err: %d", err);


	startup_stage = STARTUP_STAGE_TCP_CONNECT;
	err = blocking_connect(client_fd, (struct sockaddr *)res->ai_addr,sizeof(struct sockaddr_in));
    LOG_INF("connect err: %d", err);


	if (err >= 0) {

	startup_stage = STARTUP_STAGE_HTTP_REQUEST;


	send_data_len = snprintf(send_buf, 2000,
                                     "GET %s HTTP/1.1\r\n"
                                     "Host: %s\r\n\r\n",
                                     HTTP_PATH, HTTP_HOST);



    do {
        num_bytes =
        blocking_send(client_fd, send_buf, send_data_len, 0);
       
        if (num_bytes < 0) {
            LOG_INF("ret: %d, errno: %s\n", num_bytes, strerror(errno));
        };
		

    } while (num_bytes < 0);

	static char recv_buf[RECV_BUF_SIZE * 2];
	memset(recv_buf, 0, sizeof(recv_buf));
	int tot_num_bytes = 0;
	int recv_offset = 0;
	startup_stage = STARTUP_STAGE_WAIT_SERVER;
	struct timeval recv_timeout = {
		.tv_sec = 5,
		.tv_usec = 0,
	};
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

	do {
		/* TODO: make a proper timeout *
		 * Current solution will just hang 
		 * until remote side closes connection */
		num_bytes = recv(client_fd, recv_buf + recv_offset,
				 sizeof(recv_buf) - recv_offset - 1, 0);
		if (num_bytes > 0) {
			tot_num_bytes += num_bytes;
		}
                LOG_INF("total number of bytes: %d\n", tot_num_bytes);
                LOG_INF("num bytes: %d\n", num_bytes);
		if (num_bytes < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				LOG_INF("recv timeout after %d bytes", tot_num_bytes);
				break;
			}
			LOG_INF("\nrecv errno: %d\n", errno);
			break;
		}
		if (num_bytes == 0) {
			break;
		}
		recv_offset += num_bytes;
		recv_buf[recv_offset] = '\0';
		//LOG_INF("%s\n", recv_buf);
	} while (num_bytes > 0);

	startup_stage = STARTUP_STAGE_PARSE;

    double values[10];                       // Tableau pour stocker les doubles
    size_t count = 0;
	char *body = strstr(recv_buf, "\r\n\r\n");
	if (body != NULL) {
		body += 4;
	} else {
		body = recv_buf;
	}

	LOG_INF("Targets response body starts with: %.80s", body);

	char *scan = body;
	while (*scan != '\0' && count < 10) {
		char *endptr;
		double parsed = strtod(scan, &endptr);

		if (endptr == scan) {
			scan++;
			continue;
		}

		values[count] = parsed;
		count++;
		scan = endptr;
	}

	targets_count = (count / 2);
	if (targets_count > MAX_DISPLAY_TARGETS) {
		LOG_WRN("Parsed %u targets; clamping display to %u", targets_count, MAX_DISPLAY_TARGETS);
		targets_count = MAX_DISPLAY_TARGETS;
	}
	LOG_INF("Parsed %u coordinate pairs from server", targets_count);


    // Afficher les résultats
    for (size_t i = 0; i < targets_count*2; i++) {
		targets[i] = values[i];
    }

	startup_stage = STARTUP_STAGE_READY;

    LOG_INF("Finished. Closing socket");
    err = close(client_fd);
	    LOG_INF("initial_connexion: socket closed");
	    freeaddrinfo(res);
	    LOG_INF("initial_connexion: done");

	} else {
		startup_stage = STARTUP_STAGE_ERROR;
	}
}

void flash_firmware(){

	int err = 0;

	int k = 0;
	while(lte_lc_connect()!= 0) {
		k+=1;
				LOG_INF("Waiting for LTE connection, attempt %d", k);

		if(k >= 5){
			return;
		}
	}

		LOG_INF("Connected to LTE network");

		

	struct flash_img_context fic;

	struct sockaddr_in local_addr;
    struct addrinfo *res;
    int send_data_len;
    int num_bytes;
   
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0);
    local_addr.sin_addr.s_addr = 0;


	struct addrinfo hints = {
		.ai_family = AF_INET,       //1
		.ai_socktype = SOCK_DGRAM,  //2
        .ai_next = NULL,
        .ai_addr = NULL,
        .ai_protocol = 0   //any protocol
	};



	flash_img_init_id(&fic, PM_MCUBOOT_SECONDARY_ID); // Or appropriate slot ID
	boot_erase_img_bank(PM_MCUBOOT_SECONDARY_ID);

	
    err = getaddrinfo(HTTP_HOST, NULL, &hints, &res);	
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    err = bind(client_fd, (struct sockaddr *)&local_addr,sizeof(local_addr));
    err = blocking_connect(client_fd, (struct sockaddr *)res->ai_addr,sizeof(struct sockaddr_in));

	if (err >= 0) {
	
	send_data_len = snprintf(send_buf, 2000,
                                     "GET %s HTTP/1.1\r\n"
                                     "Host: %s\r\n\r\n",
                                     "/firmware", HTTP_HOST);



    do {
        num_bytes = blocking_send(client_fd, send_buf, send_data_len, 0);
       
        if (num_bytes < 0) {
            LOG_INF("ret: %d, errno: %s\n", num_bytes, strerror(errno));
        };
		

    } while (num_bytes < 0);

	char recv_buf[RECV_BUF_SIZE];
	int tot_num_bytes = 0;

	do {
		/* TODO: make a proper timeout *
		 * Current solution will just hang 
		 * until remote side closes connection */
		num_bytes = recv(client_fd, recv_buf, RECV_BUF_SIZE, 0);
		tot_num_bytes += num_bytes;
                LOG_INF("total number of bytes: %d\n", tot_num_bytes);
                LOG_INF("num bytes: %d\n", num_bytes);


		err = flash_img_buffered_write(&fic, recv_buf, num_bytes, false);
        if (err) {
            printk("Erreur écriture flash: %d\n", err);
            break;
        }
		
				
		if (num_bytes < 0) {
			LOG_INF("\nrecv errno: %d\n", errno);
			break;
		}
	} while (num_bytes > 0);

	}


	// Flush final pour s'assurer que tout est écrit
	err = flash_img_buffered_write(&fic, NULL, 0, true);
	if (err) {
		printk("Erreur flush final: %d\n", err);
	}

	LOG_WRN("Firmware image written; reboot disabled during diagnostics");
	return;


}

void send_to_cloud(void)
{
	LOG_INF("send_to_cloud: skipped during LTE diagnostics");
}

static bool menu_can_add_coordinate(void)
{
	return active_point_valid && (last_fix == 0);
}

static const char *menu_item_text(int index)
{
	switch (index) {
	case 0:
		return "EXIT MENU";
	case 1:
		return menu_can_add_coordinate() ? "ADD COORDINATE" : "GPS REACTIVATE";
	case 2:
		return "RELOAD COORDINATES";
	case 3:
		return "OFF";
	default:
		return "";
	}
}

static int send_manual_coordinate(double latitude, double longitude)
{
	int err = 0;
	struct sockaddr_in local_addr;
	struct addrinfo *res = NULL;
	int client_fd = -1;
	char body[96];
	int request_len;
	const struct device *display = i2c_dev;

	k_mutex_lock(&lte_mutex, K_FOREVER);
	show_startup_feedback_screen(display, STARTUP_STAGE_LTE_CONNECT);
	lte_connect();

	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(0);
	local_addr.sin_addr.s_addr = 0;

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_next = NULL,
		.ai_addr = NULL,
		.ai_protocol = 0,
	};

	err = getaddrinfo(HTTP_HOST, NULL, &hints, &res);
	if (err != 0 || res == NULL) {
		LOG_ERR("manual point: getaddrinfo failed: %d", err);
		err = (err != 0) ? err : -ENOENT;
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_disconnect;
	}

	((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);

	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0) {
		err = -errno;
		LOG_ERR("manual point: socket failed: %d", err);
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_free;
	}

	err = bind(client_fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
	if (err < 0) {
		LOG_ERR("manual point: bind failed: %d", err);
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_close;
	}

	show_startup_feedback_screen(display, STARTUP_STAGE_HTTP_REQUEST);
	err = blocking_connect(client_fd, (struct sockaddr *)res->ai_addr, sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("manual point: connect failed: %d", err);
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_close;
	}

	request_len = snprintf(body, sizeof(body), "{\"lat\":%.6f,\"lng\":%.6f}", latitude, longitude);
	if (request_len < 0 || request_len >= (int)sizeof(body)) {
		err = -EMSGSIZE;
		LOG_ERR("manual point: JSON body too large");
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_close;
	}

	request_len = snprintf(send_buf, sizeof(send_buf),
			       "POST %s HTTP/1.1\r\n"
			       "Host: %s\r\n"
			       "Content-Type: application/json\r\n"
			       "Content-Length: %d\r\n"
			       "Connection: close\r\n\r\n"
			       "%s",
			       HTTP_PATH_MANUAL_POINT, HTTP_HOST, (int)strlen(body), body);
	if (request_len < 0 || request_len >= (int)sizeof(send_buf)) {
		err = -EMSGSIZE;
		LOG_ERR("manual point: HTTP request too large");
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_close;
	}

	show_startup_feedback_screen(display, STARTUP_STAGE_WAIT_SERVER);
	err = send_all(client_fd, (const uint8_t *)send_buf, (size_t)request_len);
	if (err < 0) {
		LOG_ERR("manual point: send failed: %d", err);
		show_startup_feedback_screen(display, STARTUP_STAGE_ERROR);
		goto out_close;
	}

	LOG_INF("manual point sent: lat=%.6f lng=%.6f", latitude, longitude);
	show_startup_feedback_screen(display, STARTUP_STAGE_READY);

out_close:
	if (client_fd >= 0) {
		close(client_fd);
	}
out_free:
	if (res != NULL) {
		freeaddrinfo(res);
	}
out_disconnect:
	lte_disconnect();
	k_mutex_unlock(&lte_mutex);

	return err;
}

float sensor_value_to_float(const struct sensor_value *val)
{
    return (float)val->val1 + (val->val2 / 1000000.0f);
}

static void push_tap_event(enum tap_direction dir)
{
	if (off_mode_active) {
		return;
	}

	k_mutex_lock(&tap_mutex, K_FOREVER);
	pending_tap = dir;
	wake_request = true;
	k_mutex_unlock(&tap_mutex);
}

static enum tap_direction pop_tap_event(void)
{
	enum tap_direction dir;

	k_mutex_lock(&tap_mutex, K_FOREVER);
	dir = pending_tap;
	pending_tap = TAP_NONE;
	k_mutex_unlock(&tap_mutex);

	return dir;
}

static bool pop_wake_request(void)
{
	bool requested;

	k_mutex_lock(&tap_mutex, K_FOREVER);
	requested = wake_request;
	wake_request = false;
	k_mutex_unlock(&tap_mutex);

	return requested;
}

static void wait_for_motion_active(void)
{
	k_mutex_lock(&motion_mutex, K_FOREVER);
	while (!motion_active) {
		k_condvar_wait(&motion_cond, &motion_mutex, K_FOREVER);
	}
	k_mutex_unlock(&motion_mutex);
}

static enum tap_direction detect_tap_direction(float x, float y)
{
	float adx = fabsf(x);
	float ady = fabsf(y);

	if (adx < tap_delta_threshold && ady < tap_delta_threshold) {
		return TAP_NONE;
	}

	if (fabsf(adx - ady) < tap_direction_margin) {
		return TAP_NONE;
	}

	if (ady >= adx * tap_axis_dominance) {
		if (y < 0.0f) {
			return top_is_negative_y ? TAP_UP : TAP_DOWN;
		}
		return top_is_negative_y ? TAP_DOWN : TAP_UP;
	}

	if (adx >= ady * tap_axis_dominance) {
		if (x < 0.0f) {
			return left_is_negative_x ? TAP_LEFT : TAP_RIGHT;
		}
		return left_is_negative_x ? TAP_RIGHT : TAP_LEFT;
	}

	return TAP_NONE;
}
 
void accelerometer_thread(void *a, void *b, void *c) {

	const struct device *lis2dh = (const struct device *)a;
	float grav_x = 0.0f;
	float grav_y = 0.0f;
	float grav_z = 0.0f;
	bool filter_initialized = false;
	const float grav_alpha = 0.92f;

    while (1) {
		wait_for_motion_active();

		struct sensor_value accel[3];
		if (sensor_sample_fetch(lis2dh) < 0) {
			LOG_INF("Erreur lors de la récupération des données du capteur\n");
			return;
		}

		if (sensor_channel_get(lis2dh, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
			LOG_INF("Erreur lors dze la récupération des données du capteur\n");
			return;
		}

		float ax = sensor_value_to_float(&accel[0]);
		float ay = sensor_value_to_float(&accel[1]);
		float az = sensor_value_to_float(&accel[2]);

		if (!filter_initialized) {
			grav_x = ax;
			grav_y = ay;
			grav_z = az;
			filter_initialized = true;
		} else {
			grav_x = (grav_alpha * grav_x) + ((1.0f - grav_alpha) * ax);
			grav_y = (grav_alpha * grav_y) + ((1.0f - grav_alpha) * ay);
			grav_z = (grav_alpha * grav_z) + ((1.0f - grav_alpha) * az);
		}

		float lx = ax - grav_x;
		float ly = ay - grav_y;
		float lz = az - grav_z;


		avg += sqrtf(ax*ax+ay*ay+az*az)-9.8;


		valuex = ax*ax;
		valuey = ay*ay;
		valuez = az*az;
		accel_x = ax;
		accel_y = ay;
		accel_z = az;
		linear_x = lx;
		linear_y = ly;
		linear_z = lz;


		k_sleep(K_MSEC(measure_rate));
		wait_for_motion_active();

    }
}

void tap_thread(void *a, void *b, void *c) {
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	bool in_impulse = false;
	int64_t impulse_start_ms = 0;
	float onset_sum_x = 0.0f;
	float onset_sum_y = 0.0f;
	int onset_samples = 0;
	float peak_x = 0.0f;
	float peak_y = 0.0f;
	float peak_plane = 0.0f;
	int64_t next_allowed = 0;

	while (1) {
		wait_for_motion_active();
		float tap_x = linear_x;
		float tap_y = linear_y;
		float plane = sqrtf((tap_x * tap_x) + (tap_y * tap_y));

		int64_t now_ms = k_uptime_get();
		if (!in_impulse) {
			if (now_ms >= next_allowed && plane >= tap_delta_threshold) {
				in_impulse = true;
				impulse_start_ms = now_ms;
				onset_sum_x = tap_x;
				onset_sum_y = tap_y;
				onset_samples = 1;
				peak_x = tap_x;
				peak_y = tap_y;
				peak_plane = plane;
			}
		} else {
			if (plane > peak_plane) {
				peak_plane = plane;
				peak_x = tap_x;
				peak_y = tap_y;
			}

			if ((now_ms - impulse_start_ms) <= tap_direction_window_ms) {
				onset_sum_x += tap_x;
				onset_sum_y += tap_y;
				onset_samples++;
			}

			if (plane <= tap_release_threshold) {
				float onset_x = onset_samples > 0 ? (onset_sum_x / onset_samples) : peak_x;
				float onset_y = onset_samples > 0 ? (onset_sum_y / onset_samples) : peak_y;
				float dir_x = (tap_peak_weight * peak_x) + ((1.0f - tap_peak_weight) * onset_x);
				float dir_y = (tap_peak_weight * peak_y) + ((1.0f - tap_peak_weight) * onset_y);
				enum tap_direction dir = detect_tap_direction(dir_x, dir_y);

			if (dir != TAP_NONE) {
				push_tap_event(dir);
				next_allowed = now_ms + tap_deadtime_ms;
				LOG_INF("Tap dir=%d peak=(%.2f,%.2f) onset=(%.2f,%.2f) amp=%.2f", dir,
					(double)peak_x, (double)peak_y,
					(double)onset_x, (double)onset_y,
					(double)peak_plane);
			}

			in_impulse = false;
			}
		}

		k_sleep(K_MSEC(tap_reset_rate));
		wait_for_motion_active();
	}
}

void gps_thread(void *a, void *b, void *c) {

while(true){

k_mutex_lock(&state.mutex, K_FOREVER);
while(!state.ready) {
	k_condvar_wait(&state.cond, &state.mutex,K_FOREVER);
}
	state.ready = false;
k_mutex_unlock(&state.mutex);


	LOG_INF("GPS");
			int64_t now_ms = k_uptime_get();

			if (gps_capture_mode == GPS_CAPTURE_DORMANT) {
				if (gps_tracking != 0 || gps_using != 0 || gps_unk != 0) {
					gps_tracking = 0;
					gps_using = 0;
					gps_unk = 0;
					gnss_display_dirty = true;
				}
			} else {
				print_satellite_stats(&last_pvt);
			}

			if (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
							int heading_tmp = 0;
							int distance_m = 0;

							gnss_has_fix = true;
							if (gps_capture_mode != GPS_CAPTURE_DORMANT) {
								fix_timestamp = now_ms;
								print_fix_data(&last_pvt);
								last_fix = 0;
								gps_seen_fix_since_reactivate = true;
								gnss_display_dirty = true;
							} else if (last_new_point_ms > 0) {
								last_fix = (uint32_t)((now_ms - last_new_point_ms) / 1000);
							}

							if (gps_capture_mode == GPS_CAPTURE_REACTIVATING) {
								active_point_lat = last_latitude;
								active_point_lon = last_longitude;
								active_point_valid = true;
								last_new_point_ms = now_ms;
								gps_capture_mode = GPS_CAPTURE_ACTIVE;
								LOG_INF("GPS capture reactivated: first point acquired");
							} else if (gps_capture_mode == GPS_CAPTURE_ACTIVE) {
								if (!active_point_valid) {
									active_point_lat = last_latitude;
									active_point_lon = last_longitude;
									active_point_valid = true;
									last_new_point_ms = now_ms;
								} else {
									calcul_cap_distance_int(active_point_lat, active_point_lon,
										last_latitude, last_longitude,
										&heading_tmp, &distance_m);
									if (distance_m >= GPS_NEW_POINT_MIN_DISTANCE_M) {
										active_point_lat = last_latitude;
										active_point_lon = last_longitude;
										last_new_point_ms = now_ms;
									}
								}

							}
							//print_distance_from_reference(&last_pvt);


						} else {
							gnss_has_fix = false;

							last_fix = (uint32_t)((k_uptime_get() - fix_timestamp) / 1000);
							gnss_display_dirty = true;

							if (gps_capture_mode == GPS_CAPTURE_ACTIVE &&
							    gps_seen_fix_since_reactivate &&
							    ((int64_t)last_fix * 1000) >= GPS_DORMANT_TIMEOUT_MS) {
								gps_capture_mode = GPS_CAPTURE_DORMANT;
								gnss_display_dirty = true;
								LOG_INF("GPS capture dormant: signal lost for 2 minutes after at least one fix");
							}

							if ((k_uptime_get_32() - last_gnss_diag) >= 5000U) {
								LOG_WRN("GNSS searching: tracked=%u used=%u unhealthy=%u last_fix=%us antenna=%s",
									gps_tracking,
									gps_using,
									gps_unk,
									last_fix,
									gnss_antenna_mode());
								last_gnss_diag = k_uptime_get_32();
							}

						}

}



}

void exit_menu(const struct device *i2c_dev){

	menu = -1;
	clear_display(i2c_dev);

}

void change_gps(const struct device *i2c_dev){

uint8_t x = 5*6, y = 8;
char message[128];
if(gps_state == 0){
	gps_state = 1;
	snprintf(message, sizeof(message), " ON ");
						}else{
							gps_state = 0;
							snprintf(message, sizeof(message), " OFF");
						}
				for (const char *p = message; *p; p++) {
					dessiner_caractere(i2c_dev, *p, x, y,1);
					x += 6;

				}
}

void change_lte(const struct device *i2c_dev){

uint8_t x = 5*6, y = 16;
char message[128];
if(lte_state == 0){
	lte_state = 1;
	snprintf(message, sizeof(message), " ON ");
						}else{
							lte_state = 0;
							snprintf(message, sizeof(message), " OFF");
						}
				for (const char *p = message; *p; p++) {
					dessiner_caractere(i2c_dev, *p, x, y,1);
					x += 6;

				}
}

void reload_coordinates(const struct device *i2c_dev){
	ARG_UNUSED(i2c_dev);

	menu = -1;
	first = 0;
	gnss_display_dirty = true;
	startup_stage = STARTUP_STAGE_GNSS_WARMUP;

	if (!coordinates_fetch_running) {
		coordinates_fetch_running = true;
		if (k_thread_create(&coordinates_data, coordinates_stack,
				   K_THREAD_STACK_SIZEOF(coordinates_stack),
				   coordinates_thread, NULL, NULL, NULL, 7, 0, K_NO_WAIT) == NULL) {
			coordinates_fetch_running = false;
			startup_stage = STARTUP_STAGE_ERROR;
			LOG_ERR("reload_coordinates: failed to start coordinates thread");
		}
	} else {
		LOG_INF("reload_coordinates: fetch already running");
	}
}


void coordinates_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	coordinates_fetch_running = true;

	/* Give GNSS priority at boot to avoid starving early satellite acquisition. */
	startup_stage = STARTUP_STAGE_GNSS_WARMUP;
	int64_t warmup_deadline = k_uptime_get() + 30000;
	while (!gnss_has_fix && k_uptime_get() < warmup_deadline) {
		k_sleep(K_SECONDS(1));
	}
	LOG_INF("coordinates_thread: GNSS warmup complete (fix=%d), starting LTE fetch", gnss_has_fix ? 1 : 0);

	while (1) {
		k_mutex_lock(&lte_mutex, K_FOREVER);
		startup_stage = STARTUP_STAGE_LTE_CONNECT;
		lte_connect();
		LOG_INF("coordinates_thread: starting coordinates fetch");
		initial_connexion();
		lte_disconnect();
		k_mutex_unlock(&lte_mutex);

		if (targets_count > 0) {
			LOG_INF("coordinates_thread: loaded %d targets", targets_count);
			startup_stage = STARTUP_STAGE_READY;
			break;
		}

		LOG_WRN("coordinates_thread: no targets yet, retrying in 60s");
		startup_stage = STARTUP_STAGE_RETRY;
		k_sleep(K_SECONDS(60));
	}

	coordinates_fetch_running = false;
}
void firmware_update(const struct device *i2c_dev){


if(lte_state == 0){return; }

	clear_display(i2c_dev);


	uint8_t x = 5*6, y = 16;
	

	x=0;
	y=0;

		char line[128];
		snprintf(line,sizeof(line),"Initialization...");
		for (const char *p = line; *p; p++) {
			dessiner_caractere(i2c_dev, *p, x, y,0);
			x += 6;

		}









	flash_firmware();


	menu = -1;
	first = 0;

}

void ssd1306_power_off(const struct device *i2c_dev) {
	k_mutex_lock(&display_mutex, K_FOREVER);
    ssd1306_write_cmd(i2c_dev, 0xAE); // Display OFF
	k_mutex_unlock(&display_mutex);
}

void ssd1306_power_on(const struct device *i2c_dev) {
	k_mutex_lock(&display_mutex, K_FOREVER);
    ssd1306_write_cmd(i2c_dev, 0xAF); // Display ON
	k_mutex_unlock(&display_mutex);
}

static bool font_5x7_pixel_on(char c, int col, int row)
{
	if (c < 32 || c > 127 || col < 0 || col >= 5 || row < 0 || row >= 7) {
		return false;
	}

	return (font_5x7[c - 32][col] & (1U << row)) != 0U;
}

static void draw_refined_scaled_char(const struct device *i2c_dev, char c, uint8_t x, uint8_t y, uint8_t scale)
{
	if (c < 32 || c > 127 || scale == 0) {
		return;
	}

	/* At small sizes (scale 1-2), keep full pixels to preserve thin strokes like '%'. */
	if (scale <= 2U) {
		for (int col = 0; col < 5; col++) {
			for (int row = 0; row < 7; row++) {
				if (!font_5x7_pixel_on(c, col, row)) {
					continue;
				}
				for (uint8_t sx = 0; sx < scale; sx++) {
					for (uint8_t sy = 0; sy < scale; sy++) {
						uint16_t px = x + ((uint16_t)col * scale) + sx;
						uint16_t py = y + ((uint16_t)row * scale) + sy;
						if (px < SSD1306_WIDTH && py < SSD1306_HEIGHT) {
							allumer_pixel(i2c_dev, (uint8_t)px, (uint8_t)py);
						}
					}
				}
			}
		}
		return;
	}

	for (int col = 0; col < 5; col++) {
		for (int row = 0; row < 7; row++) {
			if (!font_5x7_pixel_on(c, col, row)) {
				continue;
			}

			bool north = font_5x7_pixel_on(c, col, row - 1);
			bool south = font_5x7_pixel_on(c, col, row + 1);
			bool west = font_5x7_pixel_on(c, col - 1, row);
			bool east = font_5x7_pixel_on(c, col + 1, row);

			for (uint8_t sx = 0; sx < scale; sx++) {
				for (uint8_t sy = 0; sy < scale; sy++) {
					bool skip = false;

					/* Trim block corners when neighbors are missing to soften curves. */
					if (!north && !west && sx == 0U && sy == 0U) {
						skip = true;
					}
					if (!north && !east && sx == (scale - 1U) && sy == 0U) {
						skip = true;
					}
					if (!south && !west && sx == 0U && sy == (scale - 1U)) {
						skip = true;
					}
					if (!south && !east && sx == (scale - 1U) && sy == (scale - 1U)) {
						skip = true;
					}

					if (!skip) {
						uint16_t px = x + ((uint16_t)col * scale) + sx;
						uint16_t py = y + ((uint16_t)row * scale) + sy;
						if (px < SSD1306_WIDTH && py < SSD1306_HEIGHT) {
							allumer_pixel(i2c_dev, (uint8_t)px, (uint8_t)py);
						}
					}
				}
			}
		}
	}
}

static void draw_hline(const struct device *i2c_dev, int x0, int x1, int y)
{
	if (y < 0 || y >= SSD1306_HEIGHT) {
		return;
	}
	if (x0 > x1) {
		int tmp = x0;
		x0 = x1;
		x1 = tmp;
	}
	if (x1 < 0 || x0 >= SSD1306_WIDTH) {
		return;
	}
	if (x0 < 0) {
		x0 = 0;
	}
	if (x1 >= SSD1306_WIDTH) {
		x1 = SSD1306_WIDTH - 1;
	}
	for (int x = x0; x <= x1; x++) {
		allumer_pixel(i2c_dev, (uint8_t)x, (uint8_t)y);
	}
}

static void draw_vline(const struct device *i2c_dev, int x, int y0, int y1)
{
	if (x < 0 || x >= SSD1306_WIDTH) {
		return;
	}
	if (y0 > y1) {
		int tmp = y0;
		y0 = y1;
		y1 = tmp;
	}
	if (y1 < 0 || y0 >= SSD1306_HEIGHT) {
		return;
	}
	if (y0 < 0) {
		y0 = 0;
	}
	if (y1 >= SSD1306_HEIGHT) {
		y1 = SSD1306_HEIGHT - 1;
	}
	for (int y = y0; y <= y1; y++) {
		allumer_pixel(i2c_dev, (uint8_t)x, (uint8_t)y);
	}
}

static void draw_charge_battery_icon(const struct device *i2c_dev, uint8_t center_x, uint8_t top_y)
{
	const int body_w = 72;
	const int body_h = 28;
	const int body_left = (int)center_x - (body_w / 2);
	const int body_top = (int)top_y;
	const int body_right = body_left + body_w - 1;
	const int body_bottom = body_top + body_h - 1;
	const int nub_w = 8;
	const int nub_h = 12;
	const int nub_left = body_right + 1;
	const int nub_top = body_top + ((body_h - nub_h) / 2);

	/* Outer frame with slight corner rounding. */
	draw_hline(i2c_dev, body_left + 4, body_right - 4, body_top);
	draw_hline(i2c_dev, body_left + 4, body_right - 4, body_top + 1);
	draw_hline(i2c_dev, body_left + 4, body_right - 4, body_bottom - 1);
	draw_hline(i2c_dev, body_left + 4, body_right - 4, body_bottom);
	draw_vline(i2c_dev, body_left, body_top + 4, body_bottom - 4);
	draw_vline(i2c_dev, body_left + 1, body_top + 4, body_bottom - 4);
	draw_vline(i2c_dev, body_right - 1, body_top + 4, body_bottom - 4);
	draw_vline(i2c_dev, body_right, body_top + 4, body_bottom - 4);

	/* Rounded corner pixels. */
	allumer_pixel(i2c_dev, (uint8_t)(body_left + 2), (uint8_t)(body_top + 2));
	allumer_pixel(i2c_dev, (uint8_t)(body_left + 3), (uint8_t)(body_top + 1));
	allumer_pixel(i2c_dev, (uint8_t)(body_right - 2), (uint8_t)(body_top + 2));
	allumer_pixel(i2c_dev, (uint8_t)(body_right - 3), (uint8_t)(body_top + 1));
	allumer_pixel(i2c_dev, (uint8_t)(body_left + 2), (uint8_t)(body_bottom - 2));
	allumer_pixel(i2c_dev, (uint8_t)(body_left + 3), (uint8_t)(body_bottom - 1));
	allumer_pixel(i2c_dev, (uint8_t)(body_right - 2), (uint8_t)(body_bottom - 2));
	allumer_pixel(i2c_dev, (uint8_t)(body_right - 3), (uint8_t)(body_bottom - 1));

	/* Terminal nub on the right side. */
	draw_hline(i2c_dev, nub_left, nub_left + nub_w - 1, nub_top);
	draw_hline(i2c_dev, nub_left, nub_left + nub_w - 1, nub_top + nub_h - 1);
	draw_vline(i2c_dev, nub_left, nub_top, nub_top + nub_h - 1);
	draw_vline(i2c_dev, nub_left + nub_w - 1, nub_top, nub_top + nub_h - 1);

	/* Filled lightning bolt, centered in battery body. */
	static const uint32_t bolt_rows[14] = {
		0x000700, 0x000F80, 0x001FC0, 0x00FFF0,
		0x07FFE0, 0x03FF00, 0x007FC0, 0x003FE0,
		0x01FFF8, 0x007FFC, 0x001FF8, 0x000FE0,
		0x0007C0, 0x000300,
	};
	const int bolt_w = 20;
	const int bolt_h = (int)ARRAY_SIZE(bolt_rows);
	const int bolt_left = (int)center_x - (bolt_w / 2);
	const int bolt_top = body_top + ((body_h - bolt_h) / 2);

	for (int row = 0; row < bolt_h; row++) {
		uint32_t bits = bolt_rows[row];
		for (int col = 0; col < bolt_w; col++) {
			if (bits & (1U << (bolt_w - 1 - col))) {
				int px = bolt_left + col;
				int py = bolt_top + row;
				if (px >= 0 && py >= 0 && px < SSD1306_WIDTH && py < SSD1306_HEIGHT) {
					allumer_pixel(i2c_dev, (uint8_t)px, (uint8_t)py);
				}
			}
		}
	}
}

static void draw_charge_screen(const struct device *i2c_dev, uint8_t batt_pct)
{
	char pct_line[8];
	const uint8_t text_scale = 2;
	const uint8_t char_w = 5 * text_scale;
	const uint8_t char_gap = 2;
	const uint8_t text_h = 7 * text_scale;
	int text_w;
	int text_x;
	int text_y;

	snprintf(pct_line, sizeof(pct_line), "%u%%", batt_pct);
	text_w = ((int)strlen(pct_line) * char_w) + (((int)strlen(pct_line) - 1) * char_gap);
	text_x = (SSD1306_WIDTH - text_w) / 2;
	text_y = (SSD1306_HEIGHT - text_h) / 2;
	if (text_x < 0) {
		text_x = 0;
	}
	if (text_y < 0) {
		text_y = 0;
	}

	clear_display(i2c_dev);

	for (size_t i = 0; i < strlen(pct_line); i++) {
		draw_refined_scaled_char(i2c_dev, pct_line[i], (uint8_t)text_x, (uint8_t)text_y, text_scale);
		text_x += char_w + char_gap;
	}
}

void off(const struct device *i2c_dev){
	int err;
	int trigger_err;
	int modem_err;
	struct pm_state_info soft_off_state = {
		.state = PM_STATE_SOFT_OFF,
		.substate_id = 0,
		.min_residency_us = 0,
	};

	LOG_WRN("Entering deep sleep mode");
	menu = -1;
	first = 0;
	gps_capture_mode = GPS_CAPTURE_DORMANT;
	gps_seen_fix_since_reactivate = false;
	active_point_valid = false;
	last_new_point_ms = 0;
	gnss_display_dirty = true;
	last_tracked = 0xFF;
	last_in_fix = 0xFF;
	last_unhealthy = 0xFF;

	k_mutex_lock(&state.mutex, K_FOREVER);
	state.ready = false;
	k_mutex_unlock(&state.mutex);
	off_mode_active = true;

	if (nrf_modem_gnss_stop() != 0) {
		LOG_WRN("Failed to stop GNSS before deep sleep");
	}

	k_mutex_lock(&lte_mutex, K_FOREVER);
	lte_disconnect();
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_POWER_OFF);
	if (err != 0) {
		LOG_WRN("Failed to set modem power off mode: %d", err);
	}
	modem_err = nrf_modem_lib_shutdown();
	if (modem_err != 0) {
		LOG_WRN("Failed to shutdown modem library in OFF mode: %d", modem_err);
	}
	k_mutex_unlock(&lte_mutex);
	lte_state = 0;

	/* Clear the tap event that likely triggered OFF so we don't wake immediately. */
	pop_tap_event();
	pop_wake_request();
	k_sem_reset(&wake_sem);
	off_wake_reset_motion();
	/* Give the board time to settle so the same OFF tap does not instantly wake it back up. */
	off_wake_armed_after_ms = k_uptime_get() + 1500;
	{
		struct sensor_value odr = {
			.val1 = off_wake_odr_hz,
			.val2 = 0,
		};
		int odr_err = sensor_attr_set(accel_sensor, SENSOR_CHAN_ACCEL_XYZ,
					      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
		if (odr_err != 0) {
			LOG_WRN("Unable to lower accelerometer ODR for OFF wake: %d", odr_err);
		}
	}

	(void)wake_trigger;
	(void)wake_trigger_handler;
	trigger_err = sensor_trigger_set(accel_sensor, &wake_trigger, wake_trigger_handler);
	if (trigger_err != 0) {
		LOG_WRN("Unable to arm accelerometer wake trigger: %d", trigger_err);
	}

	/* Ensure IRQ pin has wake sense enabled for System OFF wake-up. */
	nrf_gpio_cfg_input(NRF_DT_GPIOS_TO_PSEL(DT_NODELABEL(lis2dh), irq_gpios),
			  NRF_GPIO_PIN_NOPULL);
	nrf_gpio_cfg_sense_set(NRF_DT_GPIOS_TO_PSEL(DT_NODELABEL(lis2dh), irq_gpios),
			      NRF_GPIO_PIN_SENSE_HIGH);

	ssd1306_power_off(i2c_dev);
	tap_accept_after_ms = k_uptime_get() + (24LL * 60LL * 60LL * 1000LL);

	if (off_use_system_off) {
		LOG_WRN("OFF ECO: entering System OFF (wake on LIS2DH motion IRQ)");
		pm_state_force(0u, &soft_off_state);
		k_sleep(K_FOREVER);

		/* If we ever return here, force a clean restart as a safety fallback. */
		LOG_ERR("System OFF failed; forcing cold reboot");
		sys_reboot(SYS_REBOOT_COLD);
	}

	LOG_WRN("OFF SAFE: using motion polling wake strategy");
	bool charge_screen_on = false;
	uint8_t charge_displayed_pct = 0xFF;
	int charge_prev_mv = -1;
	int charge_rise_score = 0;
	int64_t next_charge_poll_ms = 0;
	int wait_ms = off_wake_idle_wait_ms;
	while (1) {
		int64_t now_ms = k_uptime_get();

		if (now_ms >= next_charge_poll_ms) {
			uint8_t batt_pct = 0;
			int batt_mv = 0;
			bool likely_external_power = false;

			next_charge_poll_ms = now_ms + 5000;
			if (board_battery_percent_get(&batt_pct, &batt_mv) == 0) {
				/* Heuristic: charging input is likely present if VBAT rises repeatedly
				 * between OFF samples.
				 */
				if (charge_prev_mv >= 0) {
					if (batt_mv >= charge_prev_mv + 8) {
						if (charge_rise_score < 3) {
							charge_rise_score++;
						}
					} else if (batt_mv + 4 < charge_prev_mv) {
						if (charge_rise_score > 0) {
							charge_rise_score--;
						}
					}
				}
				charge_prev_mv = batt_mv;
				if (charge_rise_score >= 2) {
					likely_external_power = true;
				}

				if (likely_external_power) {
					if (!charge_screen_on) {
						ssd1306_power_on(i2c_dev);
						clear_display(i2c_dev);
						charge_screen_on = true;
						charge_displayed_pct = 0xFF;
					}
					if (charge_displayed_pct != batt_pct) {
						draw_charge_screen(i2c_dev, batt_pct);
						charge_displayed_pct = batt_pct;
					}
				} else if (charge_screen_on) {
					clear_display(i2c_dev);
					ssd1306_power_off(i2c_dev);
					charge_screen_on = false;
					charge_displayed_pct = 0xFF;
				}
			}
		}

		if (k_sem_take(&wake_sem, K_MSEC(wait_ms)) == 0) {
			break;
		}

		if (off_wake_poll_motion(k_uptime_get())) {
			break;
		}

		wait_ms = off_wake_probe_active ? off_wake_fast_wait_ms : off_wake_idle_wait_ms;
	}

	(void)sensor_trigger_set(accel_sensor, &wake_trigger, NULL);
	LOG_WRN("OFF SAFE wake detected: rebooting for full cold boot session");
	sys_reboot(SYS_REBOOT_COLD);

}

void write_line(const struct device *i2c_dev, const char *line, uint8_t x, uint8_t y, int erase) {
	if (y > (SSD1306_HEIGHT - 7)) {
		return;
	}

	int cursor = x;
	for (const char *p = line; *p; p++) {
		if (cursor > (SSD1306_WIDTH - 5)) {
			break;
		}

		dessiner_caractere(i2c_dev, *p, (uint8_t)cursor, y, erase);
		cursor += 6;
	}
}

static void draw_menu_screen(const struct device *i2c_dev)
{
	uint8_t y = 0;
	char message[128];

	clear_display(i2c_dev);

	for (int i = 0; i < nb_menu; i++) {
		snprintf(message, sizeof(message), "%s", menu_item_text(i));
		write_line(i2c_dev, message, 6, y, 0);

		y += 8;
	}

	write_line(i2c_dev, ">", 0, menu * 8, 0);
}

static void open_menu(const struct device *i2c_dev)
{
	menu = 0;
	menu_confirm_after_ms = k_uptime_get() + tap_menu_confirm_guard_ms;
	menu_wake_reset_motion();
	draw_menu_screen(i2c_dev);
}

static void move_menu_cursor(const struct device *i2c_dev, int delta)
{
	char marker[2] = " ";

	write_line(i2c_dev, marker, 0, menu * 8, 1);
	menu = (menu + delta + nb_menu) % nb_menu;
	menu_confirm_after_ms = k_uptime_get() + tap_menu_confirm_guard_ms;
	marker[0] = '>';
	write_line(i2c_dev, marker, 0, menu * 8, 0);
}

static void execute_menu_selection(const struct device *i2c_dev)
{
	LOG_INF("MENU CHOISI: %s", menu_item_text(menu));

	switch (menu) {
	case 0:
		exit_menu(i2c_dev);
		first = 0;
		last_tracked = 0;
		last_fix = 0;
		last_in_fix = 0;
		last_unhealthy = 0;
		last_latitude = 0;
		last_longitude = 0;
		break;
	case 1:
		if (menu_can_add_coordinate()) {
			int upload_err = send_manual_coordinate(active_point_lat, active_point_lon);
			if (upload_err == 0) {
				LOG_INF("Manual coordinate uploaded successfully");
				exit_menu(i2c_dev);
				first = 0;
				show_startup_feedback_screen(i2c_dev, STARTUP_STAGE_READY);
				reload_coordinates(i2c_dev);
			} else {
				LOG_WRN("Manual coordinate upload failed: %d", upload_err);
				exit_menu(i2c_dev);
				first = 0;
			}
			break;
		}

		gps_capture_mode = GPS_CAPTURE_ACTIVE;
		last_new_point_ms = k_uptime_get();
		gps_seen_fix_since_reactivate = false;
		if (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
			print_fix_data(&last_pvt);
			active_point_lat = last_latitude;
			active_point_lon = last_longitude;
			active_point_valid = true;
			gps_seen_fix_since_reactivate = true;
			LOG_INF("GPS capture mode set to ACTIVE (instant fix)");
		} else {
			active_point_valid = false;
			LOG_INF("GPS capture mode set to ACTIVE (waiting for first fix)");
		}
		/* Always refresh signal counters immediately when exiting dormant. */
		print_satellite_stats(&last_pvt);
		last_tracked = 0xFF;
		last_in_fix = 0xFF;
		last_unhealthy = 0xFF;
		gnss_display_dirty = true;
		menu = -1;
		first = 0;
		break;
	case 2:
		reload_coordinates(i2c_dev);
		first = 0;
		break;
	case 3:
		off(i2c_dev);
		break;
	default:
		break;
	}

	if (menu >= 0) {
		draw_menu_screen(i2c_dev);
	}
}

int main(void)
{

	k_mutex_init(&display_mutex);
	k_mutex_init(&lte_mutex);
	k_mutex_init(&tap_mutex);
	k_mutex_init(&motion_mutex);
	k_mutex_init(&state.mutex);
	k_condvar_init(&state.cond);
	k_condvar_init(&motion_cond);
	k_sem_init(&wake_sem, 0, 1);
	state.ready = false;
	uint8_t x = 0, y = 0;
	char message[128];
	int err;
	bool home_screen_cleared = false;
	uint8_t battery_pct = 0;
	uint8_t displayed_battery_pct = 0xFF;
	uint8_t displayed_startup_stage = 0xFF;
	uint8_t displayed_startup_visible = 0xFF;
	bool battery_valid = false;
	int64_t next_battery_poll_ms = 0;
	tap_accept_after_ms = k_uptime_get() + tap_boot_guard_ms;
	menu_confirm_after_ms = 0;

	//const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device not ready");
		while (true) {
			k_sleep(K_SECONDS(1));
		}
	}
	LOG_INF("I2C device is ready");
	LOG_INF("GNSS antenna mode: %s", gnss_antenna_mode());

	ssd1306_init(i2c_dev);
	draw_startup_logo(i2c_dev);
	k_sleep(K_MSEC(logo_preview_ms));

	clear_display(i2c_dev);
	snprintf(message, sizeof(message), "T: %2d U: %2d UN: %d", 0, 0, 0);
	write_line(i2c_dev, message, 0, 0, 0);
	snprintf(message, sizeof(message), "LAST FIX: %d", 0);
	write_line(i2c_dev, message, 0, 8, 0);
	write_line(i2c_dev, "--%", 18 * 6, 8, 0);
	snprintf(message, sizeof(message), "                ");
	write_line(i2c_dev, message, 0, 32, 0);
	home_screen_cleared = true;
	first = 0;

	accel_sensor = DEVICE_DT_GET_ONE(st_lis2dh);
	if (!device_is_ready(accel_sensor)) {
        LOG_ERR("Erreur : LIS2DH non prêt\n");
		while (true) {
			k_sleep(K_SECONDS(1));
		}
    }

	{
		struct sensor_value odr = {
			.val1 = active_odr_hz,
			.val2 = 0,
		};
		int odr_err = sensor_attr_set(accel_sensor, SENSOR_CHAN_ACCEL_XYZ,
					      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
		if (odr_err != 0) {
			LOG_WRN("Unable to set accelerometer sampling frequency: %d", odr_err);
		}
	}

	k_thread_create(&accelerometer_data, accelerometer_stack, 1024,accelerometer_thread, (void *)accel_sensor, NULL, NULL,5, 0, K_NO_WAIT);
	k_thread_create(&tap_data, tap_stack, 1024,tap_thread, NULL, NULL, NULL,5, 0, K_NO_WAIT);
	k_thread_create(&gps_data, gps_stack, 1024,gps_thread, NULL, NULL, NULL,5, 0, K_NO_WAIT);


	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Modem library initialization failed, error: %d", err);
		while (true) {
			k_sleep(K_SECONDS(1));
		}
	}

	log_modem_state("After modem init");

	/* Initialize reference coordinates (if used). */
	if (sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE) > 1 &&
	    sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE) > 1) {
		ref_used = true;
		ref_latitude = atof(CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE);
		ref_longitude = atof(CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE);
	}

	if (modem_init() != 0) {
		LOG_ERR("Failed to initialize modem");
		while (true) {
			k_sleep(K_SECONDS(1));
		}
	}

	if (sample_init() != 0) {
		LOG_ERR("Failed to initialize sample");
		while (true) {
			k_sleep(K_SECONDS(1));
		}
	}

	if (gnss_init_and_start() != 0) {
		LOG_ERR("Failed to initialize and start GNSS");
		while (true) {
			k_sleep(K_SECONDS(1));
		}
	}

	/* Explicit runtime init: always begin with a fresh GPS reactivation cycle at boot. */
	gps_capture_mode = GPS_CAPTURE_REACTIVATING;
	active_point_valid = false;
	last_new_point_ms = 0;
	gps_seen_fix_since_reactivate = false;
	gnss_display_dirty = true;
	last_tracked = 0xFF;
	last_in_fix = 0xFF;
	last_unhealthy = 0xFF;

	LOG_INF("main: GNSS started, scheduling coordinates thread");
	k_thread_create(&coordinates_data, coordinates_stack, K_THREAD_STACK_SIZEOF(coordinates_stack),
			coordinates_thread, NULL, NULL, NULL, 7, 0, K_NO_WAIT);

	LOG_INF("main: starting initial GNSS phase");
	LOG_INF("GPS capture mode at boot: REACTIVATING");
	fix_timestamp = k_uptime_get();

	while(true){

		int64_t now_ms = k_uptime_get();
		if (now_ms >= next_battery_poll_ms) {
			int battery_mv = 0;

			next_battery_poll_ms = now_ms + 30000;
			if (board_battery_percent_get(&battery_pct, &battery_mv) == 0) {
				battery_valid = true;
				LOG_DBG("Battery(board ADC): %d mV => %u%%", battery_mv, battery_pct);
			} else {
				battery_valid = false;
			}
		}

	if(menu == -1){
		int visible_targets = targets_count;
		int layout_rows;
		bool show_startup_stage = (startup_stage != STARTUP_STAGE_READY) || (targets_count <= 0);
		uint8_t targets_base_y = show_startup_stage ? 32 : 24;

		if (visible_targets > MAX_DISPLAY_TARGETS) {
			visible_targets = MAX_DISPLAY_TARGETS;
		}
		if (visible_targets < 0) {
			visible_targets = 0;
		}
		layout_rows = show_startup_stage ? 0 : visible_targets;

		if (!home_screen_cleared) {
			clear_display(i2c_dev);
			home_screen_cleared = true;
		}

		if(first == 0){
					
			clear_display(i2c_dev);
			snprintf(message, sizeof(message), "T: %2d U: %2d UN: %d", 0, 0, 0);
			write_line(i2c_dev, message, 0, 0, 0);

			snprintf(message, sizeof(message), "LAST FIX: %d",0);
			write_line(i2c_dev, message, 0, 8, 0);
			write_line(i2c_dev, "--%", 18 * 6, 8, 0);
			write_line(i2c_dev, "                     ", 0, 24, 0);
			displayed_battery_pct = 0xFF;
			draw_target_rows_layout(i2c_dev, layout_rows, targets_base_y);


		}

		if (displayed_targets_count != layout_rows ||
		    displayed_startup_visible != (show_startup_stage ? 1 : 0)) {
			draw_target_rows_layout(i2c_dev, layout_rows, targets_base_y);
			gnss_display_dirty = true;
		}

		if (show_startup_stage) {
			if (displayed_startup_stage != startup_stage || displayed_startup_visible != 1) {
				snprintf(message, sizeof(message), "%s", startup_stage_text(startup_stage));
				write_line(i2c_dev, "                     ", 0, 24, 1);
				write_line(i2c_dev, message, 0, 24, 1);
				displayed_startup_stage = startup_stage;
				displayed_startup_visible = 1;
			}
		} else if (displayed_startup_visible != 0) {
			/* Do not blank y=24 here: this row is reused by target line #1 in READY state. */
			displayed_startup_visible = 0;
			displayed_startup_stage = startup_stage;
			gnss_display_dirty = true;
		}

		if(last_fix != ref_lastfix){
			ref_lastfix = last_fix;
			y=8;
			x=10*6;

			snprintf(message, sizeof(message), "%d   ",last_fix);
			write_line(i2c_dev, message, x,y,1);

		}

		if (!battery_valid) {
			if (displayed_battery_pct != 0xFE) {
				write_line(i2c_dev, "--%", 18 * 6, 8, 1);
				displayed_battery_pct = 0xFE;
			}
		} else if (displayed_battery_pct != battery_pct) {
			snprintf(message, sizeof(message), "%2u%%", battery_pct);
			write_line(i2c_dev, message, 18 * 6, 8, 1);
			displayed_battery_pct = battery_pct;
		}

		if(gps_state == 1 || first == 0){

			first = 1;
			y = 0;

			if (gps_capture_mode == GPS_CAPTURE_DORMANT) {
				x = 3 * 6;
				write_line(i2c_dev, "--", x, y, 1);
				x = 9 * 6;
				write_line(i2c_dev, "--", x, y, 1);
				x = 16 * 6;
				write_line(i2c_dev, "--", x, y, 1);

				/* Force refresh of numeric counters when leaving dormant mode. */
				last_tracked = 0xFF;
				last_in_fix = 0xFF;
				last_unhealthy = 0xFF;
			} else {

			if(last_tracked != gps_tracking){


				x=3*6;

				snprintf(message, sizeof(message), "%2d", gps_tracking);
				write_line(i2c_dev, message,x,y,1);

				last_tracked = gps_tracking;

			}

			if(last_in_fix != gps_using){

				x=9*6;
				snprintf(message, sizeof(message), "%2d", gps_using);

				write_line(i2c_dev,message,x,y,1);


				last_in_fix = gps_using;
			}

			if(last_unhealthy != gps_unk){

			x=16*6;
			snprintf(message, sizeof(message), "%2d", gps_unk);

			write_line(i2c_dev,message,x,y,1);

			last_unhealthy = gps_unk;

			}
			}

			if(lte_state == 1 && gps_using >= 4 && gps_capture_mode == GPS_CAPTURE_ACTIVE){

			if(position >= 5*20){

				position = 0;

				k_mutex_lock(&lte_mutex, K_SECONDS(60));

				send_to_cloud();

				k_mutex_unlock(&lte_mutex);

			}

			if((position+20) % 20 == 0){
				ts[position/20] = timestamp;
				lat[position/20] = active_point_valid ? (float)active_point_lat : (float)last_latitude;
				lng[position/20] = active_point_valid ? (float)active_point_lon : (float)last_longitude;
			}

			position++;



			}

		double calc_latitude = active_point_valid ? active_point_lat : last_latitude;
		double calc_longitude = active_point_valid ? active_point_lon : last_longitude;

		if(!show_startup_stage &&
		   (gnss_display_dirty || fabs(ref_latitude2 - calc_latitude) > 0.00001 || fabs(ref_longitude2 - calc_longitude) > 0.00001)){

			ref_latitude2 = calc_latitude;
			ref_longitude2 = calc_longitude;
			gnss_display_dirty = false;

x = 0;
y = targets_base_y;

for(int i=0;i<visible_targets;i++){

	if (y > (SSD1306_HEIGHT - 8)) {
		break;
	}

	int heading, distance;

	calcul_cap_distance_int(calc_latitude, calc_longitude, targets[i*2], targets[i*2+1], &heading, &distance);

	if(distance > 10000){
		distance = 9999;
	}


	x = 3*6;
	if (displayed_heading[i] != heading) {
		snprintf(message, sizeof(message), "%3d", heading);
		write_line(i2c_dev, message, x, y, 1);
		displayed_heading[i] = heading;
	}

	x = 14*6;
	if (displayed_distance[i] != distance) {
		snprintf(message, sizeof(message), "%4d", distance);
		write_line(i2c_dev, message, x, y, 1);
		displayed_distance[i] = distance;
	}

	y+= 8;
	x = 0;
}

}

		}

		}

		if (menu == -1 && k_uptime_get() >= tap_accept_after_ms) {
			if (menu_wake_poll_motion(k_uptime_get())) {
				LOG_INF("GOING IN THE MENU");
				open_menu(i2c_dev);
				k_sleep(K_MSEC(100));
				continue;
			}
		} else {
			menu_wake_reset_motion();
		}

		enum tap_direction tap = pop_tap_event();
		if (tap != TAP_NONE) {
			if (k_uptime_get() < tap_accept_after_ms) {
				k_sleep(K_MSEC(100));
				continue;
			}

			if (menu != -1) {
				switch (tap) {
				case TAP_UP:
					/* Haut physique: descendre le curseur dans la liste. */
					move_menu_cursor(i2c_dev, +1);
					break;
				case TAP_DOWN:
					/* Bas physique: remonter le curseur. */
					move_menu_cursor(i2c_dev, -1);
					break;
				case TAP_LEFT:
					/* Gauche physique: confirmer/entrer. */
					if (k_uptime_get() >= menu_confirm_after_ms) {
						execute_menu_selection(i2c_dev);
					}
					break;
				case TAP_RIGHT:
					/* Droite physique: inactif pour l'instant. */
					break;
				default:
					break;
				}
			}
		}

		k_sleep(K_MSEC(100));	

	}

	return 0;
}
