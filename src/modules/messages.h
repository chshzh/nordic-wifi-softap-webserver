/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file messages.h
 * @brief Common message definitions for all modules
 */

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* ==========================================================================
 * BOARD-SPECIFIC CAPABILITIES
 * ==========================================================================
 */

#if defined(CONFIG_BOARD_NRF7002DK_NRF5340_CPUAPP)
#define APP_NUM_BUTTONS 2
#define APP_NUM_LEDS    2
#define APP_BUTTON_LABELS                                                      \
	{                                                                      \
		"Button 1", "Button 2"                                         \
	}
#define APP_LED_LABELS                                                         \
	{                                                                      \
		"LED1", "LED2"                                                 \
	}
#elif defined(CONFIG_BOARD_NRF54LM20DK_NRF54LM20A_CPUAPP)
#define APP_NUM_BUTTONS 3
#define APP_NUM_LEDS    4
#define APP_BUTTON_LABELS                                                      \
	{                                                                      \
		"BUTTON0", "BUTTON1", "BUTTON2"                                \
	}
#define APP_LED_LABELS                                                         \
	{                                                                      \
		"LED0", "LED1", "LED2", "LED3"                                 \
	}
#else
#define APP_NUM_BUTTONS 4
#define APP_NUM_LEDS    4
#define APP_BUTTON_LABELS                                                      \
	{                                                                      \
		"Button 1", "Button 2", "Button 3", "Button 4"                 \
	}
#define APP_LED_LABELS                                                         \
	{                                                                      \
		"LED 1", "LED 2", "LED 3", "LED 4"                             \
	}
#endif

static inline const char *app_button_label(size_t index)
{
	static const char *const labels[] = APP_BUTTON_LABELS;
	BUILD_ASSERT(ARRAY_SIZE(labels) >= APP_NUM_BUTTONS);
	return (index < ARRAY_SIZE(labels)) ? labels[index] : "Button";
}

static inline const char *app_led_label(size_t index)
{
	static const char *const labels[] = APP_LED_LABELS;
	BUILD_ASSERT(ARRAY_SIZE(labels) >= APP_NUM_LEDS);
	return (index < ARRAY_SIZE(labels)) ? labels[index] : "LED";
}

/* ============================================================================
 * BUTTON MESSAGES
 * ============================================================================
 */

/**
 * @brief Button message types
 */
enum button_msg_type {
	BUTTON_PRESSED,  /**< Button pressed */
	BUTTON_RELEASED, /**< Button released */
};

/**
 * @brief Button message structure
 */
struct button_msg {
	enum button_msg_type type;
	uint8_t button_number;
	uint32_t press_count; /**< Total number of presses */
	uint32_t timestamp;
};

/* ============================================================================
 * LED MESSAGES
 * ============================================================================
 */

/**
 * @brief LED message types
 */
enum led_msg_type {
	LED_COMMAND_ON,     /**< Turn LED on */
	LED_COMMAND_OFF,    /**< Turn LED off */
	LED_COMMAND_TOGGLE, /**< Toggle LED */
};

/**
 * @brief LED message structure
 */
struct led_msg {
	enum led_msg_type type;
	uint8_t led_number;
};

/* ============================================================================
 * LED STATE MESSAGES (for status reporting)
 * ============================================================================
 */

/**
 * @brief LED state message structure
 */
struct led_state_msg {
	uint8_t led_number;
	bool is_on;
};

/* ============================================================================
 * WIFI MESSAGES
 * ============================================================================
 */

/**
 * @brief WiFi message types
 */
enum wifi_msg_type {
	WIFI_SOFTAP_STARTED,      /**< SoftAP started */
	WIFI_SOFTAP_STOPPED,      /**< SoftAP stopped */
	WIFI_CLIENT_CONNECTED,    /**< Client connected */
	WIFI_CLIENT_DISCONNECTED, /**< Client disconnected */
	WIFI_ERROR,               /**< WiFi error */
};

/**
 * @brief WiFi message structure
 */
struct wifi_msg {
	enum wifi_msg_type type;
	char ssid[32];
	uint8_t channel;
	int error_code;
};

/* ============================================================================
 * WEBSERVER MESSAGES
 * ============================================================================
 */

/**
 * @brief Webserver message types
 */
enum webserver_msg_type {
	WEBSERVER_STARTED,        /**< Webserver started */
	WEBSERVER_STOPPED,        /**< Webserver stopped */
	WEBSERVER_CLIENT_REQUEST, /**< Client request received */
};

/**
 * @brief Webserver message structure
 */
struct webserver_msg {
	enum webserver_msg_type type;
	uint32_t timestamp;
};

#endif /* MESSAGES_H */
