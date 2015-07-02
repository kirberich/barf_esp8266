// Constants used both by the firmware and the barf library
#pragma once

#define METHOD_GET  0
#define METHOD_POST  1

#define LED_ACTIVITY  0
#define LED_CONNECTION  1
#define LED_OFF 2
#define LED_ON 3
#define LED_GPIO 4

#define ERROR "__err__"
#define TIMEOUT "__timeout__"
#define UNEXPECTED_COMMAND "__unexpected_command__"

#define COMMAND_DEBUG "debug"
#define COMMAND_METHOD "method"
#define COMMAND_NUM_FRAMENTS "num_fragments"
#define COMMAND_PATH_FRAGMENT "path_frament"
#define COMMAND_GET_VAR "get_var"
#define COMMAND_GET_VALUE "get_value"
#define COMMAND_REQUEST_RESPONSE "respond"
#define COMMAND_RESPONSE_START "response_start"
#define COMMAND_RESPONSE_END "response_end"

#define COMMAND_SSID "ssid"
#define COMMAND_PASSWORD "password"
#define COMMAND_CONNECT "connect"
#define COMMAND_DISCONNECT "disconnect"
#define COMMAND_TIMEOUT "timeout"
#define COMMAND_LED_MODE "led_mode"
#define COMMAND_GET "get"
#define COMMAND_POST "post"
#define COMMAND_ALLOW_GPIO "allow_gpio"
#define COMMAND_DISALLOW_GPIO "disallow_gpio"
#define COMMAND_BAUD_RATE "baud_rate"
#define COMMAND_IS_CONNECTED "is_connected"
#define COMMAND_GET_IP "get_ip"
