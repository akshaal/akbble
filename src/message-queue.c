/***
 * 2016, Akshaal based upon:
 * Original copyright: Message Queue. Copyright © 2013 Matthew Tole. Version 1.0.0
 ***/

#include <pebble.h>
#include "message-queue.h"
#include "utils.h"

#define ATTEMPT_COUNT 4
#define MSG_UUID_HIST_LEN 20
#define CMD_OUT_ACK 8

typedef struct MessageQueue MessageQueue;
struct MessageQueue {
    MessageQueue* next;
    uint8_t cmd;
    char* data;
    uint32_t uuid;
    uint8_t attempts_left;
};

static void destroy_message_queue(MessageQueue* queue);
static void outbox_sent_callback(DictionaryIterator *iterator, void *context);
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context);
static void inbox_received_callback(DictionaryIterator *iterator, void *context);
static void send_next_message();
static void send_timer_callback(void* context);
static char *translate_error(AppMessageResult result);

static MessageHandler message_handler;
static MessageQueue* msg_queue = NULL;
static bool sending = false;
static bool can_send = false;

static uint32_t msg_uuid_hist[MSG_UUID_HIST_LEN] = {0};
static int8_t msg_uuid_hist_pos = 0;

void mq_init(MessageHandler handler) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Initializing mq");

    message_handler = handler;

    app_message_open(512, 768);
    app_message_register_outbox_sent(outbox_sent_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_inbox_received(inbox_received_callback);

    can_send = true;
    send_next_message();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "MQ init done");
}

bool mq_add(uint8_t cmd, char* data) {
    MessageQueue* mq = malloc(sizeof(MessageQueue));
    mq->next = NULL;
    mq->attempts_left = ATTEMPT_COUNT;
    mq->data = strdup(data);
    mq->cmd = cmd;
    mq->uuid = rand();

    if (msg_queue == NULL) {
        msg_queue = mq;
    } else {
        MessageQueue* eoq = msg_queue;
        while (eoq->next != NULL) {
            eoq = eoq->next;
        }
        eoq->next = mq;
    }

    APP_LOG(APP_LOG_LEVEL_DEBUG, "ADD: %u, %u, %s", cmd, (unsigned int)(mq->uuid), data);

    send_next_message();

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    sending = false;

    MessageQueue* sent = msg_queue;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "SENT: %u, %u, %s", (unsigned int)(sent->cmd), (unsigned int)(sent->uuid), sent->data);
    msg_queue = msg_queue->next;

    destroy_message_queue(sent);

    if (msg_queue) {
        app_timer_register(500, send_timer_callback, NULL);
    }
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    sending = false;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "ERROR: %u, %u, %s", (unsigned int)(msg_queue->cmd), (unsigned int)(msg_queue->uuid), msg_queue->data);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", translate_error(reason));

    app_timer_register(500, send_timer_callback, NULL);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "On MQ-receive");

    uint32_t uuid = 0;
    Tuple* uuid_tuple = dict_find(iterator, MSG_KEY_UUID);
    if (uuid_tuple) {
        uuid = uuid_tuple->value->uint32;
    }

    if (!uuid) {
        return; // uuid is missing
    }

    // Send ACK
    mq_fmt(CMD_OUT_ACK, 15, "%lu", uuid);

    // Check UUID
    for (int i = 0; i < MSG_UUID_HIST_LEN; i++) {
        if (msg_uuid_hist[i] == uuid) {
            return; // duplicate
        }
    }

    // Remember this UUID
    msg_uuid_hist[msg_uuid_hist_pos] = uuid;
    msg_uuid_hist_pos = (msg_uuid_hist_pos + 1) % MSG_UUID_HIST_LEN;

    // Do something useful
    message_handler(iterator);
}

static void destroy_message_queue(MessageQueue* queue) {
    free(queue->data);
    free(queue);
}

static void send_next_message() {
    if (!can_send) {
        return;
    }

    MessageQueue* mq = msg_queue;
    if (!mq) {
        return;
    }

    APP_LOG(APP_LOG_LEVEL_DEBUG, "SENDING: %u, %u, %s", (unsigned int)(mq->cmd), (unsigned int)(mq->uuid), mq->data);

    if (mq->attempts_left <= 0) {
        msg_queue = mq->next;
        destroy_message_queue(mq);
        send_next_message();
        return;
    }

    if (sending) {
        return;
    }

    sending = true;

    DictionaryIterator* dict;
    app_message_outbox_begin(&dict);
    dict_write_uint8(dict, MSG_KEY_CMD, mq->cmd);
    dict_write_cstring(dict, MSG_KEY_DATA, mq->data);
    dict_write_uint32(dict, MSG_KEY_UUID, mq->uuid);

    AppMessageResult result = app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "%s %d", translate_error(result), result);
    mq->attempts_left -= 1;
}

static char *translate_error(AppMessageResult result) {
    switch (result) {
        case APP_MSG_OK: return "APP_MSG_OK";
        case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
        case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
        case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
        case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
        case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
        case APP_MSG_BUSY: return "APP_MSG_BUSY";
        case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
        case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
        case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
        case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
        case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
        case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
        case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
        case APP_MSG_INVALID_STATE: return "APP_MSG_INVALID_STATE";
        default: return "UNKNOWN ERROR";
    }
}

static void send_timer_callback(void* context) {
    send_next_message();
}
