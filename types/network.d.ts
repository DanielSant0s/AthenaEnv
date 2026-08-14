/**
 * AthenaEnv — Networking: `Network`, `Request`, `Socket`, `WebSocket`.
 * Verified against src/js_api/ath_network.c, ath_request.c, ath_socket.c,
 * ath_websocket.c. All four require the `ATHENA_NETWORK` build flag.
 */

interface NetworkConfig {
    ip: string;
    netmask: string;
    gateway: string;
    dns: string;
}

declare namespace Network {
    /**
     * @remarks All-or-nothing: exactly 0 args (DHCP) or exactly 4 args
     * (static) — passing 1-3 arguments throws `InternalError`.
     */
    function init(ip: string, netmask: string, gateway: string, dns: string): void;
    /** DHCP mode. */
    function init(): void;
    function getConfig(): NetworkConfig;
    /** Resolves `host` to a dotted-quad IPv4 string. */
    function getHostbyName(host: string): string;
    /** Shuts down the network module. */
    function deinit(): void;
}

interface RequestResponse {
    text: string;
    status_code: number;
    headers?: string;
}

declare class Request {
    constructor();

    keepalive: boolean;
    useragent: string;
    userpwd: string;
    /** Milliseconds. */
    timeout: number;
    verifyTLS: boolean;
    followRedirects: boolean;
    /** Max 16 entries — throws `RangeError` beyond that. */
    headers: string[];

    get(url: string): RequestResponse;
    head(url: string): RequestResponse;
    post(url: string, data: string): RequestResponse;
    download(url: string, fname: string): void;

    asyncGet(url: string): void;
    asyncPost(url: string, data: string): void;
    asyncDownload(url: string, fname: string): void;
    /**
     * Polls for async completion.
     * @param timeout Defaults to 999999999 ms.
     * @param conn_timeout Defaults to 3000 ms.
     */
    ready(timeout?: number, conn_timeout?: number): boolean;
    getAsyncData(): string | undefined;
    /** Bytes transferred so far for the in-flight async request. */
    getAsyncSize(): number;
}

declare namespace Socket {
    const AF_INET: number;
    const SOCK_STREAM: number;
    const SOCK_DGRAM: number;
    const SOCK_RAW: number;
}

declare class Socket {
    constructor(domain: number, type: number);

    connect(host: string, port: number): number;
    bind(host: string, port: number): number;
    listen(): number;
    /**
     * @remarks Unlike {@link WebSocket.send}, `data` is a C string here —
     * pass a string, not an ArrayBuffer.
     */
    send(data: string): number;
    /**
     * @remarks Unlike {@link WebSocket.recv}, this returns a JS string, not
     * an ArrayBuffer.
     */
    recv(size: number): string;
    close(): void;
}

declare class WebSocket {
    /** @param options.verifyTLS Defaults to `true`. */
    constructor(url: string, options?: { verifyTLS?: boolean });

    /** @remarks Read-only — assigning throws `TypeError`. */
    readonly verifyTLS: boolean;

    /** Requires a real `ArrayBuffer` (unlike {@link Socket.send}, which takes a string). */
    send(data: ArrayBuffer): number;
    /** @returns a new `ArrayBuffer`, or `undefined` if no data is available (does not distinguish "no data yet" from an error). */
    recv(): ArrayBuffer | undefined;
    close(): void;
}
