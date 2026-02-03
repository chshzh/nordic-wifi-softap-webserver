/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include "modules/button/button.h"
#include "modules/led/led.h"
#include "modules/wifi/wifi.h"
#include "modules/webserver/webserver.h"
#include "modules/messages.h"

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
	LOG_INF("==============================================");
	LOG_INF("Nordic WiFi SoftAP Webserver");
	LOG_INF("==============================================");
	LOG_INF("Build: %s %s", __DATE__, __TIME__);
	LOG_INF("Board: %s", CONFIG_BOARD);
	LOG_INF("==============================================");

	LOG_INF("All modules initialized via SYS_INIT");
	LOG_INF("WiFi SoftAP will start automatically");
	LOG_INF("Connect to WiFi SSID: %s", CONFIG_APP_WIFI_SSID);
	LOG_INF("WiFi Password: %s", CONFIG_APP_WIFI_PASSWORD);
	LOG_INF("Then browse to: http://192.168.7.1:%d", CONFIG_APP_HTTP_PORT);
	LOG_INF("==============================================");

	/* Main thread can sleep forever - everything runs in module threads */
	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
