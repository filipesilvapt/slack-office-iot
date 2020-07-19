#ifndef CONTROLLER_CONFIGS_H
#define CONTROLLER_CONFIGS_H

// Slack SSL fingerprint (Get 'slack.com' fingerprint from https://www.grc.com/fingerprints.htm)
// Valid from 02/08/18 to 02/12/21
#define SLACK_SSL_FINGERPRINT "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3"

// Get token by creating new bot integration at https://my.slack.com/services/new/bot
#define SLACK_BOT_TOKEN  ""

// The channel identifier (i.e. GP6F6369C)
#define SLACK_CHANNEL_ID ""

// The name that will appear in the network (router) to ease the identification (Rules: https://www.ietf.org/rfc/rfc1178.txt)
#define NETWORK_HOSTNAME ""

// The name that will be used in every message sent by the controller
#define CONTROLLER_NAME  ""

// The supported commands for the corresponding actions
#define COMMAND_OPEN_1   ""
#define COMMAND_OPEN_2   ""
#define COMMAND_STATE    ""
#define COMMAND_PING     ""

// The Wi-Fi details
#define WIFI_SSID        ""
#define WIFI_PASSWORD    ""

// Extra features
#define SUPPORTS_REED_SWITCH    false

#endif
