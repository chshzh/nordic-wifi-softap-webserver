/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file webserver.h
 * @brief Webserver module header
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize webserver module
 * @return 0 on success, negative error code on failure
 */
int webserver_module_init(void);

/**
 * @brief Start HTTP server
 * @return 0 on success, negative error code on failure
 */
int webserver_start(void);

#endif /* WEBSERVER_H */
