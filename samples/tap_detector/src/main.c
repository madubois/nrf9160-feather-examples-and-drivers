/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>




#define BASELINE_ACCEL_BUFFER 100
#define TAP_THRESHOLD 100
#define TAP_TIMELIMIT 50

double baseline_accel_x[BASELINE_ACCEL_BUFFER];
double baseline_accel_y[BASELINE_ACCEL_BUFFER];
double baseline_accel_z[BASELINE_ACCEL_BUFFER];

int baseline_index = 0;
double baseline_avg_x = 0;
double baseline_avg_y = 0;
double baseline_avg_z = 0;
bool tap_detected = false;
int tap_reset_counter = 0;
int tap_state = 0;



int was_tapped(const struct device *sensor){

    double accel_x, accel_y, accel_z;

    static unsigned int count;
    struct sensor_value accel[3];
    const char *overrun = "";
    int rc = sensor_sample_fetch(sensor);

    ++count;
    if (rc == -EBADMSG)
    {
        /* Sample overrun.  Ignore in polled mode. */
        if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER))
        {
            overrun = "[OVERRUN] ";
        }
        rc = 0;
    }
    if (rc == 0)
    {
        rc = sensor_channel_get(sensor,
                                SENSOR_CHAN_ACCEL_XYZ,
                                accel);
    }
    if (rc < 0)
    {
        printf("ERROR: Update failed: %d\n", rc);
    }
    else
    {

        accel_x = sensor_value_to_double(&accel[0])/ 9.81; // Convert to g's
        accel_y = sensor_value_to_double(&accel[1])/ 9.81; // Convert to g's
        accel_z = sensor_value_to_double(&accel[2])/ 9.81; // Convert to g's


        double diff_x = accel_x / baseline_avg_x;
        double diff_y = accel_y / baseline_avg_y;
        double diff_z = accel_z / baseline_avg_z;

        baseline_avg_x += accel_x - baseline_accel_x[baseline_index];
        baseline_avg_y += accel_y - baseline_accel_y[baseline_index];
        baseline_avg_z += accel_z - baseline_accel_z[baseline_index];

        baseline_accel_x[baseline_index] = accel_x;
        baseline_accel_y[baseline_index] = accel_y;
        baseline_accel_z[baseline_index] = accel_z;

        baseline_index = (baseline_index + 1) % BASELINE_ACCEL_BUFFER;

        double tap_x = accel_x * BASELINE_ACCEL_BUFFER - baseline_avg_x;
        double tap_y = accel_y * BASELINE_ACCEL_BUFFER - baseline_avg_y;
        double tap_z = accel_z * BASELINE_ACCEL_BUFFER - baseline_avg_z;

        if(tap_detected == false){

            if(tap_x < -TAP_THRESHOLD){
                tap_detected = true;
                tap_reset_counter = (baseline_index + TAP_TIMELIMIT) % BASELINE_ACCEL_BUFFER;
                tap_state = 1;
                return tap_state;
            }

            if(tap_y < -TAP_THRESHOLD){
                tap_detected = true;
                tap_reset_counter = (baseline_index + TAP_TIMELIMIT) % BASELINE_ACCEL_BUFFER;
                tap_state = 2;
                return tap_state;
            }
/*
            if(tap_z < -TAP_THRESHOLD){
                tap_detected = true;
                tap_reset_counter = (baseline_index + TAP_TIMELIMIT) % BASELINE_ACCEL_BUFFER;
                tap_state = 3;
                return tap_state;
            }
*/
            if(tap_x > TAP_THRESHOLD){
                tap_detected = true;
                tap_reset_counter = (baseline_index + TAP_TIMELIMIT) % BASELINE_ACCEL_BUFFER;
                tap_state = 4;
                return tap_state;
            }

            if(tap_y > TAP_THRESHOLD){
                tap_detected = true;
                tap_reset_counter = (baseline_index + TAP_TIMELIMIT) % BASELINE_ACCEL_BUFFER;
                tap_state = 5;
                return tap_state;
            }
/*
            if(tap_z > TAP_THRESHOLD){
                tap_detected = true;
                tap_reset_counter = (baseline_index + TAP_TIMELIMIT) % BASELINE_ACCEL_BUFFER;
                tap_state = 6;
                return tap_state;
            }
*/
        
        }else{

            if(baseline_index == tap_reset_counter){
                tap_detected = false;
                tap_state = 0;
            }

        }

            return 0;


    }



}






static void tap_handler(const struct device *dev,
                            const struct sensor_trigger *trig)
{
    printf("Tap detected\n");
}




static void fetch_and_display(const struct device *sensor)
{
    static unsigned int count;
    struct sensor_value accel[3];
    const char *overrun = "";
    int rc = sensor_sample_fetch(sensor);

    ++count;
    if (rc == -EBADMSG)
    {
        /* Sample overrun.  Ignore in polled mode. */
        if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER))
        {
            overrun = "[OVERRUN] ";
        }
        rc = 0;
    }
    if (rc == 0)
    {
        rc = sensor_channel_get(sensor,
                                SENSOR_CHAN_ACCEL_XYZ,
                                accel);
    }
    if (rc < 0)
    {
        printf("ERROR: Update failed: %d\n", rc);
    }
    else
    {
        printf("#%u @ %u ms: %sx %f , y %f , z %f\n",
               count, k_uptime_get_32(), overrun,
               sensor_value_to_double(&accel[0]),
               sensor_value_to_double(&accel[1]),
               sensor_value_to_double(&accel[2]));
    }
}

#ifdef CONFIG_LIS2DH_TRIGGER
static void trigger_handler(const struct device *dev,
                            const struct sensor_trigger *trig)
{
    fetch_and_display(dev);
}
#endif

int main(void)
{
    const struct device *sensor = DEVICE_DT_GET(DT_ALIAS(accel0));

    if (sensor == NULL || !device_is_ready(sensor))
    {
        printf("Could not get accel0 device\n");
        return -ENODEV;
    }

#if CONFIG_LIS2DH_TRIGGER
    {
        struct sensor_trigger trig;
        int rc;

        trig.type = SENSOR_TRIG_DATA_READY;
        trig.chan = SENSOR_CHAN_ACCEL_XYZ;

        if (IS_ENABLED(CONFIG_LIS2DH_ODR_RUNTIME))
        {
            struct sensor_value odr = {
                .val1 = 1,
            };

            rc = sensor_attr_set(sensor, trig.chan,
                                 SENSOR_ATTR_SAMPLING_FREQUENCY,
                                 &odr);
            if (rc != 0)
            {
                printf("Failed to set odr: %d\n", rc);
                return rc;
            }
            printf("Sampling at %u Hz\n", odr.val1);
        }

        rc = sensor_trigger_set(sensor, &trig, trigger_handler);
        if (rc != 0)
        {
            printf("Failed to set trigger: %d\n", rc);
            return rc;
        }

        printf("Waiting for triggers\n");
        while (true)
        {
            k_sleep(K_MSEC(2000));
        }
    }
#else  /* CONFIG_LIS2DH_TRIGGER */



    printf("Polling at 0.5 Hz\n");
    while (true)
    {
        //fetch_and_display(sensor);
        //get_tap_state(sensor);
        
        int tapped = was_tapped(sensor);

        if (tapped > 0) {
            printf("Tap detected! State: %d\n", tapped);
        } 

        //printf("Tap state: %d\n", was_tapped(sensor));
        k_sleep(K_MSEC(1));
    }
#endif /* CONFIG_LIS2DH_TRIGGER */

    return 0;
}
