#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "ip_addr.h"
#include "espconn.h"
#include "mem.h"


#define user_procTaskPrio 0
#define user_procTaskQueueLen 1

static os_event_t user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);


static volatile os_timer_t send_udp_timer;
LOCAL struct espconn udp_conn;


/* Access point details (environment variables) */
static uint8_t ssid[32] = AP_SSID;
static uint8_t pass[64] = AP_PASS;


static void ICACHE_FLASH_ATTR
udp_sent_callback(void *arg)
{
    os_printf("data sent\n");

    /* Sleep for 120 seconds */
    system_deep_sleep(120000000);
}


static void ICACHE_FLASH_ATTR
send_udp(void *arg)
{
    uint8_t ret;

    uint8_t data[] = "temp_data\n";
    uint16_t len = 10;

    /* TODO: There are infinite retries. We should instead reset the chip
     * if stuff fails too much */


    ret = wifi_station_get_connect_status();
    if (ret != STATION_GOT_IP)
    {
        /* If we're not connected, wait a bit */
        /* TODO: Is there a way to just get a callback when we have an IP? */
        os_printf("Wi-Fi state (%u). Retrying in 1 second.\n", ret);
        os_timer_arm(&send_udp_timer, 1000, 0);
        return;
    }


    /* Create a UDP connection */
    udp_conn.type = ESPCONN_UDP;
    udp_conn.state = ESPCONN_NONE;

    udp_conn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
    if (udp_conn.proto.udp == NULL)
    {
        os_printf("failed to allocate memory for UDP structure\n");
        os_timer_arm(&send_udp_timer, 1000, 0);
        return;
    }

    udp_conn.proto.udp->local_port = 4444;
    udp_conn.proto.udp->remote_ip[0] = 192;
    udp_conn.proto.udp->remote_ip[1] = 168;
    udp_conn.proto.udp->remote_ip[2] = 1;
    udp_conn.proto.udp->remote_ip[3] = 10;
    udp_conn.proto.udp->remote_port = 4444;

    espconn_regist_sentcb(&udp_conn, udp_sent_callback);

    ret = (uint8_t)espconn_create(&udp_conn);
    if (ret != 0)
    {
        os_printf("failed to create UDP connection structure\n");
        os_timer_arm(&send_udp_timer, 1000, 0);
        return;
    }


    /* Send some data to the server */
    os_printf("Sending data to server: %s\n", data);
    espconn_sent(&udp_conn, data, len);
}


/* Do nothing function */
static void ICACHE_FLASH_ATTR
user_procTask(os_event_t *events)
{
    os_delay_us(10);
}


void ICACHE_FLASH_ATTR
user_init()
{
    struct station_config cfg;

    /* Prepare AP details */
    cfg.bssid_set = 0;
    memcpy(&cfg.ssid, ssid, 32);
    memcpy(&cfg.password, pass, 64);

    uart_div_modify(0, UART_CLK_FREQ / 115200);

    /* Configure GPIO2 as an output */
    gpio_init();
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    gpio_output_set(0, BIT2, BIT2, 0);

    /* Set AP details */
#if 0
    wifi_set_opmode(STATION_MODE);
    wifi_set_station_config(&cfg);
#endif

    /* Set up the UDP timer */
    os_timer_disarm(&send_udp_timer);
    os_timer_setfn(&send_udp_timer, (os_timer_func_t *)send_udp, NULL);

    /* Start the UDP timer */
    os_timer_arm(&send_udp_timer, 100, 0);

    /* Start the OS task */
    system_os_task(user_procTask, user_procTaskPrio,
                   user_procTaskQueue, user_procTaskQueueLen);
}
