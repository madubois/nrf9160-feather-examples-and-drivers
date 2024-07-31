
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_startup);

#if IS_ENABLED(CONFIG_MFD_NPM1300)
#include <zephyr/drivers/mfd/npm1300.h>

#define SYSREG_VBUSIN_BASE 0x02

#define SYSREG_TASKUPDATEILIMSW 0x00
#define SYSREG_VBUSINILIM0 0x01
#define SYSREG_VBUSINILIM_1000MA 0x0a

static int sample_setup(void)
{

    /* Get pmic */
    static const struct device *pmic = DEVICE_DT_GET(DT_NODELABEL(npm1300_pmic));
    if (!pmic)
    {
        LOG_ERR("Failed to get PMIC device\n");
        return -ENODEV;
    }

    /* Write to MFD to set SYSREG current to 1A */
    int ret = mfd_npm1300_reg_write(pmic, SYSREG_VBUSIN_BASE, SYSREG_VBUSINILIM0, 0);
    if (ret < 0)
    {
        printk("Failed to set VBUSINLIM. Err: %d\n", ret);
        return ret;
    }

    /* Save and update */
    ret = mfd_npm1300_reg_write(pmic, SYSREG_VBUSIN_BASE, SYSREG_TASKUPDATEILIMSW, 0x01);
    if (ret < 0)
    {
        printk("Failed to save settings. Err: %d\n", ret);
        return ret;
    }

    uint8_t data = 0;
    ret = mfd_npm1300_reg_read_burst(pmic, SYSREG_VBUSIN_BASE, SYSREG_VBUSINILIM0, &data, 1);
    if (ret < 0)
    {
        printk("Failed to read VBUSINLIM. Err: %d", ret);
        return ret;
    }
    else
    {
        printk("Vsys Current Limit: %d mA\n", data * 100);
    }

    return 0;
}
#else
static int sample_setup(void)
{
    return 0;
}
#endif

SYS_INIT(sample_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);