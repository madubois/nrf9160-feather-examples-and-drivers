/*
 * Copyright (c) 2022 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>

/* Gpios */
static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9160_NS)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec latch_en = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), latch_en_gpios);

static const struct gpio_dt_spec wp = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), wp_gpios);
static const struct gpio_dt_spec hold = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), hold_gpios);
#elif defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9161_NS)
static const struct device *buck2 = DEVICE_DT_GET(DT_NODELABEL(npm1300_buck2));
#endif

static void setup_accel(void)
{
	const struct device *sensor = DEVICE_DT_GET(DT_ALIAS(accel0));

	if (!device_is_ready(sensor))
	{
		printk("Could not get accel0 device\n");
		return;
	}

	// Disable the device
	struct sensor_value odr = {
		.val1 = 0,
	};

	int rc = sensor_attr_set(sensor, SENSOR_CHAN_ACCEL_XYZ,
							 SENSOR_ATTR_SAMPLING_FREQUENCY,
							 &odr);
	if (rc != 0)
	{
		printk("Failed to set odr: %d\n", rc);
		return;
	}
}

static int setup_gpio(void)
{

	gpio_pin_configure_dt(&sw0, GPIO_DISCONNECTED);
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9160_NS)
	gpio_pin_configure_dt(&led0, GPIO_DISCONNECTED);
	gpio_pin_configure_dt(&latch_en, GPIO_DISCONNECTED);

	gpio_pin_configure_dt(&wp, GPIO_INPUT | GPIO_PULL_UP);
	gpio_pin_configure_dt(&hold, GPIO_INPUT | GPIO_PULL_UP);
#endif

	return 0;
}

int setup_uart()
{

	static const struct device *const console_dev =
		DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	/* Disable console UART */
	int err = pm_device_action_run(console_dev, PM_DEVICE_ACTION_SUSPEND);
	if (err < 0)
	{
		printk("Unable to suspend console UART. (err: %d)\n", err);
		return err;
	}

	/* Turn off to save power */
	NRF_CLOCK->TASKS_HFCLKSTOP = 1;

	return 0;
}

int main(void)
{
	LOG_INF("Active Sleep Sample");

	/* Setup GPIO */
	setup_gpio();

	/* Disable accel */
	setup_accel();

	/* Init modem */
	nrf_modem_lib_init();

	/* Wait */
	k_sleep(K_SECONDS(2));

	/* Peripherals */
	setup_uart();

	/* Disable regulator */
#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9161_NS)
	int err = regulator_disable(buck2);
	if (err < 0)
		LOG_ERR("Failed to disable buck2: %d", err);
#endif

	return 0;
}
