#include "WebSocketWritable.h"
#include "WebSocket.h"

word WebSocketWritable::printf(const char *format, ...)
{
        // Re(ab)use the 'frame' buffer to conserve RAM:
        va_list ap;
        va_start(ap, format);
        frame.length = vsnprintf(frame.data, frameCapacity, format, ap);
        va_end(ap);
        return send(frame.data, frame.length);
}

word WebSocketWritable::printf_P(const __FlashStringHelper *format, ...)
{
        // Re(ab)use the 'frame' buffer to conserve RAM:
        va_list ap;
        va_start(ap, format);
        frame.length = vsnprintf_P(frame.data, frameCapacity, (const char *)format, ap);
        va_end(ap);
        return send(frame.data, frame.length);
}

