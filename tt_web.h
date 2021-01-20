#ifndef __TT_WEB_H__
#define __TT_WEB_H__

#include <stdarg.h>
#include <time.h>
#include <event2/listener.h>
#ifdef WITH_SSL
	#include <openssl/ssl.h>
	#include <openssl/err.h>
#endif

#ifndef __TT_BUFFER_H__
#include "tt_buffer.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum e_http_state{
	STATE_RECVING = 1, /* 接收请求中 */
	STATE_SUSPEND, /* 挂起连接，等待其他事件触发，这是为后续扩展考虑的，目前没用到 */
	STATE_RESPONSING, /* 生成响应中 */
	STATE_SENDING, /* 返回响应中 */
	STATE_CLOSING, /* 需要将内容发送完后断开连接 */
	STATE_CLOSED, /* 连接已经可以断开，所有资源也可回收 */
#ifdef WITH_WEBSOCKET
	STATE_WS_HANDSHAKE, /* websocket握手包回复中 */
	STATE_WS_CONNECTED, /* websocket已成功连接 */
#endif
}E_HTTP_STATE;

typedef enum e_http_send_state{
	SENDING_HEAD = 1, /* 发送消息头中 */
	SENDING_ENTITY, /* 发送消息体中 */
}E_HTTP_SEND_STATE;

#ifdef WITH_WEBSOCKET
typedef enum e_ws_event{
	EVENT_ONOPEN = 1, /* 连接事件 */
	EVENT_ONMESSAGE, /* 消息事件 */
	EVENT_ONCLOSE, /* 关闭事件 */
	EVENT_ONERROR, /* 出错事件 */
	EVENT_ONPING, /* 接受到PING包 */
	EVENT_ONPONG, /* 接受到PONG包 */
	EVENT_ONCHUNKED, /* 接受到分块 */
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

#define RANGE_NOTSET -1

/* 表示字符串形式键值对的链表结构体 */
typedef struct ST_HTTP_KVPAIR{
	char *key; /* 标识 */
	char *value; /* 解码后的内容 */
	struct ST_HTTP_KVPAIR *next;
}HTTP_KVPAIR;

typedef struct ST_HTTP_RANGE{
	size_t start;
	size_t end;
	struct ST_HTTP_RANGE *next;
}HTTP_RANGE;

typedef struct ST_HTTP_FILES{
	char *key; /* 标识 */
	char *fname; /* 文件名 */
	char *ftype; /* 文件类型 */
	unsigned char *fcontent; /* 存储文件内容，为节省内存空间，不单独为文件内容malloc */
	unsigned int fsize; /* 文件大小 */
	struct ST_HTTP_FILES *next;
}HTTP_FILES;

struct ST_HTTP_SESSION;
struct ST_SERVER;
typedef struct ST_HTTP_FD {
	struct bufferevent *bev;
	char ip_peer[48]; /* ip address of peer */
	char ip_local[48]; /* ip address of local */
	unsigned short port_peer; /* port number of peer, host bytes order */
	unsigned short port_local; /* port number of local, host bytes order */
	unsigned char *recvbuf; /* point to content of recved */
	unsigned int recvbuf_space; /* space size of recvbuf */
	unsigned int recvbuf_len; /* used space of recvbuf*/
	unsigned int content_len; /* parsed Content-Length form http header, save it instead of parse header at every single recv */
	unsigned int need_len; /* 消息还需要再接收的长度，初始为0 */
	E_HTTP_STATE state; /* link state */
	time_t tm_last_active; /* 连接的最后活动时间 */
	time_t tm_last_req; /* 最后一个请求开始的时间 */
	char *method; /* request method (GET/POST/HEAD/OPTIONS/PUT/..) */
	char *path; /* request path (query info not included) */
	char *http_version; /* http version (HTTP/1.1) */
	struct ST_HTTP_KVPAIR *header_data; /* 请求的头域解析的结果 */
	struct ST_HTTP_KVPAIR *cookie_data; /* parse cookie result */
	struct ST_HTTP_RANGE *range_data; /* parsed range result */
	struct ST_HTTP_KVPAIR *query_data; /* 请求的url中带的query信息解析的结果 */
	struct ST_HTTP_KVPAIR *post_data; /* post数据解析的结果 */
	struct ST_HTTP_FILES *file_data; /* 上传的文件解析的结果 */
#ifdef WITH_SSL
	SSL *ssl; /* 与客户端通信使用的SSL */
#endif
	struct ST_HTTP_KVPAIR *cnf_header; /* 为响应消息设置的响应头的保存位置 */
	E_HTTP_SEND_STATE send_state; /* 消息发送状态 */
	TT_BUFFER response_head; /* header of response,  */
	TT_BUFFER response_entity; /* entity of response */
#ifdef WITH_WEBSOCKET
	TT_BUFFER ws_recvq; /* websocket接收队列，存储接收的未解包的原始数据，已解包的内容使用memmove移除，不能复用recvbuf(因为各键值对都依赖recvbuf的内容) */
	TT_BUFFER ws_sendq; /* websocket发送队列，存储待发送的已封包的原始数据，已发送的内容使用memmove移除，不能和响应消息体共用response_entity */
	TT_BUFFER ws_data; /* websocket接收的原始数据解包后的结果 */
	TT_BUFFER ws_response; /* websocket待发送的原始数据封包前的结果 */
#endif
	unsigned int sending_len; /* indicate the length header, entity or ws_sendq is sending, 0 means socket is idle. */
	int (* send_cb)(struct ST_HTTP_FD *p_link);
	struct ST_WEB_SERVER *server;
	struct ST_HTTP_SESSION *session;
	struct ST_HTTP_FD *prev;
	struct ST_HTTP_FD *next;
	void *user_data;
}HTTP_FD;

typedef struct ST_WEB_SERVER {
	int id;
	char *name;
	char *root;
	int ip_version;
	unsigned short port;
#ifdef WITH_SSL
	unsigned char is_ssl;
	SSL_CTX *ssl_ctx;
#endif
	struct evconnlistener *listener;
	int (* req_dispatch)(HTTP_FD p_link);
#ifdef WITH_WEBSOCKET
	int (* ws_dispatch)(HTTP_FD *p_link, E_WS_EVENT evt);
#endif
	struct ST_WEB_SERVER *prev;
	struct ST_WEB_SERVER *next;
}WEB_SERVER;

#ifdef WITH_SSL
extern SSL_CTX *g_default_ssl_ctx;
#endif
extern HTTP_FD *g_http_links;
extern WEB_SERVER *g_servers;

extern int init_webserver();
extern void *web_server_thread(void *para);
WEB_SERVER *create_http(const char *name, int ip_version, unsigned short port, const char *root);
#ifdef WITH_SSL
WEB_SERVER *create_https(const char *name, int ip_version, unsigned short port, const char *root, SSL_CTX *ssl_ctx);
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
extern int notify_web(const char *name, void *buf, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif

