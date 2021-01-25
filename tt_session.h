#ifndef __TT_SESSION_H__
#define __TT_SESSION_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HTTP_SESSION_ID_MAX_LEN
#define HTTP_SESSION_ID_MAX_LEN 40 /* max length of session_id */
#endif
#ifndef HTTP_SESSION_MAX_HOLD
#define HTTP_SESSION_MAX_HOLD 60 /* hold duration of offline session, uint: seconds, default: 60 */
#endif
#ifndef HTTP_SESSION_TIMEOUT
#define HTTP_SESSION_TIMEOUT 300 /* hold duration of online session, uint: seconds, default: 300 */
#endif
#ifndef HTTP_SESSION_MAX
#define HTTP_SESSION_MAX 0 /* max count of session, 0 means no limit, default: 0 */
#endif

typedef struct ST_SESSION_STORAGE{
	char *key;
	void *value;
	struct ST_SESSION_STORAGE *next;
}SESSION_STORAGE;

/* struct of session */
typedef struct ST_HTTP_SESSION{
	char session_id[HTTP_SESSION_ID_MAX_LEN + 1];
	char ip[48]; /* ip address of client */
	unsigned char isonline; /* is online or not */
	time_t create_time; /* time of create */
	time_t login_time; /* time of login */
	time_t active_time; /* time of last active */
	time_t heartbeat_expire; /* time of heartbeat expire */
	time_t expire; /* time of expire */
	struct ST_SESSION_STORAGE *storage; /* saved data of session */
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
