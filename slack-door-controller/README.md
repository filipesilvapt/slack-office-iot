# Slack Door Controller

Authentication is guaranteed by adding only the authorized members to the required Slack channel.
Using some text commands in that channel it's possible to:
- Open a door
- See the door state (open or closed)
- Detect when the door is opened or closed (through the sensor)
- Detect if the door sensor has been removed

**Notes:**
- WebSocketsClient::beginSSL is incompatible with the new ESP8266 Arduino v2.5.0
https://github.com/Links2004/arduinoWebSockets/issues/428
- Right now the fix is to have the client not verify the server certificate.
- A possible solution would be to use BearSSL certificate store or to have the certificate in the sketch.
- Bear in mind that certificates might change on a regular basis.

-------------------------------------------------------------

To implement the fix, edit the file WebSocketsClient.cpp and add the if section presented below right after the creation of the ssl object in the loop function:
```
            _client.ssl = new WEBSOCKETS_NETWORK_SSL_CLASS();
#if defined(wificlientbearssl_h)
			_client.ssl->setInsecure();
#endif
            _client.tcp = _client.ssl;
```
----------------------------------------------------------------

# How to deploy a controller node:
- Make sure you are using the arduino and library versions indicated inside the sketch file
- Choose one of the board configs file and place it in the root folder with the name "BoardConfigs.h"
- Copy the "ControllerConfigs_Example.h" to a new file named "ControllerConfigs.h" and fill the required parameters
- Choose the correct board within the Arduino IDE
- Connect the device and run the sketch
