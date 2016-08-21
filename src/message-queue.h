/***
 * 2016, Akshaal based upon:
 * Original copyright: Message Queue. Copyright © 2013 Matthew Tole. Version 1.0.0
 ***/

#include <pebble.h>

#ifndef AK_MESSAGE_QUEUE_H
#define AK_MESSAGE_QUEUE_H

typedef void (*MessageHandler)(DictionaryIterator *iterator);

// We start with 10 to avoid collision with old enumeration...
enum {
    MSG_KEY_CMD = 10,
    MSG_KEY_DATA = 11,
    MSG_KEY_UUID = 12
};

void mq_init(MessageHandler handler);
bool mq_add(uint8_t cmd, char* data);

__attribute__((format(printf, 3, 4))) __attribute__ ((__gnu_inline__))
extern inline bool mq_fmt(uint8_t cmd, int max_size, char* format, ...)  {
    char buf[max_size + 1];
    snprintf(buf, max_size, format, __builtin_va_arg_pack ());
    return mq_add(cmd, buf);
}

#endif /* AK_MESSAGE_QUEUE_H */
