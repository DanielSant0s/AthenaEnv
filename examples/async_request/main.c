/*
 * Async Network, in C: the port of main.js. A non-blocking HTTPS download: the
 * terminal keeps drawing, with the bytes so far, while it runs.
 */

#include <stdio.h>
#include <stdlib.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static int async_request_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Async Network", "network", "Non-blocking HTTPS download", NULL, 0);

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

    AthenaRequest *req = athena_request_create();
    athena_request_set_verify_tls(req, true);
    static const char *headers[] = { "upgrade-insecure-requests: 1", "sec-fetch-dest: document",
                                     "sec-fetch-mode: navigate" };
    athena_request_set_headers(req, headers, 3);

    // req.asyncGet(url) of the script binding.
    req->url = "https://raw.githubusercontent.com/DanielSant0s/brewstore-db/main/brew_data.json";
    req->method = ATHENA_REQUEST_GET;
    req->save = false;
    req->chunk.memory = malloc(1);
    req->chunk.size = 0;
    athena_request_run_async(req, "Requests: Get");

    // The download runs in the background, so the UI keeps drawing meanwhile.
    while (!athena_request_poll(req, 2, 3000)) {
        snprintf(term.ui.status, sizeof(term.ui.status), "%u bytes", (unsigned)req->chunk.size);
        athenaos_terminal_draw(&term);
    }

    term.ui.status[0] = '\0';
    athenaos_terminal_log(&term, "Done: %u bytes transferred", (unsigned)req->chunk.size);
    AthenaRequestResponse *resp = athena_request_take_response(req);
    if (resp) {
        athenaos_terminal_log(&term, "%.*s", (int)resp->body_size, resp->body);
        athena_request_response_free(resp);
    }
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

ATHENAOS_APP(async_request, async_request_main)
