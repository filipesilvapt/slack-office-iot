/*
  Board: LOLIN(Wemos) D1 mini Pro
*/
#ifndef BOARD_CONFIGS_H
#define BOARD_CONFIGS_H

// Max time to wait before retrying the wi-fi connection (in millis)
#define WIFI_CONNECTION_DURATION    5000

// Duration that the electrical lock is kept active (in millis)
#define TRIGGER_DURATION            2000

// Min time before attempting to reconnect to slack (in millis)
// 30 seconds to respect the limit of 1+ connection per minute
#define SLACK_RECONNECTION_DELAY    30000

// Min time that a ping should be sent to keep the connection alive (in millis)
#define SLACK_PING_DELAY            5000

#define PING_LED_PIN                D4
#define RELAY_PIN                   D1
#define REED_SWITCH_PIN             D2
#define REED_SWITCH_CLOSED_PIN      D5

#endif
