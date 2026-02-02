/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize network event management module
 *
 * @return 0 on success, negative error code on failure
 */
int network_module_init(void);

/**
 * @brief Wait for network interface to be up
 *
 * @param timeout Maximum time to wait
 * @return 0 on success, negative error code on timeout
 */
int network_wait_for_iface_up(k_timeout_t timeout);

/**
 * @brief Wait for SoftAP to be enabled
 *
 * @param timeout Maximum time to wait
 * @return 0 on success, negative error code on timeout
 */
int network_wait_for_softap_ready(k_timeout_t timeout);

/**
 * @brief Wait for a station to connect
 *
 * @param timeout Maximum time to wait
 * @return 0 on success, negative error code on timeout
 */
int network_wait_for_station_connected(k_timeout_t timeout);

#endif /* NETWORK_H */
