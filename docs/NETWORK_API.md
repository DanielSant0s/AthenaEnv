# Network API

`Network`, `Request`, `Socket`, `WebSocket`.

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
* `headers` - string array

**Methods:**

* `get(url)`
* `head(url)`
* `post(url, data)`
* `download(url, fname)`

**Asynchronous methods:**
* `asyncGet(url)`
* `asyncDownload(url, fname)`
* `ready(*timeout*, *conn_timeout*)`
* `getAsyncData()`
* `getAsyncSize()`

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
