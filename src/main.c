/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/net_if.h>
#include <string.h>

#include "modules/button/button.h"
#include "modules/led/led.h"
#include "modules/messages.h"
#include "modules/webserver/webserver.h"
#include "modules/wifi/wifi.h"

/* ============================================================================
 * APPLICATION STATE MONITORING
 * ============================================================================
 */

/* Subscribe to WiFi events to start webserver when WiFi is ready */
static void wifi_event_listener(const struct zbus_channel *chan)
{
	const struct wifi_msg *msg = zbus_chan_const_msg(chan);

	if (msg->type == WIFI_SOFTAP_STARTED) {
		LOG_INF("WiFi SoftAP started, starting webserver...");

		/* Small delay to ensure network is fully configured */
		k_sleep(K_SECONDS(2));

		int ret = webserver_start();
		if (ret < 0) {
			LOG_ERR("Failed to start webserver: %d", ret);
		}
	}
}

ZBUS_LISTENER_DEFINE(wifi_event_listener_def, wifi_event_listener);

/* Extern reference to WiFi channel */
extern const struct zbus_channel WIFI_CHAN;
ZBUS_CHAN_ADD_OBS(WIFI_CHAN, wifi_event_listener_def, 0);

/* ============================================================================
 * MAIN APPLICATION
 * ============================================================================
 */

int main(void)
{
	struct net_if *iface = net_if_get_default();
	struct net_linkaddr *mac_addr = net_if_get_link_addr(iface);
	const char *board_name;

	/* Convert board name to proper case */
	if (strcmp(CONFIG_BOARD, "nrf7002dk") == 0 ||
	    strstr(CONFIG_BOARD, "nrf7002dk") != NULL) {
		board_name = "nRF7002DK";
	} else if (strcmp(CONFIG_BOARD, "nrf54lm20dk") == 0 ||
		   strstr(CONFIG_BOARD, "nrf54lm20dk") != NULL) {
		board_name = "nRF54LM20DK+nRF7002EBII";
	} else {
		board_name = CONFIG_BOARD;
	}

	LOG_INF("==============================================");
	LOG_INF("Nordic WiFi SoftAP Webserver");
	LOG_INF("==============================================");
	LOG_INF("Build: %s %s", __DATE__, __TIME__);
	LOG_INF("Board: %s", board_name);

	/* Print MAC address in the format "MAC: F4:CE:36:00:8B:E9" */
	if (mac_addr && mac_addr->len == 6) {
		LOG_INF("MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac_addr->addr[0],
			mac_addr->addr[1], mac_addr->addr[2], mac_addr->addr[3],
			mac_addr->addr[4], mac_addr->addr[5]);
	}

	LOG_INF("==============================================");

	LOG_INF("All modules initialized via SYS_INIT");
	LOG_INF("WiFi SoftAP will start automatically");
	LOG_INF("Connect to WiFi SSID: %s", CONFIG_APP_WIFI_SSID);
	LOG_INF("Use password configured in overlay-wifi-credentials.conf.");
	LOG_INF("Or default WiFi Password 12345678 for SSID nRF70-WebServer.");
	LOG_INF("Then browse to: http://192.168.7.1:%d", CONFIG_APP_HTTP_PORT);
	LOG_INF("==============================================");

	/* Main thread can sleep forever - everything runs in module threads */
	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
