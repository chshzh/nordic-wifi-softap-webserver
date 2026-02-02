/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "network.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(network_module, CONFIG_NETWORK_MODULE_LOG_LEVEL);

/* Event masks */
#define L2_IF_EVENT_MASK (NET_EVENT_IF_DOWN | NET_EVENT_IF_UP)
#define L2_SOFTAP_EVENT_MASK \
	(NET_EVENT_WIFI_AP_ENABLE_RESULT | \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | \
	 NET_EVENT_WIFI_AP_STA_DISCONNECTED)

/* Semaphores for network events */
static K_SEM_DEFINE(iface_up_sem, 0, 1);
static K_SEM_DEFINE(softap_ready_sem, 0, 1);
static K_SEM_DEFINE(station_connected_sem, 0, 1);

/* Network event callbacks */
static struct net_mgmt_event_callback iface_event_cb;
static struct net_mgmt_event_callback softap_event_cb;

/* Station tracking */
#define MAX_SOFTAP_STATIONS 4

struct softap_station {
	bool valid;
	uint8_t mac[6];
	struct in_addr ip_addr;
};

static struct softap_station connected_stations[MAX_SOFTAP_STATIONS];
static K_MUTEX_DEFINE(station_mutex);

static void iface_event_handler(struct net_mgmt_event_callback *cb,
				uint64_t mgmt_event,
				struct net_if *iface)
{
	char ifname[IFNAMSIZ + 1] = {0};

	switch (mgmt_event) {
	case NET_EVENT_IF_UP:
		net_if_get_name(iface, ifname, sizeof(ifname) - 1);
		LOG_INF("Network interface %s is UP", ifname);
		k_sem_give(&iface_up_sem);
		break;

	case NET_EVENT_IF_DOWN:
		net_if_get_name(iface, ifname, sizeof(ifname) - 1);
		LOG_INF("Network interface %s is DOWN", ifname);
		break;

	default:
		break;
	}
}

static void softap_event_handler(struct net_mgmt_event_callback *cb,
				 uint64_t mgmt_event,
				 struct net_if *iface)
{
	const struct wifi_status *status;
	const struct wifi_ap_sta_info *sta_info;
	char mac_str[18];
	int slot = -1;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_AP_ENABLE_RESULT:
		status = (const struct wifi_status *)cb->info;
		if (status->status == 0) {
			LOG_INF("SoftAP enabled successfully");
			k_sem_give(&softap_ready_sem);
		} else {
			LOG_ERR("SoftAP enable failed: %d", status->status);
		}
		break;

	case NET_EVENT_WIFI_AP_STA_CONNECTED:
		sta_info = (const struct wifi_ap_sta_info *)cb->info;
		
		snprintf(mac_str, sizeof(mac_str), 
			 "%02x:%02x:%02x:%02x:%02x:%02x",
			 sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
			 sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		
		LOG_INF("Station connected: %s", mac_str);
		
		/* Find empty slot */
		k_mutex_lock(&station_mutex, K_FOREVER);
		for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
			if (!connected_stations[i].valid) {
				connected_stations[i].valid = true;
				memcpy(connected_stations[i].mac, sta_info->mac, 6);
				slot = i;
				break;
			}
		}
		k_mutex_unlock(&station_mutex);
		
		if (slot >= 0) {
			LOG_DBG("Station stored in slot %d", slot);
		}
		
		k_sem_give(&station_connected_sem);
		break;

	case NET_EVENT_WIFI_AP_STA_DISCONNECTED:
		sta_info = (const struct wifi_ap_sta_info *)cb->info;
		
		snprintf(mac_str, sizeof(mac_str),
			 "%02x:%02x:%02x:%02x:%02x:%02x",
			 sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
			 sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		
		LOG_INF("Station disconnected: %s", mac_str);
		
		/* Remove from tracking */
		k_mutex_lock(&station_mutex, K_FOREVER);
		for (int i = 0; i < MAX_SOFTAP_STATIONS; i++) {
			if (connected_stations[i].valid &&
			    memcmp(connected_stations[i].mac, sta_info->mac, 6) == 0) {
				connected_stations[i].valid = false;
				memset(&connected_stations[i], 0, sizeof(struct softap_station));
				break;
			}
		}
		k_mutex_unlock(&station_mutex);
		break;

	default:
		break;
	}
}

int network_wait_for_iface_up(k_timeout_t timeout)
{
	return k_sem_take(&iface_up_sem, timeout);
}

int network_wait_for_softap_ready(k_timeout_t timeout)
{
	return k_sem_take(&softap_ready_sem, timeout);
}

int network_wait_for_station_connected(k_timeout_t timeout)
{
	return k_sem_take(&station_connected_sem, timeout);
}

int network_module_init(void)
{
	LOG_INF("Initializing network module");

	/* Initialize station tracking */
	memset(connected_stations, 0, sizeof(connected_stations));

	/* Register interface event callbacks */
	net_mgmt_init_event_callback(&iface_event_cb, iface_event_handler,
				     L2_IF_EVENT_MASK);
	net_mgmt_add_event_callback(&iface_event_cb);

	/* Register SoftAP event callbacks */
	net_mgmt_init_event_callback(&softap_event_cb, softap_event_handler,
				     L2_SOFTAP_EVENT_MASK);
	net_mgmt_add_event_callback(&softap_event_cb);

	LOG_INF("Network module initialized");
	return 0;
}

SYS_INIT(network_module_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
