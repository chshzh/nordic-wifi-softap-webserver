/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "wifi.h"
#include "../messages.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_module, CONFIG_WIFI_MODULE_LOG_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/socket.h>
#include "net_private.h"

/* ============================================================================
 * ZBUS CHANNEL DEFINITION
 * ============================================================================ */

ZBUS_CHAN_DEFINE(WIFI_CHAN,
		 struct wifi_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0)
);

/* ============================================================================
 * STATE MACHINE CONTEXT
 * ============================================================================ */

/* Forward declarations */
static void wifi_idle_entry(void *obj);
static void wifi_starting_entry(void *obj);
static enum smf_state_result wifi_starting_run(void *obj);
static void wifi_active_entry(void *obj);
static enum smf_state_result wifi_active_run(void *obj);
static void wifi_error_entry(void *obj);

/* State table */
static const struct smf_state wifi_states[] = {
	[0] = SMF_CREATE_STATE(wifi_idle_entry, NULL, NULL, NULL, NULL),
	[1] = SMF_CREATE_STATE(wifi_starting_entry, wifi_starting_run, NULL, NULL, NULL),
	[2] = SMF_CREATE_STATE(wifi_active_entry, wifi_active_run, NULL, NULL, NULL),
	[3] = SMF_CREATE_STATE(wifi_error_entry, NULL, NULL, NULL, NULL),
};

/* WiFi state machine object */
struct wifi_sm_object {
	struct smf_ctx ctx;
	bool softap_ready;
	bool start_requested;
	int error_code;
};

static struct wifi_sm_object wifi_sm;

/* Network management callbacks */
static struct net_mgmt_event_callback wifi_mgmt_cb;
static struct net_mgmt_event_callback net_mgmt_cb;

/* ============================================================================
 * STATE MACHINE IMPLEMENTATIONS
 * ============================================================================ */

static void wifi_idle_entry(void *obj)
{
	LOG_INF("WiFi in IDLE state");
}

static void wifi_starting_entry(void *obj)
{
	struct wifi_sm_object *sm = (struct wifi_sm_object *)obj;
	int ret;
	
	LOG_INF("Starting WiFi SoftAP...");
	
	struct net_if *iface = net_if_get_first_wifi();
	if (!iface) {
		LOG_ERR("No WiFi interface found");
		sm->error_code = -ENODEV;
		smf_set_state(SMF_CTX(sm), &wifi_states[3]);
		return;
	}
	
	/* Set regulatory domain */
	struct wifi_reg_domain regd = {0};
	regd.oper = WIFI_MGMT_SET;
	strncpy(regd.country_code, "US", WIFI_COUNTRY_CODE_LEN + 1);
	ret = net_mgmt(NET_REQUEST_WIFI_REG_DOMAIN, iface, &regd, sizeof(regd));
	if (ret) {
		LOG_WRN("Failed to set regulatory domain: %d", ret);
		/* Continue anyway, not fatal */
	}
	
	/* Setup DHCP server */
	struct in_addr pool_start;
	ret = inet_pton(AF_INET, "192.168.7.2", &pool_start);
	if (ret == 1) {
		ret = net_dhcpv4_server_start(iface, &pool_start);
		if (ret == -EALREADY) {
			LOG_INF("DHCP server already running");
		} else if (ret < 0) {
			LOG_ERR("Failed to start DHCP server: %d", ret);
		} else {
			LOG_INF("DHCP server started");
		}
	}
	
	/* Start SoftAP */
	struct wifi_connect_req_params params = {
		.ssid = (uint8_t *)CONFIG_APP_WIFI_SSID,
		.ssid_length = strlen(CONFIG_APP_WIFI_SSID),
		.psk = (uint8_t *)CONFIG_APP_WIFI_PASSWORD,
		.psk_length = strlen(CONFIG_APP_WIFI_PASSWORD),
		.security = WIFI_SECURITY_TYPE_PSK,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.channel = 1,
	};
	
	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params,
		       sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("Failed to enable SoftAP: %d", ret);
		sm->error_code = ret;
		smf_set_state(SMF_CTX(sm), &wifi_states[3]);
		return;
	}
	
	LOG_INF("SoftAP enable requested: SSID='%s'", CONFIG_APP_WIFI_SSID);
	
	/* Message will be published when SoftAP is actually started (in event handler) */
}

static enum smf_state_result wifi_starting_run(void *obj)
{
	struct wifi_sm_object *sm = (struct wifi_sm_object *)obj;
	
	if (sm->softap_ready) {
		smf_set_state(SMF_CTX(sm), &wifi_states[2]);
	}
	
	return SMF_EVENT_HANDLED;
}

static void wifi_active_entry(void *obj)
{
	struct wifi_msg msg;
	
	LOG_INF("WiFi SoftAP active");
	
	msg.type = WIFI_SOFTAP_STARTED;
	snprintf(msg.ssid, sizeof(msg.ssid), "%s", CONFIG_APP_WIFI_SSID);
	msg.channel = 0;
	msg.error_code = 0;
	
	zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
}

static enum smf_state_result wifi_active_run(void *obj)
{
	/* Stay in active state */
	return SMF_EVENT_HANDLED;
}

static void wifi_error_entry(void *obj)
{
	struct wifi_sm_object *sm = (struct wifi_sm_object *)obj;
	struct wifi_msg msg;
	
	LOG_ERR("WiFi error: %d", sm->error_code);
	
	msg.type = WIFI_ERROR;
	msg.error_code = sm->error_code;
	
	zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
}

/* ============================================================================
 * NETWORK EVENT HANDLERS
 * ============================================================================ */

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(iface);
	
	switch (mgmt_event) {
	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		const struct wifi_status *status = 
			(const struct wifi_status *)cb->info;
		
		if (status->status == 0) {
			LOG_INF("SoftAP enabled successfully");
			wifi_sm.softap_ready = true;
			smf_run_state(SMF_CTX(&wifi_sm));
		} else {
			LOG_ERR("SoftAP enable failed: %d", status->status);
			wifi_sm.error_code = status->status;
			smf_set_state(SMF_CTX(&wifi_sm), &wifi_states[3]);
			smf_run_state(SMF_CTX(&wifi_sm));
		}
		break;
	}
	
	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		const struct wifi_ap_sta_info *sta_info = 
			(const struct wifi_ap_sta_info *)cb->info;
		LOG_INF("Station connected: %02x:%02x:%02x:%02x:%02x:%02x",
			sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
			sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		
		struct wifi_msg msg = {
			.type = WIFI_CLIENT_CONNECTED,
		};
		zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		break;
	}
	
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		const struct wifi_ap_sta_info *sta_info = 
			(const struct wifi_ap_sta_info *)cb->info;
		LOG_INF("Station disconnected: %02x:%02x:%02x:%02x:%02x:%02x",
			sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
			sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		
		struct wifi_msg msg = {
			.type = WIFI_CLIENT_DISCONNECTED,
		};
		zbus_chan_pub(&WIFI_CHAN, &msg, K_NO_WAIT);
		break;
	}
	
	default:
		break;
	}
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				   uint64_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);
	
	switch (mgmt_event) {
	case NET_EVENT_IPV4_ADDR_ADD:
		LOG_INF("IPv4 address added");
		break;
		
	default:
		break;
	}
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================ */

int wifi_start_softap(void)
{
	wifi_sm.start_requested = true;
	smf_set_state(SMF_CTX(&wifi_sm), &wifi_states[1]);
	return smf_run_state(SMF_CTX(&wifi_sm));
}

/* ============================================================================
 * WiFi THREAD
 * ============================================================================ */

static void wifi_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	
	LOG_INF("WiFi module thread started");
	
	/* Give system time to initialize */
	k_sleep(K_SECONDS(2));
	
	/* Start SoftAP */
	wifi_start_softap();
	
	while (1) {
		k_sleep(K_SECONDS(10));
	}
}

/* Thread stack size increased for SoftAP mode with WPA supplicant */
K_THREAD_DEFINE(wifi_thread_id, 8192, wifi_thread_fn, NULL, NULL, NULL, 5, 0, 0);

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================ */

int wifi_module_init(void)
{
	LOG_INF("Initializing WiFi module");
	
	/* Initialize state machine */
	wifi_sm.softap_ready = false;
	wifi_sm.start_requested = false;
	wifi_sm.error_code = 0;
	smf_set_initial(SMF_CTX(&wifi_sm), &wifi_states[0]);
	
	/* Setup network management callbacks */
	net_mgmt_init_event_callback(&wifi_mgmt_cb,
				     wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_AP_ENABLE_RESULT |
				     NET_EVENT_WIFI_AP_STA_CONNECTED |
				     NET_EVENT_WIFI_AP_STA_DISCONNECTED);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);
	
	net_mgmt_init_event_callback(&net_mgmt_cb,
				     net_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&net_mgmt_cb);
	
	LOG_INF("WiFi module initialized");
	
	return 0;
}

SYS_INIT(wifi_module_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
