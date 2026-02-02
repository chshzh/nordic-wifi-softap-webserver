/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file wifi.h
 * @brief WiFi SoftAP module header
 */

#ifndef WIFI_H
#define WIFI_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize WiFi module
 * @return 0 on success, negative error code on failure
 */
int wifi_module_init(void);

/**
 * @brief Start WiFi SoftAP
 * @return 0 on success, negative error code on failure
 */
int wifi_start_softap(void);

#endif /* WIFI_H */
