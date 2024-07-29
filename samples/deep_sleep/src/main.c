/*
 * Copyright (c) 2024 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/mfd/npm1300.h>
#include <zephyr/sys/printk.h>

int main(void)
{

	printk("Deep sleep sample\n");

#if defined(CONFIG_BOARD_CIRCUITDOJO_FEATHER_NRF9161_NS)
	static const struct device *pmic = DEVICE_DT_GET(DT_NODELABEL(npm1300_pmic));

	/* set hibernate mode and power down */
	int ret = mfd_npm1300_hibernate(pmic, 60000);
	if (ret < 0)
	{
		printk("Failed to hibernate. Err: %d\n", ret);
		return ret;
	}

#else
#error Sample not supported on this board
#endif

	return 0;
}