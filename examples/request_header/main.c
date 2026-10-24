/*
 * Header request, in C: the port of main.js. An HTTP HEAD request.
 */

#include <athena/athena.h>
#include "athenaos/app.h"

static int request_header_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Header request", "network", "HTTP HEAD request", NULL, 0);

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
    athenaos_terminal_log(&term, "HEAD https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json");
    athenaos_terminal_draw(&term);

    AthenaRequest *req = athena_request_create();
    athena_request_set_follow_redirects(req, true);
    static const char *headers[] = { "upgrade-insecure-requests: 1", "sec-fetch-dest: document",
                                     "sec-fetch-mode: navigate" };
    athena_request_set_headers(req, headers, 3);

    AthenaRequestResponse *result = NULL;
    athena_request_head(req, "https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json", &result);
    athenaos_terminal_log(&term, "%.*s", result ? (int)result->body_size : 0, result ? result->body : "");
    athena_request_response_free(result);
    athena_request_destroy(req);

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

ATHENAOS_APP(request_header, request_header_main)
