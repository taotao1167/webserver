#ifndef __TT_SESSION_H__
#define __TT_SESSION_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HTTP_SESSION_ID_MAX_LEN
#define HTTP_SESSION_ID_MAX_LEN 40 /*session_id最大长度*/
#endif
#ifndef HTTP_SESSION_MAX_HOLD
#define HTTP_SESSION_MAX_HOLD 60 /* 离线会话保持时长，单位秒，默认60秒 */
#endif
#ifndef HTTP_SESSION_TIMEOUT
#define HTTP_SESSION_TIMEOUT 300 /* 已登陆会话超时时长，单位秒，默认300秒 */
#endif
#ifndef HTTP_SESSION_MAX
#define HTTP_SESSION_MAX 100 /* 最大session数量，默认100 */
#endif

/* 某一会话存储的信息键值对链表结构体 */
typedef struct ST_SESSION_STORAGE{
	char *key; /* 标识 */
	void *value; /* 存储的内容 */
	struct ST_SESSION_STORAGE *next;
}SESSION_STORAGE;

/* 会话结构体 */
typedef struct ST_HTTP_SESSION{
	char session_id[HTTP_SESSION_ID_MAX_LEN + 1];
	char ip[48]; /* 客户端IP地址 */
	unsigned char isonline; /* 会话是否已登陆 */
	time_t create_time; /* 会话创建时间 */
	time_t login_time; /* 会话登陆时间 */
	time_t active_time; /* 会话最后活动时间 */
	time_t heartbeat_expire; /* 心跳超时时间 */
	time_t expire; /* 会话超时时间 */
	struct ST_SESSION_STORAGE *storage; /* 会话数据存储位置 */
	struct ST_HTTP_SESSION *next;
}HTTP_SESSION;

extern HTTP_SESSION *g_http_sessions;

extern int set_session(HTTP_FD *p_link);
extern int session_login(HTTP_SESSION *p_session, const char *uname, const int level);
extern int session_logout(HTTP_SESSION *p_session);
extern int session_set_storage(HTTP_SESSION *p_session, const char *name, const char *value);
extern int session_unset_storage(HTTP_SESSION *p_session, const char *name);
extern const char *session_get_storage(HTTP_SESSION *p_session, const char *key, const char *default_value);
extern int session_free_storage(SESSION_STORAGE **p_head);
extern void update_session_expires(HTTP_FD *p_link);
extern void session_timeout_check(void);

#ifdef __cplusplus
}
#endif

#endif
