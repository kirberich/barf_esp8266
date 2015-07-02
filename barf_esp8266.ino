/* This sketch implements a user-friendly though slightly ridiculous serial-wifi bridge for an ESP8266 chip.
	 Upload this sketch to an ESP8266, connect the RX and TX pins to (software or hardware) pins of an arduino
	 and use the barf library on arduino for super-easy bidirectional sending of statuses and commands over wifi.
 */
#include <ESP8266WiFi.h>
#include "barf_constants.h"
#include "LinkedList.h"

String ssid;
String password;
WiFiServer server(80);
unsigned int server_timeout = 1000;
int led_mode = LED_ACTIVITY;
unsigned long last_activity = 0;
bool allow_gpio = true;
int baud_rate = 9600;

struct RequestVar {
	String name;
	String value;
};

String format_ip(IPAddress address) {
	return String(address[0]) + "." + String(address[1]) + "." + String(address[2]) + "." + String(address[3]);
}

void update_led() {
	if (led_mode == LED_GPIO) {
		// Leave the led alone if it's being used as a GPIO pin
		return;
	}

	bool is_on = false;

	if (led_mode == LED_ACTIVITY) {
		is_on = millis() - last_activity < 500;
	} else if (led_mode == LED_CONNECTION) {
		is_on = is_connected();
	} else if (led_mode == LED_OFF) {
		is_on = false;
	} else if (led_mode == LED_ON) {
		is_on = true;
	}
	digitalWrite(2, is_on);
}

void disconnect() {
	WiFi.disconnect();
}

void connect(unsigned int timeout) {
	if (!ssid) {
		send_data(COMMAND_DEBUG, String("No ssid."));
		return;
	}

	if (password) {
		WiFi.begin(ssid.c_str(), password.c_str());
	} else {
		WiFi.begin(ssid.c_str());
	}

	// If a timeout of 0 is used this might try forever, locking up the code.
	unsigned long before = millis();
	while (is_connected()) {
		if (timeout && millis() - before > timeout) {
			return;
		}
		delay(200);
	}

	server.begin();
}

bool connect() {
	connect(1000);
}

String read_line(unsigned long timeout) {
	unsigned long begin = millis();

	while(!Serial.available()) {
		if (millis() - begin > timeout) {
			return TIMEOUT;
		}
	}

	String line;

	char c;
	while (true) {
		if (Serial.available()) {
			c = Serial.read();

			if (c == '\n') {
				break;
			}

			line += c;
		}

		if (millis() - begin > timeout) {
			return TIMEOUT;
		}
	}

	return line;
}

String read_line() {
	return read_line(1000);
}

void send_data(String command_type, String value) {
	Serial.print(command_type + String(" ") + value + String('\n'));
}

void send_data(String command_type) {
	Serial.print(command_type + String('\n'));
}

String request_response() {
	// Send command asking for a response, then wait around for one
	send_data(COMMAND_REQUEST_RESPONSE);
	return read_line();
}

void setup() {
	Serial.begin(baud_rate);
	delay(10);

	// prepare GPIO2, for a status led
	pinMode(2, OUTPUT);
	digitalWrite(2, 0);

	// The chip remembers its last ssid and password, so generally just running the server will make it reachable,
	// even without reconfiguring.
	server.begin();
}

bool is_connected() {
	return WiFi.status() == WL_CONNECTED;
}

void handle_request() {
	// Check if a client has connected
	WiFiClient client = server.available();
	if (!client) {
		return;
	}

	// Wait until the client sends some data
	// If the client doesn't send any data quickly enough, just bail.
	unsigned int before = millis();
	while(!client.available()){
		delay(1);
		if (millis() - before > server_timeout) {
			client.stop();
			return;
		}
	}

	// Read the first line of the request
	String req = client.readStringUntil('\r');
	client.flush();

	// Get the request method and remove method bit from req string
	String method = req.substring(0, req.indexOf(" "));
	req = req.substring(req.indexOf(" ")+1);

	// Get the resource path and transmit it in parts
	String resource_path = req.substring(0, req.indexOf(" "));
	req = req.substring(req.indexOf(" ")+1);

	String path_without_args = resource_path;
	String query_string;
	int query_string_index = resource_path.indexOf("?");
	if (query_string_index != -1) {
		path_without_args = resource_path.substring(0, query_string_index);
		query_string = resource_path.substring(query_string_index);
	}

	// Add a trailing slash for easier parsing
	if (path_without_args[path_without_args.length() - 1] != '/') {
		path_without_args = path_without_args += "/";
	}

	// Get path segments
	String chopped_path = path_without_args.substring(1);
	LinkedList<String> fragments;
	while(true) {
		int next_slash = chopped_path.indexOf("/");
		if (next_slash != -1) {
			String fragment = chopped_path.substring(0, next_slash);
			fragments.add(fragment);
			chopped_path = chopped_path.substring(next_slash+1);
		} else {
			break;
		}
	}

	// Add a trailing & for easier parsing
	if (query_string[query_string.length() - 1] != '&') {
		query_string += "&";
	}

	// Get GET variables
	String chopped_query_string = query_string.substring(1);
	LinkedList<RequestVar> get_vars;
	while(true) {
		int next_var = chopped_query_string.indexOf("&");
		if (next_var != -1) {
			String var_value = chopped_query_string.substring(0, next_var);
			String var = var_value;
			String value;
			int equals_index = var.indexOf("=");
			if (equals_index != -1) {
				var = var_value.substring(0, equals_index);
				value = var_value.substring(equals_index+1);
			} else {
				value = "";
			}
			get_vars.add({var, value});

			chopped_query_string = chopped_query_string.substring(next_var+1);
		} else {
			break;
		}
	}
	client.flush();

	String response_data;
	int status_code = 200;
	String status_reason = "OK";

	if (fragments.get(0) == "gpio" && allow_gpio) {
		// Handle GPIO request
		int pin_number = fragments.get(1).toInt();
		if (method == "GET" && fragments.size() != 2) {
			status_code = 400;
			response_data = "GET requests need to be in the form /gpio/<pin>";
		} else if (method == "POST" && fragments.size() != 3) {
			status_code = 400;
			response_data = "POST requests need to be in the form /gpio/<pin>/<value>";
		}

		if (method == "GET") {
			response_data = String(digitalRead(pin_number));
		} else if (fragments.get(2) == "input") {
			pinMode(pin_number, INPUT);
			response_data = String("Configured pin ") + fragments.get(1) + String(" as INPUT");
		} else if (fragments.get(2) == "output") {
			pinMode(pin_number, OUTPUT);
			response_data = String("Configured pin ") + fragments.get(1) + String(" as OUTPUT");
		} else {
			analogWrite(pin_number, fragments.get(2).toInt());
			response_data = String("Set pin ") + fragments.get(1) + String(" to ") + fragments.get(2);
		}
	} else {
		// Send data over serial
		send_data(COMMAND_METHOD, method);
		send_data(COMMAND_NUM_FRAMENTS, String(fragments.size()));
		for(int i=0; i<fragments.size(); ++i) {
			send_data(COMMAND_PATH_FRAGMENT, fragments.get(i));
		}
		for(int i=0; i < get_vars.size(); ++i) {
			RequestVar var = get_vars.get(i);
			send_data(COMMAND_GET_VAR, var.name);
			send_data(COMMAND_GET_VALUE, var.value);
		}

		// All data's been sent (we're ignoring the request body for now), now send a command to get any response data
		response_data = request_response();

		if (response_data.substring(0, 7) == "status:") {
			status_code = response_data.substring(7, response_data.indexOf(" ")).toInt();
			response_data = response_data.substring(7);
		} else if (!response_data.length()) {
			status_code = 404;
		} else if (response_data == TIMEOUT) {
			response_data = "";
			status_code = 504;
		}
	}

	// It'd be better if the client just supplied the reason, but I can't be bothered now.
	if (status_code == 400) {
		status_reason = "Bad Request";
	} else if (status_code == 404) {
		status_reason = "Not Found";
	} else if (status_code == 401) {
		status_reason = "Unauthorized";
	} else if (status_code == 403) {
		status_reason = "Forbidden";
	} else if (status_code == 418) {
		status_reason = "I'm a teapot";
	} else if (status_code == 500) {
		status_reason = "Internal Server Error";
	} else if (status_code == 504) {
		status_reason = "Gateway Timeout";
	} else {
		status_reason = "";
	}

	// Prepare the response
	String s = String("HTTP/1.1 " + String(status_code) + " " + status_reason + "\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>") + response_data + String("</html>\n");

	// Send the response to the client
	client.print(s);
	client.stop();

	// Update activity timestamp
	last_activity = millis();
}

void handle_serial() {
	while(Serial.available()) {
		String line = Serial.readStringUntil('\n');

		String command = line;
		String args;
		int first_space = line.indexOf(" ");
		if (first_space != -1) {
			command = line.substring(0, first_space);
			args = line.substring(first_space+1);
		}

		if (command == COMMAND_DEBUG){
			send_data(String("ssid"), ssid);
			send_data(String("password"), password);
			send_data(String("connected"), String(is_connected()));
			send_data(String("led_mode"), String(led_mode));
			send_data(String("baud rate"), String(baud_rate));
			send_data(String("ip"), format_ip(WiFi.localIP()));
		} else if (command == COMMAND_TIMEOUT) {
			// timeout <ms>
			server_timeout = args.toInt();
		} else if (command == COMMAND_SSID) {
			ssid = args;
		} else if (command == COMMAND_PASSWORD) {
			password = args;
		} else if (command == COMMAND_CONNECT) {
			connect();
		} else if (command == COMMAND_IS_CONNECTED) {
			send_data(COMMAND_IS_CONNECTED, String(is_connected()));
		} else if (command == COMMAND_DISCONNECT) {
			disconnect();
		} else if (command == COMMAND_GET_IP) {
			send_data(COMMAND_GET_IP, format_ip(WiFi.localIP()));
		} else if (command == COMMAND_LED_MODE) {
			// led_mode <mode>
			led_mode = args.toInt();
		} else if (command == COMMAND_ALLOW_GPIO || command == COMMAND_DISALLOW_GPIO) {
			allow_gpio = command == COMMAND_ALLOW_GPIO;
		} else if (command == COMMAND_BAUD_RATE)  {
			// baud_rate <new_rate>
			baud_rate = args.toInt();
			Serial.begin(baud_rate);
		} else if (command == COMMAND_GET || command == COMMAND_POST) {
			// get <address>:<port>/<path> or get <address/<path>

			// Filter out protocol. Find first colon, check if it's got // after it, if yes, remove everything up to that point.
			int colon_index = args.indexOf(':');
			if (args.length() > colon_index + 3 && colon_index != -1 && args[colon_index + 1] == '/' && args[colon_index + 2] == '/') {
				args = args.substring(colon_index + 3);
			}

			String method = command == COMMAND_POST ? "POST" : "GET";

			int port_start = args.indexOf(":");
			int path_start = args.indexOf("/");
			int port = 80;
			String address;
			String path;

			if (port_start != -1) {
				address = args.substring(0, port_start);

				if (path_start != -1) {
					port = args.substring(port_start+1, path_start).toInt();
					path = args.substring(path_start);
				} else {
					port = args.substring(port_start+1).toInt();
					path = "/";
				}
			} else {
				port = 80;
				if (path_start != -1) {
					address = args.substring(0, path_start);
					path = args.substring(path_start);
				} else {
					address = args;
					path = "/";
				}
			}
			WiFiClient client;
			client.connect(address.c_str(), port);
			client.print(method + String(" ") + path + String(" HTTP/1.1") + String('\n'));
			client.print(String("Host: ") + address + String(":") + String(port) + String('\n'));
			client.print('\n');


			// Get response
			String response = client.readString();

			// Send the data to the client, flanked by response_start and response_end commands
			send_data(COMMAND_RESPONSE_START);
			Serial.print(response);
			send_data(COMMAND_RESPONSE_END);
		}

		last_activity = millis();
	}
}

void loop() {
	handle_serial();
	handle_request();
	delay(1);

	update_led();
}
