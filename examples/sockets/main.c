/*
 * Sockets demo, in C: the port of main.js. A raw TCP socket talking HTTP to
 * www.google.com:80.
 */

#include <string.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static int sockets_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Sockets demo", "network", "Raw TCP socket to www.google.com:80", NULL, 0);

    athenaos_terminal_log(&term, "Resetting IOP and loading the network driver...");
    athenaos_terminal_draw(&term);

    athena_iop_reset();
    // Keep the pad driver after the reset so Home works.
    athena_iop_load_module(athena_iop_get_module_by_name("padman"), 0, NULL);
    athena_iop_load_module(athena_iop_get_module_by_name("SMAP"), 0, NULL);

    athena_net_init_dhcp();

    AthenaNetConfig config;
    athena_net_get_config(&config);
    athenaos_terminal_log(&term, "IP Address: %s", config.ip);
    athenaos_terminal_log(&term, "Netmask: %s", config.netmask);
    athenaos_terminal_log(&term, "Gateway: %s", config.gateway);
    athenaos_terminal_log(&term, "DNS: %s", config.dns);
    athenaos_terminal_draw(&term);

    AthenaSocket *s = athena_socket_create(AF_INET, SOCK_STREAM);

    char host[16] = "";
    athena_net_resolve("www.google.com", host, sizeof(host));
    athenaos_terminal_log(&term, "www.google.com -> %s", host);
    athenaos_terminal_draw(&term);

    const char *request = "GET / HTTP/1.1\r\nHost:www.google.com\r\n\r\n";
    athena_socket_connect(s, host, 80);
    athena_socket_send(s, request, strlen(request));
    static char msg[1024];
    size_t received = 0;
    if (athena_socket_recv(s, msg, sizeof(msg), &received) <= 0)
        received = 0;
    athenaos_terminal_log(&term, "%.*s", (int)received, msg);
    athena_socket_close(s);
    athena_socket_destroy(s);

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

ATHENAOS_APP(sockets, sockets_main)
