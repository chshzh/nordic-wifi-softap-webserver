/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file button.h
 * @brief Button module header
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize button module
 * @return 0 on success, negative error code on failure
 */
int button_module_init(void);

#endif /* BUTTON_H */
