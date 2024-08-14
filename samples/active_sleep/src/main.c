/*
 * Copyright (c) 2022 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/mfd/npm1300.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
LOG_MODULE_REGISTER(main);

#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>

/* Addresses */
#define NPM1300_BUCK_BASE 0x04U
#define NPM1300_BUCK_OFFSET_EN_CLR 0x01U
#define NPM1300_BUCK_BUCKCTRL0 0x15U
#define NPM1300_BUCK_STATUS 0x34U

/* Gpios */
static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9160)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec latch_en =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), latch_en_gpios);

static const struct gpio_dt_spec wp =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), wp_gpios);
static const struct gpio_dt_spec hold =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), hold_gpios);
#endif

#if IS_ENABLED(CONFIG_REGULATOR_NPM1300)
static const struct device *buck2 = DEVICE_DT_GET(DT_NODELABEL(npm1300_buck2));
#else
static const struct device *dev_i2c2 = DEVICE_DT_GET(DT_NODELABEL(i2c2));
#endif

static void setup_accel(void) {
  const struct device *sensor = DEVICE_DT_GET(DT_ALIAS(accel0));

  if (!device_is_ready(sensor)) {
    printk("Could not get accel0 device\n");
    return;
  }

  // Disable the device
  struct sensor_value odr = {
      .val1 = 0,
  };

  int rc = sensor_attr_set(sensor, SENSOR_CHAN_ACCEL_XYZ,
                           SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
  if (rc != 0) {
    printk("Failed to set odr: %d\n", rc);
    return;
  }
}

static int setup_gpio(void) {

  gpio_pin_configure_dt(&sw0, GPIO_DISCONNECTED);
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9160)
  gpio_pin_configure_dt(&led0, GPIO_DISCONNECTED);
  gpio_pin_configure_dt(&latch_en, GPIO_DISCONNECTED);

  gpio_pin_configure_dt(&wp, GPIO_INPUT | GPIO_PULL_UP);
  gpio_pin_configure_dt(&hold, GPIO_INPUT | GPIO_PULL_UP);
#endif

  return 0;
}

int setup_uart() {

  static const struct device *const console_dev =
      DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

  /* Disable console UART */
  int err = pm_device_action_run(console_dev, PM_DEVICE_ACTION_SUSPEND);
  if (err < 0) {
    printk("Unable to suspend console UART. (err: %d)\n", err);
    return err;
  }

  /* Turn off to save power */
  NRF_CLOCK->TASKS_HFCLKSTOP = 1;

  return 0;
}

static int setup_pmic() {

  int err;

#if IS_ENABLED(CONFIG_REGULATOR_NPM1300)

  /* Disable if not already disabled */
  if (regulator_is_enabled(buck2)) {
    err = regulator_disable(buck2);
    if (err < 0) {
      LOG_ERR("Failed to disable buck2: %d", err);
      return err;
    }
  }
#else
  // Write necessary commands to disable buck
  // return mfd_npm1300_reg_write(config->mfd, BUCK_BASE, BUCK_OFFSET_EN_CLR +
  // 2U, 1U);
  {
    uint8_t buff[] = {NPM1300_BUCK_BASE, NPM1300_BUCK_OFFSET_EN_CLR + 2U, 1U};

    int err = i2c_write(dev_i2c2, buff, sizeof(buff), 0x6B);
    if (err < 0) {
      return err;
    }
  }

  uint8_t buff[] = {NPM1300_BUCK_BASE, NPM1300_BUCK_BUCKCTRL0, 0x08};
  return i2c_write(dev_i2c2, buff, sizeof(buff), 0x6B);

#endif

#if IS_ENABLED(CONFIG_MFD_NPM1300)
  /* Get pmic */
  static const struct device *pmic = DEVICE_DT_GET(DT_NODELABEL(npm1300_pmic));
  if (!pmic) {
    LOG_ERR("Failed to get PMIC device\n");
    return -ENODEV;
  }

  uint8_t reg = 0;

  /* See if pulldown is not already enabled */
  err = mfd_npm1300_reg_read(pmic, NPM1300_BUCK_BASE, NPM1300_BUCK_BUCKCTRL0,
                             &reg);
  if (err < 0)
    printk("Failed to set VBUSINLIM. Err: %d\n", err);

  if ((reg & 0x08) == 0) {

    /* Write to MFD to enable pulldown for BUCK2 */
    err = mfd_npm1300_reg_write(pmic, NPM1300_BUCK_BASE, NPM1300_BUCK_BUCKCTRL0,
                                0x08);
    if (err < 0)
      printk("Failed to set VBUSINLIM. Err: %d\n", err);
  }

  // for (int i = 0; i < 2; i++) {
  //   reg = 0;
  //   err = mfd_npm1300_reg_read(pmic, NPM1300_BUCK_BASE, NPM1300_BUCK_STATUS,
  //                              &reg);
  //   if (err < 0)
  //     printk("Failed to read buck status: Err: %i", err);
  //   else
  //     printk("Buck status: %i", reg);
  // }

#endif

  return 0;
}

int main(void) {
  int err = 0;

  LOG_INF("Active Sleep Sample");

  /* Setup GPIO */
  setup_gpio();

  /* Disable accel */
  setup_accel();

  /* Init modem */
  err = nrf_modem_lib_init();
  if (err < 0)
    LOG_ERR("Unable to initialize nRF Modem lib. Err: %i", err);

  /* Peripherals */
  setup_uart();

  /* Disable regulator */
  setup_pmic();

  return 0;
}
