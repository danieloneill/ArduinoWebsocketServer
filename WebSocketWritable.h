#include <Arduino.h>
#include <stdarg.h>

#ifndef H_WEBSOCKETWRITABLE
#define H_WEBSOCKETWRITABLE

// Implement a way to "printf" to the socket. Also provided is a PSTR-able method for additional (and delicious) RAM savings.
class WebSocketWritable {
public:
    virtual byte send(char *str, word length);
    word printf(const char *format, ...);
    word printf_P(const __FlashStringHelper *format, ...);
};

#endif
