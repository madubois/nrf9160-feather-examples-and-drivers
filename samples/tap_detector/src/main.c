/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>


void trigger_handler(const struct device *dev,
                     const struct sensor_trigger *trig)
{
    struct sensor_value accel[3];

    int err = sensor_sample_fetch(dev);
    if (err) {
        printk("Failed to fetch sample: %d\n", err);
        return;
    }

    err = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    if (err) {
        printk("Failed to get acceleration data: %d\n", err);
        return;
    }

    printk("Acceleration X: %d.%06d, Y: %d.%06d, Z: %d.%06d\n",
           accel[0].val1, accel[0].val2,
           accel[1].val1, accel[1].val2,
           accel[2].val1, accel[2].val2);
}

int main(void)
{
   
    const struct device *accelerometer = DEVICE_DT_GET(DT_ALIAS(accel0));
    if (!device_is_ready(accelerometer)) {
        printk("Accelerometer device not ready\n");
        return -1;
    }




    struct sensor_value attr;
    int err;


/*
    // Set full scale to ±2g
    struct sensor_value full_scale = {
        .val1 = 2,  // 2g
        .val2 = 0
    };

    err = sensor_attr_set(accelerometer, SENSOR_CHAN_ACCEL_XYZ,
                             SENSOR_ATTR_FULL_SCALE, 
                             &full_scale);
    if (err) {
        printk("Failed to set full scale: %d\n", err);
        return err;
    }

    // Set sampling frequency to 100 Hz
    struct sensor_value odr = {
        .val1 = 100,
        .val2 = 0
    };

    err = sensor_attr_set(accelerometer, SENSOR_CHAN_ACCEL_XYZ,
                         SENSOR_ATTR_SAMPLING_FREQUENCY, 
                         &odr);
    if (err) {
        printk("Failed to set sampling frequency: %d\n", err);
        return err;
    }

*/








    /*
    
    attr.val1 = 0;
    attr.val2 = (int32_t)(SENSOR_G * 1.5);

    err = sensor_attr_set(accelerometer, SENSOR_CHAN_ACCEL_XYZ,
                     SENSOR_ATTR_SLOPE_TH, &attr);

    if (err) { 
        printk("Failed to set slope threshold: %d\n", err);
        return err;
    }   
*/

    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

struct sensor_value odr = {
                .val1 = 1,
            };

            err = sensor_attr_set(accelerometer, trig.chan,
                                 SENSOR_ATTR_SAMPLING_FREQUENCY,
                                 &odr);






    err = sensor_trigger_set(accelerometer, &trig, trigger_handler);
    if (err) {
        printk("Failed to set trigger: %d\n", err);
        return err;
    }



    while(true) {
        // Wait for the trigger to be called
        k_sleep(K_MSEC(1000));
    }
    



    return 0;
}
