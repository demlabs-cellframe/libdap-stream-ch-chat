#include "ch_chat_text.h"

#define LOG_TAG "ch_chat_text"
#include "stream_ch_pkt.h"

void ch_chat_text_new(stream_ch_t * ch, void * arg);
void ch_chat_text_delete(stream_ch_t * ch, void * arg);
void ch_chat_text_packet_in(stream_ch_t * ch, void * arg);
void ch_chat_text_packet_out(stream_ch_t * ch, void * arg);

int ch_chat_text_init()
{
    log_it(INFO, "ch_chat_text_init()");
    stream_ch_proc_add('t',ch_chat_text_new,ch_chat_text_delete,
                       ch_chat_text_packet_in,ch_chat_text_packet_out);

    return 0;
}

void ch_chat_text_deinit()
{
    log_it(INFO, "ch_chat_text_deinit()");
}

void ch_chat_text_new(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_chat_text_new()");
}

void ch_chat_text_delete(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_chat_text_delete()");
}

void ch_chat_text_packet_in(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_chat_text_packet_in()");
    log_it(INFO, "Message = %s", ((stream_ch_pkt_t*)arg)->data );
}

void ch_chat_text_packet_out(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_chat_text_packet_out()");
}
