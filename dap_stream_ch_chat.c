#include "stream_ch_pkt.h"
#include "stream.h"
#include "stream_pkt.h"
#include "db_contact.h"
#include "db_chat.h"

#include "stream_ch_proc.h"

#include "ch_chat.h"

#define LOG_TAG "ch_chat"

void ch_chat_new(stream_ch_t * ch, void * arg);
void ch_chat_delete(stream_ch_t * ch, void * arg);
void ch_chat_packet_in(stream_ch_t * ch, void * arg);
void ch_chat_packet_out(stream_ch_t * ch, void * arg);
void ch_parse_command(stream_ch_t * ch, void * arg);
void parse_contact_and_domain(char* input_str, char** out_login, char** out_domain);
static void ch_chat_get_all_user_contacts(stream_ch_t * ch);
static void ch_chat_change_contact_list(stream_ch_t * ch, char* data, int op_code);
static void ch_chat_get_user_information(stream_ch_t * ch, char* data);
static void ch_chat_return_N_channel_msgs(stream_ch_t * ch, char* data);
static void ch_chat_get_message(stream_ch_t * ch, char* data);
static void ch_chat_get_contacts_domain(stream_ch_t * ch);
static void send_empty_pkt(stream_ch_t * ch, int op_code);
static void ch_chat_get_list_channels(stream_ch_t * ch);
static void ch_chat_subscribe_on_channel(stream_ch_t * ch);
static void ch_chat_get_channel_members(stream_ch_t * ch);
static void parse_login_and_destination_channel_name(stream_ch_t * ch,
                                                    char** login_out, char** channel_out);

int ch_chat_init()
{
    log_it(NOTICE, "ch chat text module init");
    stream_ch_proc_add('t',ch_chat_new,ch_chat_delete,
                       ch_chat_packet_in,ch_chat_packet_out);

    return 0;
}

void ch_chat_deinit()
{
    log_it(INFO, "ch_chat_deinit()");
}

void ch_chat_new(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_chat_new()");
}

void ch_chat_delete(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_chat_delete()");
}

void ch_chat_packet_in(stream_ch_t * ch, void * arg)
{
    switch (((stream_ch_pkt_t*)arg)->hdr.type) {
    case 'c': ch_parse_command(ch, arg); break;
    case 't': ch_chat_get_message(ch, arg); break; // 't' - message type
    default:
        break;
    }
    stream_ch_set_ready_to_write(ch,true);
}

void ch_chat_packet_out(stream_ch_t * ch, void * arg)
{
    stream_ch_set_ready_to_write(ch,false);
  //  log_it(INFO, "ch_chat_packet_out()");
}


/**
 * @brief ch_chat_get_message
 * @param ch
 * @param data
 * @details get message from client
 */
static void ch_chat_get_message(stream_ch_t * ch, char* data)
{
    DapChannelChatPkt_t* chat_pkt = (DapChannelChatPkt_t*) ((stream_ch_pkt_t*)data)->data;

    char *domain = strstr(ch->stream->pkt_buf_in->hdr.d_addr, "#");

    if(domain == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_SEND_MESSAGE_NOT_DELIVERED);
        return;
    }

    int length_name_channel = domain - ch->stream->pkt_buf_in->hdr.d_addr;
    char name_channel[length_name_channel];
    strncpy(name_channel, ch->stream->pkt_buf_in->hdr.d_addr, length_name_channel);
    name_channel[length_name_channel] = 0;

    if(save_massage_in_channel(name_channel, ch->stream->pkt_buf_in->hdr.s_addr,
                               (char*)chat_pkt->data))
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_SEND_MESSAGE_DELIVERED);
    else
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_SEND_MESSAGE_NOT_DELIVERED);

}

void ch_parse_command(stream_ch_t * ch, void * arg)
{
    log_it(INFO, "ch_parse_command()");
    DapChannelChatPkt_t* chat_pkt = (DapChannelChatPkt_t*) ((stream_ch_pkt_t*)arg)->data;

    switch (chat_pkt->header.op_code) {
    case CHAT_PACKET_OP_CODE_GET_ALL_CONTACTS:
        ch_chat_get_all_user_contacts(ch); break;
    case CHAT_PACKET_OP_CODE_ADD_CONTACT:
        ch_chat_change_contact_list(ch, (char*)chat_pkt->data, CHAT_PACKET_OP_CODE_ADD_CONTACT);
        break;
    case CHAT_PACKET_OP_CODE_DELETE_CONTACT:
        ch_chat_change_contact_list(ch, (char*)chat_pkt->data, CHAT_PACKET_OP_CODE_DELETE_CONTACT);
        break;
    case CHAT_PACKET_OP_CODE_GET_USER_INFORMATION:
        ch_chat_get_user_information(ch, (char*)chat_pkt->data);
        break;
    case CHAT_PACKET_OP_CODE_GET_LAST_N_MESSAGE_IN_CHANNEL:
        ch_chat_return_N_channel_msgs(ch, (char*)chat_pkt->data);
        break;
    case CHAT_PACKET_OP_CODE_GET_CONCACTS_DOMAIN:
        ch_chat_get_contacts_domain(ch);
        break;
    case CHAT_PACKET_OP_CODE_GET_LIST_CHANNEL:
        ch_chat_get_list_channels(ch);
        break;
    case CHAT_PACKET_OP_CODE_GET_SUBSCRIBE_ON_CHANNEL:
        ch_chat_subscribe_on_channel(ch);
        break;
    case CHAT_PACKET_OP_CODE_GET_CHANNEL_MEMBERS:
        ch_chat_get_channel_members(ch);
        break;
    default:
        log_it(WARNING, "Unknown op_code");
        break;
    }
}

static void ch_chat_get_channel_members(stream_ch_t * ch)
{
    char *str_ptr = strchr(ch->stream->pkt_buf_in->hdr.d_addr, '#');
    if(str_ptr == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_CHANNEL_MEMBERS_ERROR);
        log_it(WARNING, "Error get members channel");
        return;
    }

    int len = (int) (str_ptr - ch->stream->pkt_buf_in->hdr.d_addr);

    char name_channel[len];
    strncpy(name_channel, ch->stream->pkt_buf_in->hdr.d_addr, len);
    name_channel[len] = '\0';

    char *channel_id;

    db_get_id_by_name_channel(name_channel, &channel_id);

    if(channel_id == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_CHANNEL_MEMBERS_ERROR);
        log_it(WARNING, "Error get members channel");
        return;
    }

    char *subscribers_id;
    get_subscribers_on_channel(channel_id, &subscribers_id);

    if(subscribers_id == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_CHANNEL_MEMBERS_ERROR);
        log_it(WARNING, "Error get members channel");
        return;
    }

    const int MAX_TOKEN_SIZE = 90;

    DapChannelChatPkt_t *ch_pkt_out = (DapChannelChatPkt_t *) calloc(1,
                (strlen(subscribers_id) / 24) * MAX_TOKEN_SIZE + 160 + sizeof(ch_pkt_out->header) );

    ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_CHANNEL_MEMBERS;

    char token[MAX_TOKEN_SIZE];
    strcpy(ch_pkt_out->data,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n");
    sprintf(token, "<channel_subscribers name_channel=\"%s\">\n",name_channel);
    strcat(ch_pkt_out->data,token);

    char* sep = "\n";
    char* id = strtok (subscribers_id, sep);
    char* login_user;

    while (id != NULL)
    {
        db_get_login_by_id(id, &login_user);

        sprintf(token, "<member><id>%s</id><login>%s</login></member>", id, login_user);
        strcat(ch_pkt_out->data, token);

        id = strtok (NULL,sep);
        free(login_user);
    }

    strcat(ch_pkt_out->data, "</channel_subscribers>\n");

    stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);

    free(channel_id); free(subscribers_id); free(ch_pkt_out);
}

static void ch_chat_subscribe_on_channel(stream_ch_t * ch)
{
    char *login, *name_channel;
    parse_login_and_destination_channel_name(ch, &login, &name_channel);

    if ( login == NULL || name_channel == NULL)
    {
        free(login); free(name_channel);
        send_empty_pkt(ch,CHAT_PACKET_OP_CODE_ERROR_SUBSCRIBE_ON_CHANNEL);
        log_it(WARNING,"Bad request subscribe on channel");
        return;
    }

    char* user_id, *channel_id;
    db_get_id_by_login(login, &user_id);
    db_get_id_by_name_channel(name_channel, &channel_id);

    if ( user_id == NULL || channel_id == NULL)
    {
        free(user_id); free(channel_id);
        send_empty_pkt(ch,CHAT_PACKET_OP_CODE_ERROR_SUBSCRIBE_ON_CHANNEL);
        log_it(WARNING,"Bad request subscribe on channel");
        return;
    }

    if ( subscribe_on_channel(channel_id, user_id) )
        send_empty_pkt(ch,CHAT_PACKET_OP_CODE_SUBSCRIBE_ON_CHANNEL_OK);
    else
        send_empty_pkt(ch,CHAT_PACKET_OP_CODE_ERROR_SUBSCRIBE_ON_CHANNEL);

    free(channel_id); free(user_id);
}

static void ch_chat_get_list_channels(stream_ch_t * ch)
{
    char *xml_out;
    list_channels(&xml_out);
    DapChannelChatPkt_t *ch_pkt_out = (DapChannelChatPkt_t *) calloc(1, strlen(xml_out)
                                           + sizeof(ch_pkt_out->header) );
    ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_LIST_CHANNEL;

    strcpy(ch_pkt_out->data, xml_out);
    stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);

    free(xml_out);
    free(ch_pkt_out);
}

static void ch_chat_get_contacts_domain(stream_ch_t * ch)
{
    log_it(INFO, "ch_chat_get_contacts_domain");

    // Get Domain from Source add client
    char* domain = strstr(ch->stream->pkt_buf_in->hdr.s_addr, "@");
    char* contacts;

    get_all_contacts_in_domain(domain + 1, &contacts);

    if(contacts == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_CONCACTS_DOMAIN_FAIL);
        return;
    }

    DapChannelChatPkt_t *ch_pkt_out = (DapChannelChatPkt_t *) calloc(1, ( strlen(contacts) * 3 + 150 )
                                           + sizeof(ch_pkt_out->header));

    ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_CONCACTS_DOMAIN;

    strcpy(ch_pkt_out->data, contacts);

    stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);

    free(contacts);
    free(ch_pkt_out);
}

static void ch_chat_return_N_channel_msgs(stream_ch_t * ch, char* data)
{
    DapChannelChatPkt_t* ch_pkt_out = NULL;

    char *domain = strstr(ch->stream->pkt_buf_in->hdr.d_addr, "#");

    if(domain == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_LAST_N_MESSAGE_IN_CHANNEL_FAIL);
        return;
    }

    int length_name_channel = domain - ch->stream->pkt_buf_in->hdr.d_addr;
    char name_channel[length_name_channel];
    strncpy(name_channel, ch->stream->pkt_buf_in->hdr.d_addr, length_name_channel);
    name_channel[length_name_channel] = 0;

    int N = atoi(data);

    if( N == 0 )
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_LAST_N_MESSAGE_IN_CHANNEL_FAIL);
        return;
    }

    char *xml_out;
    get_last_N_messages(N, name_channel, &xml_out);

    if(xml_out == NULL)
    {
        send_empty_pkt(ch, CHAT_PACKET_OP_CODE_GET_LAST_N_MESSAGE_IN_CHANNEL_FAIL);
        return;
    }

    ch_pkt_out = (DapChannelChatPkt_t *) calloc(1, strlen(xml_out)
                                                + sizeof(ch_pkt_out->header));

    ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_LAST_N_MESSAGE_IN_CHANNEL;
    strcpy(ch_pkt_out->data,xml_out);
    stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);

    free(ch_pkt_out);
    free(xml_out);
    return;

}

static void send_empty_pkt(stream_ch_t * ch, int op_code)
{
    DapChannelChatPkt_t * ch_pkt_out = (DapChannelChatPkt_t *) calloc (1,sizeof(ch_pkt_out->header) + 30);
    ch_pkt_out->header.op_code = op_code;
    stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);
    free(ch_pkt_out);
}

static void ch_chat_get_user_information(stream_ch_t * ch, char* data)
{
    char *xml_out;
    DapChannelChatPkt_t* ch_pkt_out =
            (DapChannelChatPkt_t *) calloc(1, 512 + sizeof(ch_pkt_out->header));

    db_get_profile_information(data, &xml_out);

    if(xml_out == NULL) // login not found
    {
        ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_USER_INFORMATION_FAIL;
    }
    else
    {
        strcpy(ch_pkt_out->data,xml_out);
        ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_USER_INFORMATION;
    }

    stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);

    free(ch_pkt_out); free(xml_out);
}


static void ch_chat_change_contact_list(stream_ch_t * ch, char* data, int op_code)
{

    log_it(INFO, "data = %s", data);

    int length_login = strpbrk(ch->stream->pkt_buf_in->hdr.s_addr, "@") -
            ch->stream->pkt_buf_in->hdr.s_addr;

    char* login = (char*) malloc (length_login);
    strncpy(login,ch->stream->pkt_buf_in->hdr.s_addr, length_login);
    login[length_login] = '\0';

    DapChannelChatPkt_t* ch_pkt_out =
            (DapChannelChatPkt_t *) calloc(1, 30 + sizeof(ch_pkt_out->header));
    ch_pkt_out->header.op_code = op_code;

    switch (ch_pkt_out->header.op_code) {
    case CHAT_PACKET_OP_CODE_ADD_CONTACT:
    {
        if( db_add_contact(login,data) )
            strcpy((char*)ch_pkt_out->data, "Contact successfully added");
        else
            strcpy((char*)ch_pkt_out->data, "Contact not added");

        stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);
        break;
    }
    case CHAT_PACKET_OP_CODE_DELETE_CONTACT:

        if( db_delete_contact(login, data) )
            strcpy((char*)ch_pkt_out->data, "Contact successfully removed");
        else
            strcpy((char*)ch_pkt_out->data, "Error removed");

        stream_ch_pkt_write_f(ch,'t',(const char*)ch_pkt_out, NULL);
        break;
    default:
        log_it(WARNING,"Unknown op_code");
        break;
    }

    free(login);
    free(ch_pkt_out);
}

static void ch_chat_get_all_user_contacts(stream_ch_t * ch)
{
    char *login, *domain;
    parse_contact_and_domain(ch->stream->pkt_buf_in->hdr.s_addr, &login, &domain);

    DapChannelChatPkt_t* ch_pkt_out;
    char* contacts;
    db_get_contacts(login, &contacts);
    if(contacts == NULL) // Contact list is clear
    {
        ch_pkt_out = (DapChannelChatPkt_t *) calloc(1, 100 + sizeof(ch_pkt_out->header));

        strcpy(ch_pkt_out->data,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
                                  "<user_contacts>\n</user_contacts>");
        ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_ALL_CONTACTS;

        stream_ch_pkt_write_f(ch,'t',(const char*) ch_pkt_out, NULL);
        free(login);
        free(domain);
        free(ch_pkt_out);
        return;
    }

    ch_pkt_out = (DapChannelChatPkt_t *) calloc(1, ( strlen(contacts) * 3 + 150 )
                                           + sizeof(ch_pkt_out->header));

    ch_pkt_out->header.op_code = CHAT_PACKET_OP_CODE_GET_ALL_CONTACTS;

    strcpy(ch_pkt_out->data,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
           "<user_contacts>\n");

    char* sep = "\n";
    char* istr = strtok (contacts,sep);
    char temp_str[128];
    char *out_login;
    while (istr != NULL)
    {
        // WARNING NEED CHANGE THIS CALL FUNC CODE AND TEST
        db_get_login_by_id(istr, &out_login);

        sprintf(temp_str, "<contact><id>%s</id><login>%s</login></contact>",
                istr, out_login);

        strcat(ch_pkt_out->data, temp_str);
        istr = strtok (NULL,sep);
    }

    strcat(ch_pkt_out->data, "</user_contacts>");
    stream_ch_pkt_write_f(ch,'t', (const char*) ch_pkt_out, NULL);

    free(login);
    free(domain);
    free(contacts);
    free(ch_pkt_out);
}

/**
 * @brief parse_contact_and_domain
 * @param input_str
 * @param out_login
 * @param out_domain
 * @example input: user@domain.com output: out_login = user, out_domain = domain.com
 */
void parse_contact_and_domain(char* input_str, char** out_login, char** out_domain)
{
    int length_login = strpbrk(input_str, "@") - input_str;

    *out_login = (char*) malloc (length_login);
    *out_domain = (char*) malloc (strlen(input_str) - length_login - 1);

    strncpy(*out_login,input_str, length_login);
    (*out_login)[length_login] = '\0';

    strcpy(*out_domain, input_str + length_login + 1);
}

static void parse_login_and_destination_channel_name(stream_ch_t * ch,
                                                    char** login_out, char** channel_out)
{
    *login_out = NULL; *channel_out = NULL;
    char *str_ptr = strchr(ch->stream->pkt_buf_in->hdr.s_addr, '@');
    if(str_ptr == NULL)
        return;

    int len = (int) (str_ptr - ch->stream->pkt_buf_in->hdr.s_addr);
    *login_out = (char*) malloc(len);
    strncpy(*login_out, ch->stream->pkt_buf_in->hdr.s_addr, len);
    (*login_out)[len] = '\0';

    str_ptr = strchr(ch->stream->pkt_buf_in->hdr.d_addr, '#');
    if(str_ptr == NULL)
        return;

    len = (int) (str_ptr - ch->stream->pkt_buf_in->hdr.d_addr);

    *channel_out = (char*) malloc(len);
    strncpy(*channel_out, ch->stream->pkt_buf_in->hdr.d_addr, len);
    (*channel_out)[len] = '\0';
}
