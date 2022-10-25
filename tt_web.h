#ifndef __TT_WEB_H__
#define __TT_WEB_H__

#include <stdarg.h>
#include <time.h>

#ifndef __TT_BUFFER_H__
#include "tt_buffer.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum e_http_state{
	STATE_RECVING = 1, /* data recving */
	STATE_SUSPEND, /* suspend this line until trigger target event */
	STATE_RESPONSING, /* generating response content */
	STATE_SENDING, /* sending data to peer */
	STATE_CLOSING, /* sending data to peer, close linke when all data is sended */
	STATE_CLOSED, /* nothing is need to do, should close link and free resource */
#ifdef WITH_WEBSOCKET
	STATE_WS_HANDSHAKE, /* responsing websocket handshake package */
	STATE_WS_CONNECTED, /* websocket is connected */
#endif
}E_HTTP_STATE;

typedef enum e_http_send_state{
	SENDING_HEAD = 1, /* sending http head */
	SENDING_ENTITY, /* sending http entity */
}E_HTTP_SEND_STATE;

#ifdef WITH_WEBSOCKET
typedef enum e_ws_event{
	EVENT_ONOPEN = 1, /* open */
	EVENT_ONMESSAGE, /* recv a package */
	EVENT_ONCLOSE, /* close */
	EVENT_ONERROR, /* error */
	EVENT_ONPING, /* recv a PING package */
	EVENT_ONPONG, /* recv a PONG package */
	EVENT_ONCHUNKED, /* recv a chunked package */
}E_WS_EVENT;

typedef enum e_ws_opcode{
	WS_OPCODE_CONTINUATION = 0x00,
	WS_OPCODE_TEXT = 0x01,
	WS_OPCODE_BINARY = 0x02,
	WS_OPCODE_CLOSE = 0x08,
	WS_OPCODE_PING = 0x09,
	WS_OPCODE_PONG = 0x0A,
}E_WS_OPCODE;
#endif

#define RANGE_NOTSET (size_t)-1

/* key value pair */
typedef struct ST_HTTP_KVPAIR{
	char *key; /* key */
	char *value; /* value (decoded) */
	struct ST_HTTP_KVPAIR *next;
}HTTP_KVPAIR;

typedef struct ST_HTTP_RANGE{
	size_t start;
	size_t end;
	struct ST_HTTP_RANGE *next;
}HTTP_RANGE;

typedef struct ST_HTTP_FILES{
	char *key; /* key */
	char *fname; /* file name */
	char *ftype; /* file type */
	unsigned char *fcontent; /* file content, not malloc another space for less use ram */
	unsigned int fsize; /* file size */
	struct ST_HTTP_FILES *next;
}HTTP_FILES;

struct ST_HTTP_SESSION;
struct ST_SERVER;
typedef struct ST_HTTP_FD {
	void *backendio;
	char ip_peer[48]; /* ip address of peer */
	char ip_local[48]; /* ip address of local */
	unsigned short port_peer; /* port number of peer, host bytes order */
	unsigned short port_local; /* port number of local, host bytes order */
	unsigned char *recvbuf; /* point to content of recved */
	unsigned int recvbuf_space; /* space size of recvbuf */
	unsigned int recvbuf_len; /* used space of recvbuf*/
	unsigned int content_len; /* parsed Content-Length form http header, save it instead of parse header at every single recv */
	unsigned int need_len; /* data length that still need to recv, init 0 */
	E_HTTP_STATE state; /* link state */
	time_t tm_last_active; /* the time of last active */
	time_t tm_last_req; /* the time of last request */
	char *method; /* request method (GET/POST/HEAD/OPTIONS/PUT/..) */
	char *path; /* request path (query info not included) */
	char *http_version; /* http version (HTTP/1.1) */
	struct ST_HTTP_KVPAIR *header_data; /* parsed head */
	struct ST_HTTP_KVPAIR *cookie_data; /* parsed cookie */
	struct ST_HTTP_RANGE *range_data; /* parsed range result */
	struct ST_HTTP_KVPAIR *query_data; /* parsed query info included in url */
	struct ST_HTTP_KVPAIR *post_data; /* parsed post info */
	struct ST_HTTP_FILES *file_data; /* parsed file info */
	struct ST_HTTP_KVPAIR *cnf_header; /* saved content by call web_set_header */
	E_HTTP_SEND_STATE send_state; /* sending state */
	TT_BUFFER response_head; /* header of response */
	TT_BUFFER response_entity; /* entity of response */
#ifdef WITH_WEBSOCKET
	TT_BUFFER ws_recvq; /* recved raw (not decoded) data from websocket */
	TT_BUFFER ws_sendq; /* packed data to send by websocket */
	TT_BUFFER ws_data; /* decoded data from websocket */
	TT_BUFFER ws_response; /* raw data that not packed to send by websocket */
#endif
	unsigned int sending_len; /* indicate the length header, entity or ws_sendq is sending, 0 means socket is idle. */
	int (* send_cb)(struct ST_HTTP_FD *p_link);
	struct ST_WEB_SERVER *server;
	struct ST_HTTP_SESSION *session;
	struct ST_HTTP_FD *prev;
	struct ST_HTTP_FD *next;
	void *userdata;
	void (*free_userdata)(void *userdata);
} HTTP_FD;

typedef struct ST_WEB_SERVER {
	int id;
	char *name;
	char *root;
	int ip_version;
	unsigned short port;
#ifdef WITH_SSL
	unsigned char is_ssl;
#endif
	void *backend;
	int (* req_dispatch)(HTTP_FD p_link);
#ifdef WITH_WEBSOCKET
	int (* ws_dispatch)(HTTP_FD *p_link, E_WS_EVENT evt);
#endif
	struct ST_WEB_SERVER *prev;
	struct ST_WEB_SERVER *next;
}WEB_SERVER;

extern HTTP_FD *g_http_links;
extern WEB_SERVER *g_servers;

extern int init_webserver();
extern void *web_server_thread(void *para);
extern WEB_SERVER *create_http(const char *name, int ip_version, unsigned short port, const char *root);
#ifdef WITH_SSL
extern WEB_SERVER *create_https(const char *name, int ip_version, unsigned short port, const char *root, const char *crt_file, const char *key_file);
#endif
extern int destroy_server(WEB_SERVER *p_svr);
extern int destroy_server_by_id(int svr_id);

extern const char *get_mime_type(const char *p_path);
extern const char *htmlencode(const char * src);
extern const char *web_header_str(HTTP_FD *p_link, const char *key, const char *default_value);
extern const char *web_cookie_str(HTTP_FD *p_link, const char *key, const char *default_value);
extern const char *web_query_str(HTTP_FD *p_link, const char *key, const char *default_value);
extern const char *web_post_str(HTTP_FD *p_link, const char *key, const char *default_value);
extern const HTTP_FILES *web_file_data(HTTP_FD *p_link, const char *key);
extern HTTP_FILES *web_file_list(HTTP_FD *p_link, const char *key);
extern int web_file_list_free(HTTP_FILES **p_head);
extern int web_set_header(HTTP_FD *p_link, const char *name, const char *value);
extern int web_unset_header(HTTP_FD *p_link, const char *name);
extern int web_vprintf(HTTP_FD *p_link, const char *format, va_list args);
extern int web_printf(HTTP_FD *p_link, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern int web_write(HTTP_FD *p_link, const void *content, unsigned int content_len);
extern int web_no_copy(HTTP_FD *p_link, void *content, unsigned int used, unsigned int space, int is_malloced);
extern int web_fin(HTTP_FD *p_link, int http_code);
extern int web_busy_response(HTTP_FD *p_link);
#ifdef WITH_WEBSOCKET
extern int ws_handshake(HTTP_FD *p_link);
extern int ws_vprintf(HTTP_FD *p_link, const char *format, va_list args);
extern int ws_printf(HTTP_FD *p_link, const char *format, ...) __attribute__((format(printf, 2, 3)));
extern int ws_write(HTTP_FD *p_link, const unsigned char *content, unsigned int content_len);
extern int ws_pack(HTTP_FD *p_link, unsigned char opcode);
#endif
extern int set_pollingfunc(void (*pollingfunc)(void));
extern int notify_web_getvalue(long int *len);
extern int sync_call(const char *name, void *payload, size_t payload_len);
extern int notify_web(const char *name, void *payload, size_t payload_len, int (*callback)(int ret, void *arg), void *arg);

#ifdef __cplusplus
}
#endif

#endif

