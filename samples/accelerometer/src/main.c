#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>


#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gnss_sample, CONFIG_LOG_DEFAULT_LEVEL);



#define LIS2DH_I2C_ADDR 0x19  // ou 0x19 selon votre câblage
#define LIS2DH_CLICK_SRC_REG 0x39


#define TAPBUFFER 500
#define THRESHOLD 10
#define TAPWINDOW 10

float x_buffer[TAPBUFFER];
float y_buffer[TAPBUFFER];
float z_buffer[TAPBUFFER];
int buffer_position = 0;

float x_buffer_avg = 0;
float y_buffer_avg = 0;
float z_buffer_avg = 0;

float x_buffer_variation = 0;
float y_buffer_variation = 0;
float z_buffer_variation = 0;

int tapwindow_position = 0;

int k = 0;

int position = 0;

static float sensor_value_to_float(const struct sensor_value *val)
{
    return (float)val->val1 + (val->val2 / 1000000.0f);
}

static void lis2dh_trigger_handler(const struct device *dev)
                                   //const struct sensor_trigger *trig)
{


    struct sensor_value accel[3];

    if (sensor_sample_fetch(dev) < 0) {
        LOG_INF("Erreur lors de la récupération des données du capteur\n");
        return;
    }

    if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
        LOG_INF("Erreur lors dze la récupération des données du capteur\n");
        return;
    }



    float ax = sensor_value_to_float(&accel[0]);
    float ay = sensor_value_to_float(&accel[1]);
    float az = sensor_value_to_float(&accel[2]);

        k++;

    //if(k > TAPBUFFER){
    //    LOG_INF("Accélération : X=%.2f g, Y=%.2f g, Z=%.2f g", ax, ay, az);

    //    k = 0;
    //}



    x_buffer_avg += ax;
    x_buffer_avg -= x_buffer[buffer_position];
    x_buffer[buffer_position] = ax;

    y_buffer_avg += ay; 
    y_buffer_avg -= y_buffer[buffer_position];
    y_buffer[buffer_position] = ay;
    
    //z_buffer_avg -= z_buffer[position_to_remove];
    //z_buffer_avg += az;
    //z_buffer[buffer_position] = ax;




    buffer_position = (buffer_position + 1 + TAPBUFFER) % TAPBUFFER;
    





    if(tapwindow_position < 0){

    if(ax > x_buffer_avg/(float)TAPBUFFER + THRESHOLD){

        LOG_INF("%.2f %.2f DROITE",ax,ay);
        tapwindow_position = TAPWINDOW;
        return;
    }
    if(ax < x_buffer_avg/(float)TAPBUFFER - THRESHOLD){
        LOG_INF("%.2f %.2f GAUCHE",ax,ay);
        tapwindow_position = TAPWINDOW;
        return;
    }
    
    if(ay > y_buffer_avg/(float)TAPBUFFER + THRESHOLD){

        LOG_INF("%.2f %.2f HAUT",ax,ay);
        tapwindow_position = TAPWINDOW;
        return;
    }

    if(ay < y_buffer_avg/(float)TAPBUFFER - THRESHOLD){
        LOG_INF("%.2f %.2f BAS",ax,ay);
        tapwindow_position = TAPWINDOW;
        return;
    }



    }else{tapwindow_position-=1;
    }

    /*

    printk("📈 Accélération : X=%d.%03d g, Y=%d.%03d g, Z=%d.%03d g\n",
       (int)ax, (int)(fabsf(ax - (int)ax) * 1000),
       (int)ay, (int)(fabsf(ay - (int)ay) * 1000),
       (int)az, (int)(fabsf(az - (int)az) * 1000));

       */
}


static void lis2dh_trigger_handler2(const struct device *dev,
                                   const struct sensor_trigger *trig)
{

    LOG_INF("read2");

}




void uart_thread(void *arg1, void *arg2, void *arg3){

    while (true)
    {



    struct sensor_value accel[3];

    if (sensor_sample_fetch(arg1) < 0) {
        LOG_INF("Erreur lors de la récupération des données du capteur\n");
        return;
    }

    if (sensor_channel_get(arg1, SENSOR_CHAN_ACCEL_XYZ, accel) < 0) {
        LOG_INF("Erreur lors dze la récupération des données du capteur\n");
        return;
    }

float ax = sensor_value_to_float(&accel[0]);
float ay = sensor_value_to_float(&accel[1]);



    buffer_position = (buffer_position + 1 + TAPBUFFER) % TAPBUFFER;
    





    if(tapwindow_position < 0 && arg2 == 0){

    if(ax > x_buffer_avg/(float)TAPBUFFER + THRESHOLD){

        LOG_INF("%.2f %.2f DROITE",ax,ay);
        tapwindow_position = TAPWINDOW;
        arg2 = 1;
        return;
    }
    if(ax < x_buffer_avg/(float)TAPBUFFER - THRESHOLD){
        LOG_INF("%.2f %.2f GAUCHE",ax,ay);
        tapwindow_position = TAPWINDOW;
        arg2 = 2;
        return;
    }
    
    if(ay > y_buffer_avg/(float)TAPBUFFER + THRESHOLD){

        LOG_INF("%.2f %.2f HAUT",ax,ay);
        tapwindow_position = TAPWINDOW;
        arg2 = 3;
        return;
    }

    if(ay < y_buffer_avg/(float)TAPBUFFER - THRESHOLD){
        LOG_INF("%.2f %.2f BAS",ax,ay);
        tapwindow_position = TAPWINDOW;
        arg2 = 4;
        return;
    }

    }else{
        

        tapwindow_position-=1;
    }




        /* code */
    }
    
}


#define STACKSIZE KB(2)
//K_THREAD_DEFINE(uart_thread_id, STACKSIZE, uart_thread, NULL, NULL, NULL,K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

K_THREAD_STACK_DEFINE(uart_thread_stack_area, STACKSIZE);
struct k_thread uart_thread_data;


void main(void)
{


    
for(int i =0;i<TAPBUFFER;i++){
    x_buffer[i] = 0;
}

    const struct device *lis2dh = DEVICE_DT_GET_ONE(st_lis2dh);

    if (!device_is_ready(lis2dh)) {
        printk("Erreur : LIS2DH non prêt\n");
        return;
    }

    

struct sensor_value full_scale;

// Mettre à ±4g → 4 * 9.80665 = 39.2266 m/s²
    full_scale.val1 = 19;        // partie entière
    full_scale.val2 = 613300;    // partie fractionnaire en micro (1e-6)

// Envoyer l'attribut
int ret = sensor_attr_set(lis2dh, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_FULL_SCALE, &full_scale);




    k_tid_t uast_tid = k_thread_create(&uart_thread_data,uart_thread_stack_area,
                                    K_THREAD_STACK_SIZEOF(uart_thread_stack_area),
                                    uart_thread, lis2dh, &position, NULL,
                                    K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);




    /*
    struct sensor_value attr;
    attr.val1 = 0;
    attr.val2 = (int32_t)(SENSOR_G * 1.5);

    if (sensor_attr_set(lis2dh, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SLOPE_TH, &attr) < 0) {
        printk("Erreur lors de la configuration du seuil de pente\n");
    }
*/




/*
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_ACCEL_XYZ,
    };

    if (sensor_trigger_set(lis2dh, &trig, lis2dh_trigger_handler2) < 0) {
        printk("Erreur lors de l’enregistrement du trigger\n");
    } else {
        printk("Trigger prêt. Attente de tap...\n");
    }

*/



    while(true){


        if(position != 0){
            switch(position){
                case 1:
                    LOG_INF("DROITE");
                    break;
                case 2:
                    LOG_INF("GAUCHE");
                    break;
                case 3:
                    LOG_INF("HAUT");
                    break;
                case 4:
                    LOG_INF("BAS");
                    break;
            }
            position = 0;
        }

        

        k_sleep(K_SECONDS(1));
    }
}
