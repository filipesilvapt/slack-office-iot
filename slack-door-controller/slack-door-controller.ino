/*
  #--- Debounce
  Each time the input pin goes from LOW to HIGH (e.g. because of a push-button
  press), the output pin is toggled from LOW to HIGH or HIGH to LOW.  There's
  a minimum delay between toggles to debounce the circuit (i.e. to ignore
  noise).
  http://www.arduino.cc/en/Tutorial/Debounce
  #---
*/

/**
   Arduino IDE v1.8.9
*/
#include <Arduino.h>

#include <stdio.h>

/*
  ESP8266 Boards Manager v2.5.2
  Add the following line to the Arduino preferences additional boards manager URLs:
  http://arduino.esp8266.com/stable/package_esp8266com_index.json
  Go to the boards manager and install the esp8266 by ESP8266 Community
*/
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

/*
   WebSockets by Markus Sattler v2.1.4
   https://github.com/Links2004/arduinoWebSockets
*/
#include <WebSocketsClient.h>

/*
   ArduinoJson by Benoît Blanchon v6.11.4
   https://arduinojson.org/
   https://arduinojson.org/v6/doc/upgrade/
*/
#include <ArduinoJson.h>

/*
   QueueList by Efstathios Chatzikyriakidis v1.0 2010-09-28
   (External Lib) https://playground.arduino.cc/Code/QueueList
*/
#include <QueueList.h>

/*
   Configurations that are specific to each controller node
*/
#include "ControllerConfigs.h"

/*
   Specific board configurations and some general ones
*/
#include "BoardConfigs.h"


ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool isConnectedToSlack = false;
bool isReedSwitchConnected = false;
bool isReedSwitchConnectedRecently = false;
bool isReedHigh = false;
bool isReedLow = false;
bool isReedTriggerSent = true;
bool isReedConnectionChanged = false;
int reedSwitchState = LOW;
bool isFirstRun = true;

struct SLACK_MESSAGE {
  char type;
  //const char *channel;
  //const char *userID;
};

QueueList <SLACK_MESSAGE> queue;

unsigned long lastHeapMsg = 0;
unsigned long lastPing = 0;
unsigned long lastSlackConnectionRequest = 0;
SLACK_MESSAGE currentMsg;

/**
  Sends a ping message to Slack. Call this function immediately after establishing
  the WebSocket connection, and then every 5 seconds to keep the connection alive.
*/
void sendPing() {
  DynamicJsonDocument jsonBuffer(100);
  jsonBuffer["type"] = "ping";
  jsonBuffer["id"] = nextCmdId++;
  String json;
  serializeJson(jsonBuffer, json);
  webSocket.sendTXT(json);
}

void sendSuccessMessage() {
  sendWebSocketMessage("*" CONTROLLER_NAME ":* Unlocked");
}

/*
   Send a message informing the change of state that the door sensor detected.
*/
void sendReedSwitchTriggerMessage() {
  // Return if no contact sensor is connected
  if (!isReedSwitchConnected) {
    return;
  }

  // Check the contact state of the contact sensor
  String message = ":footprints: *" CONTROLLER_NAME ":*";
  if (reedSwitchState == HIGH) {
    message += " Closed manually";
  } else {
    message += " Openned manually";
  }

  sendWebSocketMessage(message);
}

/*
   Send a message with the current state of the door.
*/
void sendReedSwitchState() {
  String message;

  // Check the connection state of the contact sensor
  if (!isReedSwitchConnected) {
    if (SUPPORTS_REED_SWITCH) {
      message = ":heavy_exclamation_mark: *" CONTROLLER_NAME ":* Door sensor not found!\nPlease check if it is connected.";
    } else {
      message = ":information_source: *" CONTROLLER_NAME ":* Door sensor not supported by this controller.";
    }
    sendWebSocketMessage(message);
    return;
  }

  // If it's connected, get its contact state
  message = ":information_source: *" CONTROLLER_NAME ":* Is";
  if (reedSwitchState == HIGH) {
    message += " CLOSED";
  } else {
    message += " OPEN";
  }

  sendWebSocketMessage(message);
}

/*
   Send a message informing the detected change of the connection state of the door sensor.
*/
void sendReedConnectionChangedMessage() {
  String message;

  // Check the connection state of the contact sensor
  if (isReedSwitchConnected) {
    message = ":heavy_check_mark: *" CONTROLLER_NAME ":* Door sensor CONNECTED";
  } else {
    message = ":heavy_exclamation_mark: *" CONTROLLER_NAME ":* Door sensor DISCONNECTED";
  }

  sendWebSocketMessage(message);
}

/*
   Send a message with the current network information.
*/
void sendNetworkInformation() {
  String message = ":information_source: *" CONTROLLER_NAME ":* Network Information";
  message += "\n";
  message += getNetworkInformation();
  sendWebSocketMessage(message);
}

/*
   Send a message that informs that the device is connected.
*/
void sendHelloMessage() {
  String message = ":heavy_check_mark: *" CONTROLLER_NAME ":* Connected";

  // If a contact sensor is available, add its information
  if (isReedSwitchConnected) {
    if (reedSwitchState == HIGH) {
      message += " (State: CLOSED)";
    } else {
      message += " (State: OPEN)";
    }
  }

  // Add network information
  message += "\n";
  message += getNetworkInformation();

  sendWebSocketMessage(message);
}

/*
  Processes the received slack message by looking for the predefined commands
  to perform the correspondent actions.
*/
void processSlackMessage(char *payload) {
  // Parse JSON object
  DynamicJsonDocument jsonBuffer(1024);
  DeserializationError error = deserializeJson(jsonBuffer, payload);
  Serial.println("After deserialize");

  if (error) {
    Serial.println("Parsing failed!");
    return;
  }

  Serial.println("Has no error");

  // If one of the following keys is not present, return immediately
  if (!jsonBuffer.containsKey("type")
      || !jsonBuffer.containsKey("channel")
      || !jsonBuffer.containsKey("text")
      || !jsonBuffer.containsKey("user")) {
    return;
  }

  Serial.println("Has necessary fields");

  // Check if it's a message
  const char* type = jsonBuffer["type"];
  if (strcasecmp(type, "message") != 0) {
    return;
  }

  Serial.println("Is a message type");

  // Check if it's coming from the authorized channel
  const char* channel = jsonBuffer["channel"];
  if (strcasecmp(channel, SLACK_CHANNEL_ID) != 0) {
    return;
  }

  Serial.println("Is from the correct channel");

  // Normalize the message
  const char* trimedText = jsonBuffer["text"];
  //char trimedText[sizeof(text)];
  //trimCharArray(trimedText, text);

  //Serial.println("After trimming");

  // Get the user Id to include in the return message
  const char* userID = jsonBuffer["user"];

  // Evaluate the message to see if it's an opening command
  if (strcasecmp(trimedText, COMMAND_OPEN_1) == 0
      || strcasecmp(trimedText, COMMAND_OPEN_2) == 0) {

    Serial.printf("------------------------> %s %s %s\n", trimedText, channel, userID);

    digitalWrite(RELAY_PIN, HIGH);
    delay(TRIGGER_DURATION);
    digitalWrite(RELAY_PIN, LOW);

    SLACK_MESSAGE msgToQueue = { 'S' };
    queue.push(msgToQueue);

    return;
  }

  Serial.println("After checking if it's an opening command");

  // Evaluate the message to see if it's a state command
  if (strcasecmp(trimedText, COMMAND_STATE) == 0) {

    Serial.printf("------------------------> %s %s %s\n", trimedText, channel, userID);

    SLACK_MESSAGE msgToQueue = { 'R' };
    queue.push(msgToQueue);

    return;
  }

  Serial.println("After checking if it's a state command");

  // Evaluate the message to see if it's a ping command
  if (strcasecmp(trimedText, COMMAND_PING) == 0) {

    Serial.printf("------------------------> %s %s %s\n", trimedText, channel, userID);

    SLACK_MESSAGE msgToQueue = { 'P' };
    queue.push(msgToQueue);

    return;
  }

  Serial.println("After checking if it's a ping command");
}

/*
  Called on each web socket event. Handles disconnection, and also incoming
  messages from slack.
*/
void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      isConnectedToSlack = false;

      // Reset the ESP to avoid OOM exception
      ESP.reset();
      break;

    case WStype_CONNECTED:
      Serial.printf("Connected Free Heap: %d | Heap Fragmentation: %d | Max Free Block Size: %d\n", ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      isConnectedToSlack = true;
      sendPing();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      processSlackMessage((char*)payload);
      break;
  }
}

/*
  Establishes a bot connection to Slack:
  1. Performs a REST call to get the WebSocket URL
  2. Connects the WebSocket
  Returns true if the connection was established successfully.
*/
void connectToSlack() {
  Serial.println("Connecting to slack ...");

  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.connect)
  // This is a Web API Tier 1 with a limit of 1+ per minute
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.connect?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d and message: %s\n", httpCode, http.errorToString(httpCode).c_str());
    return;
  }

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host= " + host + " Path= " + path);
  webSocket.beginSSL(host, 443, path, "", "");
  Serial.println("After begin ssl");
  Serial.printf("Begin ssl Free Heap: %d | Heap Fragmentation: %d | Max Free Block Size: %d\n", ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
  webSocket.onEvent(webSocketEvent);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(PING_LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(REED_SWITCH_STATE_PIN, INPUT);
  pinMode(REED_SWITCH_CONNECTED_PIN, INPUT);

  delay(500);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(WIFI_CONNECTION_DURATION);
  }

  Serial.print("IP: ");
  Serial.print(WiFi.localIP());
  Serial.print(" MAC: ");
  Serial.println(WiFi.macAddress());

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

/*
  Sends a ping every 5 seconds, and handles reconnections.
*/
void loop() {
  webSocket.loop();

  if (lastHeapMsg == 0 || millis() - lastHeapMsg > 5000) {
    Serial.printf("Free Heap: %d | Heap Fragmentation: %d | Max Free Block Size: %d\n", ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize());
    lastHeapMsg = millis();
  }

  getReedSwitchState();

  // Try to connect if not connected or reconnect if enough time has passed since last attempt
  if (!isConnectedToSlack &&
      ((millis() - lastSlackConnectionRequest) > SLACK_RECONNECTION_DELAY
       || lastSlackConnectionRequest == 0)) {
    lastSlackConnectionRequest = millis();

    connectToSlack();

    return;
  }

  if (isConnectedToSlack) {
    if (isFirstRun) {
      sendHelloMessage();
    }

    processMessagesInQueue();

    evaluateReedSwitchStates();

    keepConnectionAlive();

    isFirstRun = false;
  }
}

/*
   Send ping every X seconds, to keep the connection alive
*/
void keepConnectionAlive() {
  if (millis() - lastPing > SLACK_PING_DELAY) {
    digitalWrite(PING_LED_PIN, LOW);

    sendPing();

    lastPing = millis();
  } else {
    digitalWrite(PING_LED_PIN, HIGH);
  }
}

/*
   Process any waiting message
*/
void processMessagesInQueue() {
  while (!queue.isEmpty ()) {
    currentMsg = queue.pop();
    switch (currentMsg.type) {
      case 'S': // success
        sendSuccessMessage();

        lastPing = millis();
        break;

      case 'F': // fail
        break;

      case 'R': // current state of the reed switch
        sendReedSwitchState();
        break;

      case 'P': // current network information
        sendNetworkInformation();
        break;
    }
  }
}

/*
  Send a json message to the slack channel using the web socket.
  This does not support attachments nor blocks.
*/
void sendWebSocketMessage(String message) {
  // Start creating the json for the response
  DynamicJsonDocument messageBuffer(1024);
  messageBuffer["id"] = nextCmdId++;
  messageBuffer["type"] = "message";
  messageBuffer["channel"] = SLACK_CHANNEL_ID;
  messageBuffer["text"] = message;

  // Send the message
  String messageJson;
  serializeJson(messageBuffer, messageJson);
  Serial.printf("JSON to send-> %s\n", messageJson.c_str());
  webSocket.sendTXT(messageJson);
}

/*
   Experimental.
   Post a json message to the slack channel using the web api.
*/
void sendWebApiMessage() {
  //std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  //client->setFingerprint(SLACK_SSL_FINGERPRINT);

  HTTPClient https;

  Serial.println("[HTTPS] begin...");
  if (https.begin("https://slack.com/api/chat.postMessage", SLACK_SSL_FINGERPRINT)) {
    Serial.println("[HTTPS] headers...");

    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " SLACK_BOT_TOKEN);

    Serial.println("[HTTPS] post...");
    int httpCode = https.POST("{ \"channel\": \"G9APARESY\",\"as_user\": true,\"text\": \"asdas\"}");
    //int httpCode = https.POST(messageJson);
    //String payload = https.getString();

    Serial.println("[HTTPS] code...");

    Serial.println(httpCode);
    //Serial.println(payload);

    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

/*
  Trim the extra white spaces of a message.
  !! Takes too long to execute on a long message which makes the watchdog reset the device.
  !! Therefore it's not being used right now.
*/
void trimCharArray(char *dest, const char *src) {
  char prevChar = ' ';

  for (; *src != '\0'; ++src) {
    if (*src != ' ' || (*src == ' ' && prevChar != ' ')) {
      prevChar = *src;
      *dest++ = *src;
    }
  }
  *dest = '\0';
}

/*
   Provides a formatted string containing the network information.
*/
String getNetworkInformation() {
  String information;
  information += "*Wi-Fi:* _";
  information += WiFi.SSID();
  information += "_";

  information += " *| Strength:* _";

  int rssi = WiFi.RSSI();
  if (rssi >= -40) {
    information += " ▰▰▰▰";
  } else if (rssi >= -60) {
    information += " ▰▰▰▱";
  } else if (rssi >= -70) {
    information += " ▰▰▱▱";
  } else if (rssi >= -80) {
    information += " ▰▱▱▱";
  } else {
    information += " ▱▱▱▱";
  }
  information += " (";
  information += rssi;
  information += ")_";

  information += "\n*IP*: _";
  information += WiFi.localIP().toString();
  information += "_";

  information += " *| MAC*: _";
  information += WiFi.macAddress();
  information += "_";

  return information;
}

/*
   Check if the reed switch is connected and if so, read its state (open or closed)
*/
void getReedSwitchState() {
  // Return immediately if it doesn't support a reed switch
  if (!SUPPORTS_REED_SWITCH) {
    return;
  }

  // Get the connectivity state of the reed switch
  int reedSwitchConnectedState = digitalRead(REED_SWITCH_CONNECTED_PIN);

  // Check if the reed switch is connected to the controller
  if (reedSwitchConnectedState == HIGH) {
    Serial.printf("HIGH %d %d %d\n", isFirstRun, isReedSwitchConnected, reedSwitchConnectedState);
    // Check if the connection changed (previously disconnected)
    if (!isFirstRun && !isReedSwitchConnected) {
      isReedConnectionChanged = true;
      isReedSwitchConnectedRecently = true;
      Serial.println("changed to connected");
    }

    // Get the reed switch state if it's connected
    reedSwitchState = digitalRead(REED_SWITCH_STATE_PIN);

    isReedSwitchConnected = true;
  } else {
    //Serial.printf("HIGH %d %d %d\n", isFirstRun, isReedSwitchConnected, reedSwitchClosedPinState);
    // Check if the connection changed (previously connected)
    if (!isFirstRun && isReedSwitchConnected) {
      isReedConnectionChanged = true;
      isReedSwitchConnectedRecently = false;
      Serial.println("changed to disconnected");
    }

    isReedSwitchConnected = false;
  }
}

void evaluateReedSwitchStates() {
  if (SUPPORTS_REED_SWITCH) {
    evaluateReedSwitchConnectivityStateToInform();

    evaluateReedSwitchStateToInform();
  }
}

/*
   Evaluate the connectivity state of the reed sensor and send a message if it has changed
*/
void evaluateReedSwitchConnectivityStateToInform() {
  // Ignore the first run
  if (!isFirstRun && isReedConnectionChanged) {
    isReedConnectionChanged = false;
    sendReedConnectionChangedMessage();
  }
}

/*
  Evaluate the reed switch state and send a message if it has changed
*/
void evaluateReedSwitchStateToInform() {
  if (isReedSwitchConnected) {
    if (reedSwitchState == HIGH && !isReedHigh) {
      // If the switch is closed and it's the first time after it closed
      isReedLow = false;
      isReedHigh = true;

      // Send trigger message if not the first run or after a recent reed connection
      if (!isFirstRun && !isReedSwitchConnectedRecently) {
        isReedTriggerSent = false;
      }
    } else if (reedSwitchState == LOW && !isReedLow) {
      // If the switch is open and it's the first time after it openned
      isReedLow = true;
      isReedHigh = false;

      // Send trigger message if not the first run or after a recent reed connection
      if (!isFirstRun && !isReedSwitchConnectedRecently) {
        isReedTriggerSent = false;
      }
    }

    // Now that we have ignored any possible alteration after a recent reed connection, clear the flag
    isReedSwitchConnectedRecently = false;

    // Only trigger the message of the reed state if it's not the first run as we already send the state in the hello
    if (!isFirstRun && !isReedTriggerSent) {
      isReedTriggerSent = true;
      sendReedSwitchTriggerMessage();
    }
  }
}
