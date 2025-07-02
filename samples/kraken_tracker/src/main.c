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
#include <zephyr/sys/reboot.h>



LOG_MODULE_REGISTER(gnss_sample, CONFIG_GNSS_SAMPLE_LOG_LEVEL);



#define SSD1306_I2C_ADDR 0x3D // Adresse I2C typique pour SSD1306

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


static uint8_t framebuffer[128][8] = {0}; // 128 colonnes x 4 pages (pour 128x32)



void allumer_pixel(const struct device *i2c_dev, uint8_t x, uint8_t y) {
    if (x >= 128 || y >= 64) return;
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
}

void eteindre_pixel(const struct device *i2c_dev, uint8_t x, uint8_t y) {
    //if (x >= 128 || y >= 64) return;
    //extern uint8_t framebuffer[128][4];
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
}

static void ssd1306_init(const struct device *i2c_dev) {
    ssd1306_write_cmd(i2c_dev, 0xAE); // Display OFF
    ssd1306_write_cmd(i2c_dev, 0xD5); // Set display clock divide ratio/oscillator freq
    ssd1306_write_cmd(i2c_dev, 0x80); // Default setting for display clock divide ratio
    ssd1306_write_cmd(i2c_dev, 0xA8); // Set multiplex ratio
    ssd1306_write_cmd(i2c_dev, 0x3F); // 1/32 duty (0x1F pour 32 lignes)
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
    ssd1306_write_cmd(i2c_dev, 0x12); // 0x02 pour 128x32 (voir datasheet)
    ssd1306_write_cmd(i2c_dev, 0x81); // Set contrast control
    ssd1306_write_cmd(i2c_dev, 0xCF);
    ssd1306_write_cmd(i2c_dev, 0xD9); // Set pre-charge period
    ssd1306_write_cmd(i2c_dev, 0xF1);
    ssd1306_write_cmd(i2c_dev, 0xDB); // Set VCOMH deselect level
    ssd1306_write_cmd(i2c_dev, 0x40);
    ssd1306_write_cmd(i2c_dev, 0xA4); // Entire display ON (resume)
    ssd1306_write_cmd(i2c_dev, 0xA6); // Set normal display (not inverted)
    ssd1306_write_cmd(i2c_dev, 0xAF); // Display ON
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


	if(erase){
    // Effacer la zone 5x7 avant de dessiner le caractère
    for (uint8_t col = 0; col < 5; col++) {
        for (uint8_t row = 0; row < 7; row++) {
            eteindre_pixel(i2c_dev, x + col, y + row);
        }
    }}



    // Affiche tout caractère ASCII 32..127 (inclus)
    if (c < 32 || c > 127) return;
    const uint8_t *bitmap = font_5x7[c - 32];
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = bitmap[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                allumer_pixel(i2c_dev, x + col, y + row);
            }
        }
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

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) || defined(CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST)
static struct k_work_q gnss_work_q;

#define GNSS_WORKQ_THREAD_STACK_SIZE 2304
#define GNSS_WORKQ_THREAD_PRIORITY   5

K_THREAD_STACK_DEFINE(gnss_workq_stack_area, GNSS_WORKQ_THREAD_STACK_SIZE);
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE || CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST */

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)
#include "assistance.h"

static struct nrf_modem_gnss_agps_data_frame last_agps;
static struct k_work agps_data_get_work;
static volatile bool requesting_assistance;
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE */


static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static uint64_t fix_timestamp;

/* Reference position. */
static bool ref_used;
static double ref_latitude;
static double ref_longitude;

K_MSGQ_DEFINE(nmea_queue, sizeof(struct nrf_modem_gnss_nmea_data_frame *), 10, 4);
static K_SEM_DEFINE(pvt_data_sem, 0, 1);
static K_SEM_DEFINE(time_sem, 0, 1);

BUILD_ASSERT(IS_ENABLED(CONFIG_LTE_NETWORK_MODE_LTE_M_GPS) ||
	     IS_ENABLED(CONFIG_LTE_NETWORK_MODE_NBIOT_GPS) ||
	     IS_ENABLED(CONFIG_LTE_NETWORK_MODE_LTE_M_NBIOT_GPS),
	     "CONFIG_LTE_NETWORK_MODE_LTE_M_GPS, "
	     "CONFIG_LTE_NETWORK_MODE_NBIOT_GPS or "
	     "CONFIG_LTE_NETWORK_MODE_LTE_M_NBIOT_GPS must be enabled");

BUILD_ASSERT((sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE) == 1 &&
	      sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE) == 1) ||
	     (sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE) > 1 &&
	      sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE) > 1),
	     "CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE and "
	     "CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE must be both either set or empty");



struct shared_state {
	struct k_mutex mutex;
	struct k_condvar cond;
	bool ready;
};

struct shared_state state;



static void gnss_event_handler(int event)
{
	int retval;
	struct nrf_modem_gnss_nmea_data_frame *nmea_data;

	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
		if (retval == 0) {
			k_sem_give(&pvt_data_sem);
		}
		break;


	case NRF_MODEM_GNSS_EVT_NMEA:
		nmea_data = k_malloc(sizeof(struct nrf_modem_gnss_nmea_data_frame));
		if (nmea_data == NULL) {
			LOG_ERR("Failed to allocate memory for NMEA");
			break;
		}

		retval = nrf_modem_gnss_read(nmea_data,
					     sizeof(struct nrf_modem_gnss_nmea_data_frame),
					     NRF_MODEM_GNSS_DATA_NMEA);
		if (retval == 0) {
			retval = k_msgq_put(&nmea_queue, &nmea_data, K_NO_WAIT);
		}

		if (retval != 0) {
			k_free(nmea_data);
		}else{
			LOG_ERR("NMEA NO FREE");
		}
		break;

	case NRF_MODEM_GNSS_EVT_AGPS_REQ:
#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)
		retval = nrf_modem_gnss_read(&last_agps,
					     sizeof(last_agps),
					     NRF_MODEM_GNSS_DATA_AGPS_REQ);
		if (retval == 0) {
			k_work_submit_to_queue(&gnss_work_q, &agps_data_get_work);
		}
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE */
		break;

	default:
		break;
	}
}

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)
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

void lte_connect(void)
{
	int err;

	LOG_INF("Connecting to LTE network");

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
	lte_connect();
#endif /* CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

	err = assistance_request(&last_agps);
	if (err) {
		LOG_ERR("Failed to request assistance data");
	}

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
	lte_disconnect();
#endif /* CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND */

	requesting_assistance = false;
}
#endif /* !CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE */




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

#if defined(CONFIG_GNSS_SAMPLE_LTE_ON_DEMAND)
	lte_lc_register_handler(lte_lc_event_handler);
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

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE) || defined(CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST)
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

#if !defined(CONFIG_GNSS_SAMPLE_ASSISTANCE_NONE)
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

	/* Enable all supported NMEA messages. */
	uint16_t nmea_mask = NRF_MODEM_GNSS_NMEA_RMC_MASK |
			     NRF_MODEM_GNSS_NMEA_GGA_MASK |
			     NRF_MODEM_GNSS_NMEA_GLL_MASK |
			     NRF_MODEM_GNSS_NMEA_GSA_MASK |
			     NRF_MODEM_GNSS_NMEA_GSV_MASK;

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
#define HTTP_PORT 8545
#define MAX_MTU_SIZE     2000
#define RECV_BUF_SIZE    2048
#define SEND_BUF_SIZE    MAX_MTU_SIZE

#define JSON_TEMPLATE "%ld,%.9f,%.9f"
#define JSON_TEMPLATE_LONG "%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f"

char send_buf[2000];

bool gps_state = 1;
bool lte_state = 0;

struct k_mutex lte_mutex;
time_t timestamp;
volatile float avg = 0;
float thresold = 19;
int reset_threshold = 20;

int refresh_rate = 1000;
int confirmation_rate = 20;

int menu = -1;
int tap_state = 0;
int confirmation_state = 0;

const char *menu_str[] = {
    "EXIT MENU",
	"GPS:",
    "LTE:",
	"RELOAD COORDINATES",
	"FW UPDATE",
	"OFF"
};
int nb_menu = 6;

K_THREAD_STACK_DEFINE(accelerometer_stack, 1024);
K_THREAD_STACK_DEFINE(tap_stack, 1024);
K_THREAD_STACK_DEFINE(gps_stack, 1024);

struct k_thread accelerometer_data;
struct k_thread tap_data;
struct k_thread gps_data;

int measure_rate = 10;

float valuex = 0;
float valuey = 0;
float valuez = 0;

int tap_reset_rate = 10;
int tap_threshold = 35;
int tap_kickback_waittime = 100;
int confirmation_timeout = 30;
int multitap = 0;
int bypass = 0;
uint32_t last_fix = 0;
int gps_rate = 500;
volatile int first = 0;
uint32_t ref_lastfix = 0;





void clear_display(const struct device *i2c_dev) {
    // 1. Efface le framebuffer RAM
    memset(framebuffer, 0, sizeof(framebuffer));

    // 2. Pour chaque page, envoie 128 zéros d'un coup
    for (uint8_t page = 0; page < 8; page++) {
        ssd1306_write_cmd(i2c_dev, 0x21); // Set column address
        ssd1306_write_cmd(i2c_dev, 0);    // Start column
        ssd1306_write_cmd(i2c_dev, 127);  // End column
        ssd1306_write_cmd(i2c_dev, 0x22); // Set page address
        ssd1306_write_cmd(i2c_dev, page); // Start page
        ssd1306_write_cmd(i2c_dev, page); // End page

        // Envoie 128 octets d'un coup (plus rapide que 128 appels séparés)
        uint8_t buf[129];
        buf[0] = 0x40; // Control byte for data
        memset(&buf[1], 0, 128);
        i2c_write(i2c_dev, buf, sizeof(buf), SSD1306_I2C_ADDR);
    }
}

void print_satellite_stats(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	uint8_t tracked   = 0;
	uint8_t in_fix    = 0;
	uint8_t unhealthy = 0;

	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
		if (pvt_data->sv[i].sv > 0) {
			tracked++;

			if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) {
				in_fix++;
			}

			if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) {
				unhealthy++;
			}
		}
	}

	gps_tracking = tracked;
	gps_using = in_fix;
	gps_unk = unhealthy;


}

int blocking_send(int fd, uint8_t *buf, uint32_t size, uint32_t flags)
{
    int err;

    do {
        err = send(fd, buf, size, flags);
    } while (err < 0 && (errno == EAGAIN));

    return err;
}

int blocking_connect(int fd, struct sockaddr *local_addr, socklen_t len)
{
    int err;

    do {
        err = connect(fd, local_addr, len);
		LOG_INF("ERROR CONNECT = %d", err);
    } while (err < 0 && errno == EAGAIN);

    return err;
}

void send_to_cloud(){




			lte_lc_connect();

						LOG_INF("5");

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

    int err = getaddrinfo(HTTP_HOST, NULL, &hints, &res);
    LOG_INF("getaddrinfo err: %d", err);
	
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);
   


	//for(int i =0;i<5;i++){



    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    LOG_INF("client_fd: %d", client_fd);
    err = bind(client_fd, (struct sockaddr *)&local_addr,sizeof(local_addr));
    LOG_INF("bind err: %d", err);


    err = blocking_connect(client_fd, (struct sockaddr *)res->ai_addr,sizeof(struct sockaddr_in));
    LOG_INF("connect err: %d", err);


	if (err >= 0) {

    LOG_INF("Prepare send buffer:");
	
	
	send_data_len = snprintf(send_buf, 2000,
									"POST %s HTTP/1.1\r\n"
                                    "Host: %s\r\n\r\n"
									JSON_TEMPLATE_LONG,
									HTTP_PATH_LONG, HTTP_HOST,
									(long)ts[0], lat[0], lng[0],
									(long)ts[1], lat[1], lng[1],
									(long)ts[2], lat[2], lng[2],
									(long)ts[3], lat[3], lng[3],
									(long)ts[4], lat[4], lng[4]);



    do {
        num_bytes =
        blocking_send(client_fd, send_buf, send_data_len, 0);
       
        if (num_bytes < 0) {
            LOG_INF("ret: %d, errno: %s\n", num_bytes, strerror(errno));
        };
		

    } while (num_bytes < 0);


    LOG_INF("Finished. Closing socket");
    err = close(client_fd);

}

    freeaddrinfo(res);
int ret;
  ret = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);


    if (ret) {
        LOG_ERR("Failed to disconnect from LTE network");
    } else {
        LOG_INF("Disconnected from LTE network");
    }


}

void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
/*
	
	printf("Latitude:       %.06f\n", pvt_data->latitude);
	printf("Longitude:      %.06f\n", pvt_data->longitude);
	printf("Altitude:       %.01f m\n", pvt_data->altitude);
	printf("Accuracy:       %.01f m\n", pvt_data->accuracy);
	printf("Speed:          %.01f m/s\n", pvt_data->speed);
	printf("Speed accuracy: %.01f m/s\n", pvt_data->speed_accuracy);
	printf("Heading:        %.01f deg\n", pvt_data->heading);
	printf("Date:           %04u-%02u-%02u\n",
	       pvt_data->datetime.year,
	       pvt_data->datetime.month,
	       pvt_data->datetime.day);
	printf("Time (UTC):     %02u:%02u:%02u.%03u\n",
	       pvt_data->datetime.hour,
	       pvt_data->datetime.minute,
	       pvt_data->datetime.seconds,
	       pvt_data->datetime.ms);
	printf("PDOP:           %.01f\n", pvt_data->pdop);
	printf("HDOP:           %.01f\n", pvt_data->hdop);
	printf("VDOP:           %.01f\n", pvt_data->vdop);
	printf("TDOP:           %.01f\n", pvt_data->tdop);
*/

	struct tm tm;

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

void initial_connexion(){

	int err = 0;

	lte_lc_connect();
	if (err) {
		LOG_ERR("Failed to connect to LTE network, error: %d", err);
	}

	LOG_INF("Connected to LTE network");

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

    err = getaddrinfo(HTTP_HOST, NULL, &hints, &res);
    LOG_INF("getaddrinfo err: %d", err);
	
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);
   

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    LOG_INF("client_fd: %d", client_fd);
    err = bind(client_fd, (struct sockaddr *)&local_addr,sizeof(local_addr));
    LOG_INF("bind err: %d", err);


    err = blocking_connect(client_fd, (struct sockaddr *)res->ai_addr,sizeof(struct sockaddr_in));
    LOG_INF("connect err: %d", err);


	if (err >= 0) {

    LOG_INF("Prepare send buffer:");

	
	uint8_t x = 0, y = 8;

	char line[128];
	snprintf(line,sizeof(line),"CONNECTING...");
	write_line(i2c_dev, line, x,y,0);


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

	char recv_buf[RECV_BUF_SIZE] = {0};
	int tot_num_bytes = 0;

	do {
		/* TODO: make a proper timeout *
		 * Current solution will just hang 
		 * until remote side closes connection */
		num_bytes = recv(client_fd, recv_buf, RECV_BUF_SIZE, 0);
		tot_num_bytes += num_bytes;
                LOG_INF("total number of bytes: %d\n", tot_num_bytes);
                LOG_INF("num bytes: %d\n", num_bytes);
		if (num_bytes < 0) {
			LOG_INF("\nrecv errno: %d\n", errno);
			break;
		}
		//LOG_INF("%s\n", recv_buf);
	} while (num_bytes > 0);


    double values[10];                       // Tableau pour stocker les doubles
    size_t count = 0;


    char *token = strtok(recv_buf, ",");
    while (token != NULL && count < 10) {
        values[count] = strtod(token, NULL);  // Conversion vers double
        count++;
        token = strtok(NULL, ",");
    }

	targets_count = (count/2);

		x = 0;
		y = 16;

		if(targets_count <= 1){
			snprintf(line, sizeof(line),"%d TARGET",targets_count);

		}else{
			snprintf(line, sizeof(line),"%d TARGETS",targets_count);

		}	

		write_line(i2c_dev, line, x, y,0);


    // Afficher les résultats
    for (size_t i = 0; i < targets_count*2; i++) {
		targets[i] = values[i];
    }

    LOG_INF("Finished. Closing socket");
    err = close(client_fd);

}

    freeaddrinfo(res);
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	lte_state = 0;

}

void flash_firmware(){

	int err = 0;

	lte_lc_connect();
		if (err) {
			LOG_ERR("Failed to connect to LTE network, error: %d", err);
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

	boot_request_upgrade(BOOT_SWAP_TYPE_PERM);
	sys_reboot(SYS_REBOOT_COLD);


}

float sensor_value_to_float(const struct sensor_value *val)
{
    return (float)val->val1 + (val->val2 / 1000000.0f);
}
 
void accelerometer_thread(void *a, void *b, void *c) {

	const struct device *lis2dh = (const struct device *)a;

    while (1) {

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


		avg += sqrtf(ax*ax+ay*ay+az*az)-9.8;


		valuex = ax*ax;
		valuey = ay*ay;
		valuez = az*az;


		k_sleep(K_MSEC(measure_rate));

    }
}

void tap_thread(void *a, void *b, void *c) {

	int reset_state = 0;

	while(1){

		if(reset_state >= reset_threshold){

			reset_state = 0;
			avg = 0;
			tap_state = 0;
			
		}

		if(fabs(avg) > tap_threshold){
			LOG_INF("TAPPED %.2f",valuex);

			tap_state = 1;
			reset_state = reset_threshold;
			k_sleep(K_MSEC(tap_kickback_waittime));
		}

		k_sleep(K_MSEC(tap_reset_rate));
		reset_state += 1;
	}
}

void gps_thread(void *a, void *b, void *c) {

while(true){

k_mutex_lock(&state.mutex, K_FOREVER);
while(!state.ready) {
	k_condvar_wait(&state.cond, &state.mutex,K_FOREVER);
}
k_mutex_unlock(&state.mutex);


	LOG_INF("GPS");


			k_mutex_lock(&lte_mutex, K_FOREVER);


			print_satellite_stats(&last_pvt);

			if (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
							fix_timestamp = k_uptime_get();
							print_fix_data(&last_pvt);
							//print_distance_from_reference(&last_pvt);


						} else {

							last_fix = (uint32_t)((k_uptime_get() - fix_timestamp) / 1000);

						}

								k_mutex_unlock(&lte_mutex);
	k_sleep(K_MSEC(gps_rate));

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

	if(lte_state == 0){return;}

	clear_display(i2c_dev);

	uint8_t x = 0, y = 0;
	char line[128];
	snprintf(line,sizeof(line),"INITIALIZATION...");
	write_line(i2c_dev, line, x, y, 0);


	lte_lc_init();
	lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_LTEM_GPS,LTE_LC_SYSTEM_MODE_PREFER_LTEM);

	initial_connexion();

	menu = -2;
	bypass = 1;
	multitap = 1;
	first = 0;

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









	lte_lc_init();
	lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_LTEM_GPS,LTE_LC_SYSTEM_MODE_PREFER_LTEM);

	flash_firmware();


	menu = -1;
	bypass = 1;
	multitap = 1;

}

void ssd1306_power_off(const struct device *i2c_dev) {
    ssd1306_write_cmd(i2c_dev, 0xAE); // Display OFF
}

void ssd1306_power_on(const struct device *i2c_dev) {
    ssd1306_write_cmd(i2c_dev, 0xAF); // Display ON
}

void off(const struct device *i2c_dev){

	k_mutex_lock(&state.mutex, K_FOREVER);
	state.ready = false;
	k_mutex_unlock(&state.mutex);

	ssd1306_power_off(i2c_dev);

	while(tap_state == 0){


		k_sleep(K_SECONDS(5));

	}

	ssd1306_power_on(i2c_dev);

	k_mutex_lock(&state.mutex, K_FOREVER);
	state.ready = true;
	k_condvar_broadcast(&state.cond);
	k_mutex_unlock(&state.mutex);

}

void write_line(const struct device *i2c_dev, const char *line, uint8_t x, uint8_t y, int erase) {
	for (const char *p = line; *p; p++) {
		dessiner_caractere(i2c_dev, *p, x, y, erase);
		x += 6;
	}
}

int main(void)
{

k_mutex_init(&lte_mutex);
k_mutex_init(&state.mutex);
k_condvar_init(&state.cond);
state.ready = true;


		uint8_t x = 0, y = 0;

char message[128];


	//const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device not ready");
		sys_reboot(SYS_REBOOT_COLD);
	}
	LOG_INF("I2C device is ready");

	clear_display(i2c_dev); // Efface l'écran avant de dessiner
    ssd1306_init(i2c_dev); // <-- Ajoute cette ligne ici



	write_line(i2c_dev, "FW_VERSION 3", 0, 0, 1);
	k_sleep(K_SECONDS(5));

	clear_display(i2c_dev); // Efface l'écran avant de dessiner

    const struct device *lis2dh = DEVICE_DT_GET_ONE(st_lis2dh);
    if (!device_is_ready(lis2dh)) {
        LOG_ERR("Erreur : LIS2DH non prêt\n");
        sys_reboot(SYS_REBOOT_COLD);
    }

	k_thread_create(&accelerometer_data, accelerometer_stack, 1024,accelerometer_thread, (void *)lis2dh, NULL, NULL,5, 0, K_NO_WAIT);
	k_thread_create(&tap_data, tap_stack, 1024,tap_thread, NULL, NULL, NULL,5, 0, K_NO_WAIT);
	k_thread_create(&gps_data, gps_stack, 1024,gps_thread, NULL, NULL, NULL,5, 0, K_NO_WAIT);

	int err;

	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Modem library initialization failed, error: %d", err);
		sys_reboot(SYS_REBOOT_COLD);
	}

	lte_lc_init();
	lte_lc_system_mode_set(LTE_LC_SYSTEM_MODE_LTEM_GPS,LTE_LC_SYSTEM_MODE_PREFER_LTEM);

	/* Initialize reference coordinates (if used). */
	if (sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE) > 1 &&
	    sizeof(CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE) > 1) {
		ref_used = true;
		ref_latitude = atof(CONFIG_GNSS_SAMPLE_REFERENCE_LATITUDE);
		ref_longitude = atof(CONFIG_GNSS_SAMPLE_REFERENCE_LONGITUDE);
	}

	if (modem_init() != 0) {
		LOG_ERR("Failed to initialize modem");
		sys_reboot(SYS_REBOOT_COLD);
	}

	if (sample_init() != 0) {
		LOG_ERR("Failed to initialize sample");
		sys_reboot(SYS_REBOOT_COLD);
	}

	if (gnss_init_and_start() != 0) {
		LOG_ERR("Failed to initialize and start GNSS");
		sys_reboot(SYS_REBOOT_COLD);
	}

	fix_timestamp = k_uptime_get();

	lte_lc_init();

	multitap = 0;
	int confirmation = 0;

	while(true){

	if(menu == -1){

		if(first == 0){
					
		snprintf(message, sizeof(message), "T: %2d U: %2d UN: %d", 0, 0, 0);
		write_line(i2c_dev, message, 0, 0, 0);

		snprintf(message, sizeof(message), "LAST FIX: %d   ",0);
		write_line(i2c_dev, message, 0, 8, 0);


		x = 0; 
		y = 32;


		for(int i=0;i<targets_count;i++){

			snprintf(message, sizeof(message), "%1d:",i+1);
			write_line(i2c_dev, message, 0, y, 0);

			snprintf(message, sizeof(message), "DEG");
			write_line(i2c_dev, message, 7*6, y, 0);

			snprintf(message, sizeof(message), "|");
			write_line(i2c_dev, message, 12*6, y, 0);

			snprintf(message, sizeof(message), "M");
			write_line(i2c_dev, message, 20*6, y, 0);

			y+= 8;
			x = 0;
		}


				}



				if(last_fix != ref_lastfix){
					ref_lastfix = last_fix;
					y=8;
					x=10*6;

					snprintf(message, sizeof(message), "%d   ",last_fix);
					write_line(i2c_dev, message, x,y,1);
		
				}

				if(gps_state == 1 || first == 0){

					first = 1;
					y = 0;

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

					if(lte_state == 1 && gps_using >= 4){

					if(position >= 5*20){

						position = 0;

						k_mutex_lock(&lte_mutex, K_SECONDS(60));

						send_to_cloud();

						k_mutex_unlock(&lte_mutex);

					}

					if((position+20) % 20 == 0){
						ts[position/20] = timestamp;
						lat[position/20] = last_latitude;
						lng[position/20] = last_longitude;
					}

					position++;
		


					}

	if(fabs(ref_latitude2 - last_latitude) > 0.00001 || fabs(ref_longitude2 - last_longitude) > 0.00001){

		ref_latitude2 = last_latitude;
		ref_longitude2 = last_longitude;

		x = 0; 
		y = 32;

		for(int i=0;i<targets_count;i++){

			int heading, distance;

			calcul_cap_distance_int(last_latitude, last_longitude, targets[i*2], targets[i*2+1], &heading, &distance);

			if(distance > 10000){
				distance = 9999;
			}


			x = 3*6;

			snprintf(message, sizeof(message), "%3d", heading);
			write_line(i2c_dev, message, x, y, 1);

			x = 14*6;
			snprintf(message, sizeof(message), "%4d", distance);
			write_line(i2c_dev, message, x, y, 1);

			y+= 8;
			x = 0;
		}

}

				}

		}

	if(tap_state == 1 || bypass == 1){

			bypass = 0;
			multitap += 1;


			if(menu == -1 || menu == -2){

				multitap = 0;
				LOG_INF("GOING IN THE MENU");
				menu = 0;

				clear_display(i2c_dev);
				y=0;

				for(int i=0;i<nb_menu;i++){

					LOG_INF("%s",menu_str[i]);
					x = 6;
					snprintf(message, sizeof(message), "%s",menu_str[i]);
					write_line(i2c_dev, message, x, y, 0);

					if(i == 1){

						if(gps_state == 0){
							snprintf(message, sizeof(message), " OFF");
						}else{
							snprintf(message, sizeof(message), " ON ");
						}

						write_line(i2c_dev,message, 5*6,y,0);

					}
					if(i == 2){

						if(lte_state == 0){
							snprintf(message, sizeof(message), " OFF");
						}else{
							snprintf(message, sizeof(message), " ON ");
						}

						write_line(i2c_dev,message, 5*6,y,0);
					}

				y += 8;


				}


				//Remettre le nouveau caractère
				x = 0;
				y = 0;
				snprintf(message, sizeof(message), ">");
				write_line(i2c_dev, message, x, y, 0);
				confirmation = 0;

			}
			
			if(menu != -1 && multitap == 1){

				multitap = 0;
				confirmation = 0;
				
				//effacer le caractère
				snprintf(message, sizeof(message), " ");
				write_line(i2c_dev, message, 0, menu*8, 1);

				//Changer la position
				menu = (menu + 1 + nb_menu) % nb_menu;

				//Remettre le nouveau caractère
				snprintf(message, sizeof(message), ">");
				write_line(i2c_dev, message, 0, menu*8, 0);

			}

		}

	if(menu != -1 && confirmation >= confirmation_timeout){

				multitap = 0;
				confirmation = 0;

				LOG_ERR("MENU CHOISI: %s",menu_str[menu]);


				switch(menu){

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
							change_gps(i2c_dev);
							first = 0;
							break;
					case 2:
							change_lte(i2c_dev);
							first = 0;
							break;
					case 3:
							reload_coordinates(i2c_dev);
							first = 0;
							break;
					case 4:
							flash_firmware();
							break;
					case 5:
							off(i2c_dev);
							break;
				}

			}

		k_sleep(K_MSEC(100));	
		confirmation += 1;	

	}

	return 0;
}
