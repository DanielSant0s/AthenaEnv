# Network API

`Network`, `Request`, `Socket`, `WebSocket`.

By default these live in `athena_network.erl`. Importing the module also installs the four names as globals.

```js
import { Network, Request, Socket, WebSocket } from 'athena_network.erl';
```

P.S.: *Italic* parameters refer to optional parameters.

## Network module

* `Network.init(*ip*, *netmask*, *gateway*, *dns*)`
```js
Network.init("192.168.0.10", "255.255.255.0", "192.168.0.1", "192.168.0.1"); //Static mode
Network.init(); //DHCP Mode, dynamic.
```
* `let conf = Network.getConfig()` — Returns `conf.ip`, `conf.netmask`, `conf.gateway`, `conf.dns`.
* `Network.deinit()` — Shuts down the network module.

## Request module

**Construction:**
```js
let r = new Request();
```

**Properties:**

* `keepalive` - bool
* `useragent` - string
* `userpwd` - string
* `timeout` - number, milliseconds
* `verifyTLS` - bool
* `followRedirects` - bool
* `headers` - string array, 16 entries maximum

**Methods:**

* `get(url)`, `head(url)`, `post(url, data)` — return `{ text, status_code, headers }`.
* `download(url, fname)` — writes the body to `fname` on the calling thread.

**Asynchronous methods:**
* `asyncGet(url)`
* `asyncPost(url, data)`
* `asyncDownload(url, fname)` — opens `fname` for writing and downloads on a background thread. The path is used as given.
* `ready(*unused*, *stall_ms*)` — returns `true` once that thread has finished. `stall_ms` defaults to `3000`. If no bytes have arrived within that window, `ready` throws. The first argument is accepted and ignored, so `ready(30)` does not cap the transfer.
* `getAsyncData()` — body of a finished in-memory request, as a string.
* `getAsyncSize()` — bytes received so far.

## Socket module

**Properties:**

* `Socket.AF_INET`
* `Socket.SOCK_STREAM`
* `Socket.SOCK_DGRAM`
* `Socket.SOCK_RAW`

**Construction:**
```js
let s = new Socket(Socket.AF_INET, Socket.SOCK_STREAM);
```

**Methods:**

* `connect(host, port)`
* `bind(host, port)`
* `listen()`
* `send(data)` - Send string data, returns bytes sent.
* `recv(size)` - Receive up to `size` bytes, returned as a string.
* `close()`

## WebSocket module

**Construction:**
```js
let s = new WebSocket("wss://example.com");
```

**Methods:**

* `send(data)` - Send data with Buffer.
* `recv()` - Receive data to a buffer.
