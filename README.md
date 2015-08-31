## Websocket Server for Arduino

This library implements a Websocket server running on an Arduino. It's based on the [proposed standard][1] published December 2011 which is supported in the current versions (June 2012) of Firefox, Chrome, and Safari 6.0 beta (not older Safari, unfortunately) and thus is quite usable.

The implementation in this library has restrictions as the Arduino platform resources are very limited:

* The server **only** handles TXT frames.
* The server **only** handles **single byte** chars. The Arduino just can't handle UTF-8 to it's full.
* The server **only** accepts **final** frames. No fragmented data, in other words.
* For now, the server silently ignores all frames except TXT and CLOSE.
* The amount of simultaneous connections may be limited by RAM or hardware. (Each connection takes 16 bytes of RAM, and the W5100 shield is hardware-limited to 4 simultaneous connections.)
* There's no keep-alive logic implemented.

_Required headers (example):_

	GET /whatever HTTP/1.1
	Host: server.example.com
	Upgrade: websocket
	Connection: Upgrade
	Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
	Sec-WebSocket-Version: 13

The server checks that all of these headers are present, but only cares that the version is 13.

_Response example:_

	HTTP/1.1 101 Switching Protocols
	Upgrade: websocket
	Connection: Upgrade
	Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=

The last line is the Base64 encoded SHA-1 hash of the key with a concatenated GUID, as specified by the standard.

**Daniel O'Neill:** ***Compared to the original, this library consumes significantly less RAM and provides additional functionality.***

### Requirements

* Arduino IDE 1.0.1 or greater. You should not use 1.0 since it has a bug in the Ethernet library that will affect this library.
* An Arduino Duemilanove or greater with Ethernet shield. An Arduino Ethernet should work too, but it has not been tested.
* A Websocket client that conforms to version 13 of the protocol.

### Getting started

Install the library to "libraries" folder in your Arduino sketchbook folder. For example, on a mac that's `~/Documents/Arduino/libraries`.

Try the supplied echo example together with the the [web based test client][2] to ensure that things work.

Start playing with your own code and enjoy!

An example websocket server might look as follows:

```
#include <Ethernet.h>
#include <WebSocket.h>
#include <WebSocketServer.h>

// Websocket Server
WebSocketServer *wsServer;

void onConnect(InboundWebSocket &socket, void *opaque) {
  Serial.println(F("New connection!"));
  socket.registerDataCallback(&onData, NULL);
}

void onData(WebSocket &socket, char *socketString, word frameLength, void *opaque)
{
  Serial.print(F("Received packet: "));
  Serial.println(socketString);
}

void setupEthernet() {
  Ethernet.begin();

  // Give Ethernet time to get ready
  delay(100);
}

void setupWebsocket()
{
  wsServer = new WebSocketServer("/", 80, 3, 250);
  wsServer->registerConnectCallback(&onConnect, NULL);
  wsServer->begin();
}

void setup()
{
  Serial.begin( 115200 );
  setupEthernet();
  setupWebsocket();
}

void loop()
{
  delay(10);
  wsServer->listen();
}
```

# API

## void WebSocket *ws = new WebSocket( word [Maximum frame size] )
`Constructor for a WebSocket object. Optional parameter allows to increase maximum frame size to specified value.`

--

## void WebSocket::registerDataCallback( DataCallback *callback, [void *opaque=NULL] )
`Register a callback to call when a data frame is received. If **opaque** is provided it will be passed as a callback parameter:`

```cpp
    typedef void DataCallback(WebSocket &socket, char *socketString, word frameLength, void *opaque);
```

--

## void WebSocket::registerConnectCallback( Callback *callback, [void *opaque=NULL] )
`Register a callback to call when the connection is established. If **opaque** is provided it will be passed as a callback parameter:`

```cpp
    typedef void Callback(WebSocket &socket, void *opaque);
```

--

## void WebSocket::registerDisconnectCallback( Callback *callback, [void *opaque=NULL] )
`Register a callback to call when the connection is closed. If **opaque** is provided it will be passed as a callback parameter:`

```cpp
    typedef void Callback(WebSocket &socket, void *opaque);
```

--

## bool WebSocket::connected()
`Returns **true** if the socket is connected, otherise **false**.`

--

## EthernetClient WebSocket::socket()
`Returns the associated **EthernetClient** object.`

--

## byte WebSocket::send(char *str, word length)
`Transmit a TXT string exactly **length** bytes long.`

* Returns the count of bytes transmitted in frame.

--

## void WebSocket::listen()
`WebSocket polling function. This is meant to be called by the developer periodically, such as from within the **loop()** method.`

--

## void WebSocket::close()
`Gracefully terminate the associated connection.`

--

## void WebSocket::setKeepalive(unsigned int interval)
`Specify a keepalive frequency in milliseconds, or 0 for "don't transmit".`

--

## void WebSocket::setTimeout(unsigned int deadline)
`Specify a connection timeout in milliseconds, or 0 for "never timeout". (Although it still can if the underlying socket dies.)`

--

## static void WebSocket::initialise( word maxFrameSize )
`Can be called by the user to (re-)initialise the global WebSocket context.`

--

## static void WebSocket::deinitialise();
`Deinitialise the global WebSocket context to free as much RAM as possible. WebSocket::initialise() must be called before resuming use and no WebSocket functionality will be available until it is.`

--

## WebSocketServer *wss = new WebSocketServer([const char *urlPrefix = "/"], [int inPort = 80], [byte maxConnections = 4], [word maxFrameSize = 96])
`Create a new WebSocketServer context, optionally with parameters such as URL prefix, port, maximum allowed connections, and maximum data frame size.`

* Returns a WebSocketServer context object pointer.

--

## void WebSocketServer::registerConnectCallback(Callback *callback, [void *opaque=NULL])
`Register a callback to call when a new connection is received. If **opaque** is provided it will be passed as a callback parameter:`

```cpp
    typedef void Callback(InboundWebSocket &socket, void *opaque);
```

**InboundWebSocket can be accessed as a normal WebSocket object, but a WebSocket object cannot do the inverse.**

--

## void WebSocketServer::registerDisconnectCallback(Callback *callback, [void *opaque=NULL])
`Register a callback to call when a connection is closed. If **opaque** is provided it will be passed as a callback parameter:`

```cpp
    typedef void Callback(InboundWebSocket &socket, void *opaque);
```

**InboundWebSocket can be accessed as a normal WebSocket object, but a WebSocket object cannot do the inverse.**

--

## void WebSocketServer::begin()
`Start listening for connections. This must be called before any connections will be accepted.`

--

## void WebSocketServer::listen()
`Main listener for incoming data. Should be called from the loop.`

--

## byte WebSocketServer::connectionCount()
`Returns a count of current connections to this context object.`

--

## byte WebSocketServer::send(char *string, word length);
`Broadcast to a text string of specified length to all connected clients.`

* Returns a count of bytes transmitted.


# Feedback

I'm a pretty lousy programmer, at least when it comes to Arduino, and it's been 15 years since I last touched C++, so do file issues for every opportunity for improvement.

Oh by the way, quoting myself:

> Don't forget to place a big ***fat*** disclaimer in the README. There is most certainly bugs in the code and I may well have misunderstood some things in the specification which I have only skimmed through and not slept with. So _please_ do not use this code in appliancies where living things could get hurt, like space shuttles, dog tread mills and Large Hadron Colliders.


[1]: http://datatracker.ietf.org/doc/rfc6455/?include_text=1 "Protol version implemented here"
[2]: http://www.websocket.org/echo.html "Echo Test client"
