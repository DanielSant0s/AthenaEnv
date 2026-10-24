/*
 * Websockets, in C: the port of main.js. A secure WebSocket to
 * wss://echo.websocket.org, logging the server's greeting.
 */

#include <stdlib.h>

#define WS_RECV_BUF_SIZE 16000

#include <athena/athena.h>
#include "athenaos/app.h"

static int websockets_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Websockets", "network", "wss://echo.websocket.org", NULL, 0);

    athenaos_terminal_log(&term, "Resetting IOP and loading the network driver...");
    athenaos_terminal_draw(&term);

    athena_iop_reset();
    // Keep the pad driver after the reset so Home works.
    athena_iop_load_module(athena_iop_get_module_by_name("padman"), 0, NULL);
    athena_iop_load_module(athena_iop_get_module_by_name("SMAP"), 0, NULL);

    athena_net_init_dhcp();

    AthenaNetConfig config;
    athena_net_get_config(&config);
    athenaos_terminal_log(&term, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gateway\":\"%s\",\"dns\":\"%s\"}",
                          config.ip, config.netmask, config.gateway, config.dns);
    athenaos_terminal_log(&term, "Connecting to wss://echo.websocket.org ...");
    athenaos_terminal_draw(&term);

    AthenaWebSocket *ws = athena_websocket_connect("wss://echo.websocket.org", true);
    if (ws) {
        uint8_t *buf = malloc(WS_RECV_BUF_SIZE);
        size_t len = 0;
        if (athena_websocket_recv(ws, buf, WS_RECV_BUF_SIZE, &len) != 0)
            len = 0;
        athenaos_terminal_log(&term, "%.*s", (int)len, (const char *)buf);
        free(buf);
        athena_websocket_close(ws);
        athena_websocket_destroy(ws);
    }

    athena_net_deinit();

    AthenaPad *pad = athena_pad_open(0);
    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;
        athenaos_terminal_draw(&term);
    }
    return 0;
}

ATHENAOS_APP(websockets, websockets_main)
