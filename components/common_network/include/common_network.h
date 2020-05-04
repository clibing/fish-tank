#ifndef _COMMON_NETWORK_H_
#define _COMMON_NETWORK_H_

/**
 * type code:
 *   1: only connection wifi
 *   2: smart config network and connection
 * status code:
 *   1: ok
 *   -1: failed
 */
typedef void(*wifi_callback_t)(int type, int status, char *ip);

typedef void(*oled_callback_t)(char *message);

void initialise_wifi(wifi_callback_t w_callback, oled_callback_t o_callback);

void start_smart_config(wifi_callback_t  w_callback, oled_callback_t o_callback);

#endif

