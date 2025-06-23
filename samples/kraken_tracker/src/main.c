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

LOG_MODULE_REGISTER(gnss_sample, CONFIG_GNSS_SAMPLE_LOG_LEVEL);




#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>

#include <zephyr/sys/crc.h>


#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/flash_img.h>








#include <zephyr/net/socket.h>


#define HTTP_HOST "plongee.duckdns.org"
#define HTTP_PATH "/firmware"
#define HTTP_PORT 8545
#define MAX_MTU_SIZE     2000
#define RECV_BUF_SIZE    2048
#define SEND_BUF_SIZE    MAX_MTU_SIZE

#define TEST_STRING "T"

//#define JSON_TEMPLATE "{\"Angle X\": %d\"Angle Y\": %d\"Angle Z\": %d}"
#define JSON_TEMPLATE "%ld,%.9f,%.9f"


#define JSON_TEMPLATE_LONG "%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f,%ld,%.9f,%.9f"

    char send_buf[2000];





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






void flash_firmware(){

	struct flash_img_context fic;

	int err;
	struct sockaddr_in local_addr;
    struct addrinfo *res;
    int send_data_len;
    int num_bytes;
    int mtu_size = MAX_MTU_SIZE;
   
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

	nrf_modem_lib_init();
	lte_lc_init();
	lte_lc_connect();
	
    err = getaddrinfo(HTTP_HOST, NULL, &hints, &res);	
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTP_PORT);

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    err = bind(client_fd, (struct sockaddr *)&local_addr,sizeof(local_addr));
    err = blocking_connect(client_fd, (struct sockaddr *)res->ai_addr,sizeof(struct sockaddr_in));

	if (err >= 0) {
	
	send_data_len = snprintf(send_buf, 2000,
                                     "GET %s HTTP/1.1\r\n"
                                     "Host: %s\r\n\r\n",
                                     HTTP_PATH, HTTP_HOST);



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

	boot_request_upgrade(BOOT_SWAP_TYPE_TEST);
	sys_reboot(SYS_REBOOT_COLD);

}




int main(void)
{

	LOG_INF("Initiating firmware update...");
	k_sleep(K_FOREVER); // Pause pour voir le résultat

	return 0;
}
