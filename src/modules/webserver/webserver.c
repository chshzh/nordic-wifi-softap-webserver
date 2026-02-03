/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "webserver.h"
#include "../button/button.h"
#include "../led/led.h"
#include "../messages.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(webserver_module, CONFIG_WEBSERVER_MODULE_LOG_LEVEL);

#include <stdio.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/net/http/service.h>
#include <zephyr/smf.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

#define NUM_BUTTONS APP_NUM_BUTTONS
#define NUM_LEDS    APP_NUM_LEDS

BUILD_ASSERT(NUM_BUTTONS > 0, "At least one button expected");
BUILD_ASSERT(NUM_LEDS > 0, "At least one LED expected");

/* ============================================================================
 * BUTTON STATE TRACKING (via Zbus)
 * ============================================================================
 */

struct button_state {
	bool is_pressed;
	uint32_t press_count;
	uint8_t button_number;
};

static struct button_state button_states[NUM_BUTTONS];

static void button_listener(const struct zbus_channel *chan)
{
	const struct button_msg *msg = zbus_chan_const_msg(chan);

	if (msg->button_number < NUM_BUTTONS) {
		int idx = msg->button_number;
		button_states[idx].button_number = msg->button_number;
		button_states[idx].press_count = msg->press_count;
		button_states[idx].is_pressed = (msg->type == BUTTON_PRESSED);

		LOG_DBG("Button %d state updated: %s, count=%d",
			msg->button_number,
			button_states[idx].is_pressed ? "pressed" : "released",
			button_states[idx].press_count);
	}
}

ZBUS_LISTENER_DEFINE(button_listener_def, button_listener);

/* Extern reference to channels */
extern const struct zbus_channel BUTTON_CHAN;
extern const struct zbus_channel LED_CMD_CHAN;
ZBUS_CHAN_ADD_OBS(BUTTON_CHAN, button_listener_def, 0);

/* ============================================================================
 * HTTP SERVICE DEFINITION
 * ============================================================================
 */

static uint16_t http_service_port = CONFIG_APP_HTTP_PORT;
HTTP_SERVICE_DEFINE(webserver_service, NULL, &http_service_port, 1, 10, NULL,
		    NULL, NULL);

/* ============================================================================
 * STATIC WEB RESOURCES
 * ============================================================================
 */

/* Index HTML */
static const uint8_t index_html_gz[] = {
#include "index.html.gz.inc"
};

struct http_resource_detail_static index_html_resource_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	/* clang-format on */
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};

HTTP_RESOURCE_DEFINE(index_html_resource, webserver_service, "/",
		     &index_html_resource_detail);

/* Main JS */
static const uint8_t main_js_gz[] = {
#include "main.js.gz.inc"
};

struct http_resource_detail_static main_js_resource_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "application/javascript",
		},
	/* clang-format on */
	.static_data = main_js_gz,
	.static_data_len = sizeof(main_js_gz),
};

HTTP_RESOURCE_DEFINE(main_js_resource, webserver_service, "/main.js",
		     &main_js_resource_detail);

/* Styles CSS */
static const uint8_t styles_css_gz[] = {
#include "styles.css.gz.inc"
};

struct http_resource_detail_static styles_css_resource_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/css",
		},
	/* clang-format on */
	.static_data = styles_css_gz,
	.static_data_len = sizeof(styles_css_gz),
};

HTTP_RESOURCE_DEFINE(styles_css_resource, webserver_service, "/styles.css",
		     &styles_css_resource_detail);

/* ============================================================================
 * DYNAMIC API ENDPOINTS
 * ============================================================================
 */

/* GET /api/buttons - Get button states */
static uint8_t button_api_buf[512];

static int button_api_handler(struct http_client_ctx *client,
			      enum http_data_status status,
			      const struct http_request_ctx *request_ctx,
			      struct http_response_ctx *response_ctx,
			      void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(request_ctx);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	int offset = 0;
	int remaining = sizeof(button_api_buf);
	int written = snprintf((char *)button_api_buf + offset, remaining,
			       "{\"buttons\":[");
	if (written < 0 || written >= remaining) {
		return -ENOMEM;
	}
	offset += written;
	remaining -= written;

	for (int i = 0; i < NUM_BUTTONS; i++) {
		const bool is_last = (i == NUM_BUTTONS - 1);
		const uint8_t button_number = button_states[i].button_number;
		const char *button_name = app_button_label(button_number);

		written = snprintf(
			(char *)button_api_buf + offset, remaining,
			/* clang-format off */
			"{\"number\":%u,\"name\":\"%s\",\"pressed\":%s, \"count\":%u}%s",
			/* clang-format on */
			button_number, button_name ? button_name : "",
			button_states[i].is_pressed ? "true" : "false",
			button_states[i].press_count, is_last ? "" : ",");
		if (written < 0 || written >= remaining) {
			return -ENOMEM;
		}
		offset += written;
		remaining -= written;
	}

	written = snprintf((char *)button_api_buf + offset, remaining, "]}");
	if (written < 0 || written >= remaining) {
		return -ENOMEM;
	}
	offset += written;

	response_ctx->body = button_api_buf;
	response_ctx->body_len = offset;
	response_ctx->final_chunk = true;
	response_ctx->status = HTTP_200_OK;

	return 0;
}

static struct http_resource_detail_dynamic button_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "application/json",
		},
	/* clang-format on */
	.cb = button_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(button_api_resource, webserver_service, "/api/buttons",
		     &button_api_detail);

/* GET /api/leds - Get LED states */
static uint8_t led_get_api_buf[512];

static int led_get_api_handler(struct http_client_ctx *client,
			       enum http_data_status status,
			       const struct http_request_ctx *request_ctx,
			       struct http_response_ctx *response_ctx,
			       void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(request_ctx);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	/* Get LED states */
	int written = led_get_all_states_json((char *)led_get_api_buf,
					      sizeof(led_get_api_buf));

	if (written > 0) {
		response_ctx->body = led_get_api_buf;
		response_ctx->body_len = written;
		response_ctx->final_chunk = true;
		response_ctx->status = HTTP_200_OK;
	}

	return 0;
}

static struct http_resource_detail_dynamic led_get_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "application/json",
		},
	/* clang-format on */
	.cb = led_get_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(led_get_api_resource, webserver_service, "/api/leds",
		     &led_get_api_detail);

/* POST /api/led - Control LED */
struct led_control_cmd {
	uint8_t led;
	char action[8]; /* "on", "off", "toggle" */
};

static const struct json_obj_descr led_control_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct led_control_cmd, led, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct led_control_cmd, action,
			    JSON_TOK_STRING_BUF),
};

static int led_post_api_handler(struct http_client_ctx *client,
				enum http_data_status status,
				const struct http_request_ctx *request_ctx,
				struct http_response_ctx *response_ctx,
				void *user_data)
{
	ARG_UNUSED(client);
	ARG_UNUSED(user_data);

	if (status != HTTP_SERVER_DATA_FINAL) {
		return 0;
	}

	if (request_ctx->data == NULL || request_ctx->data_len == 0) {
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	struct led_control_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));
	int ret = json_obj_parse((char *)request_ctx->data,
				 request_ctx->data_len, led_control_descr,
				 ARRAY_SIZE(led_control_descr), &cmd);

	if (ret < 0) {
		LOG_WRN("Failed to parse LED command: %d", ret);
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	LOG_INF("LED control: LED %d, action='%s'", cmd.led, cmd.action);

	if (cmd.led >= NUM_LEDS) {
		LOG_WRN("LED command out of range: %d (max: %d)", cmd.led,
			NUM_LEDS - 1);
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	/* Publish LED command via Zbus */
	struct led_msg msg;
	msg.led_number = cmd.led;

	if (strcmp(cmd.action, "on") == 0) {
		msg.type = LED_COMMAND_ON;
	} else if (strcmp(cmd.action, "off") == 0) {
		msg.type = LED_COMMAND_OFF;
	} else if (strcmp(cmd.action, "toggle") == 0) {
		msg.type = LED_COMMAND_TOGGLE;
	} else {
		LOG_WRN("Unknown LED action: %s", cmd.action);
		response_ctx->status = HTTP_400_BAD_REQUEST;
		response_ctx->final_chunk = true;
		return 0;
	}

	ret = zbus_chan_pub(&LED_CMD_CHAN, &msg, K_MSEC(100));
	if (ret < 0) {
		LOG_ERR("Failed to publish LED command: %d", ret);
		response_ctx->status = HTTP_500_INTERNAL_SERVER_ERROR;
	} else {
		response_ctx->status = HTTP_200_OK;
	}

	response_ctx->final_chunk = true;
	return 0;
}

static struct http_resource_detail_dynamic led_post_api_detail = {
	/* clang-format off */
	.common = {
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_POST),
		},
	/* clang-format on */
	.cb = led_post_api_handler,
	.holder = NULL,
	.user_data = NULL,
};

HTTP_RESOURCE_DEFINE(led_post_api_resource, webserver_service, "/api/led",
		     &led_post_api_detail);

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

int webserver_start(void)
{
	LOG_INF("Starting HTTP server on port %d", CONFIG_APP_HTTP_PORT);

	int ret = http_server_start();
	if (ret < 0) {
		LOG_ERR("Failed to start HTTP server: %d", ret);
		return ret;
	}

	LOG_INF("HTTP server started successfully");
	LOG_INF("Access the web interface at: http://192.168.7.1:%d",
		CONFIG_APP_HTTP_PORT);

	return 0;
}

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================
 */

int webserver_module_init(void)
{
	LOG_INF("Initializing webserver module");

	/* Initialize button states */
	for (int i = 0; i < NUM_BUTTONS; i++) {
		button_states[i].button_number = i;
		button_states[i].is_pressed = false;
		button_states[i].press_count = 0;
	}

	LOG_INF("Webserver module initialized");

	return 0;
}

SYS_INIT(webserver_module_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
