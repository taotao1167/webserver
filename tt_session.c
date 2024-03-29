#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifndef __TT_WEB_H__
#include "tt_web.h"
#endif
#ifndef __TT_SESSION_H__
#include "tt_session.h"
#endif
#ifndef __TT_SHA1_H__
#include "tt_sha1.h"
#endif

#ifdef WATCH_RAM
#include "tt_malloc_debug.h"
#define MY_MALLOC(x) my_malloc((x), __FILE__, __LINE__)
#define MY_FREE(x) my_free((x), __FILE__, __LINE__)
#define MY_REALLOC(x, y) my_realloc((x), (y), __FILE__, __LINE__)
#else
#define MY_MALLOC(x) malloc((x))
#define MY_FREE(x) free((x))
#define MY_REALLOC(x, y) realloc((x), (y))
#endif

HTTP_SESSION *g_http_sessions = NULL;
int g_session_count = 0;

/* add new session to g_http_sessions */
static HTTP_SESSION *append_session(const char *session_id, char *ip) {
	HTTP_SESSION *new_session = NULL, *p_tail = NULL;
	new_session = (HTTP_SESSION *)MY_MALLOC(sizeof(HTTP_SESSION));
	if (new_session == NULL) {
		return NULL;
	}
	memset(new_session, 0x00, sizeof(HTTP_SESSION));
	strcpy(new_session->session_id, session_id);
	strcpy(new_session->ip, ip);
	new_session->create_time = new_session->active_time = time(0);
	new_session->expire = new_session->create_time + HTTP_SESSION_MAX_HOLD;
	if (g_http_sessions == NULL) {
		g_http_sessions = new_session;
	} else {
		for (p_tail = g_http_sessions; p_tail->next != NULL; p_tail = p_tail->next);
		p_tail->next = new_session;
	}
	g_session_count++;
	return new_session;
}
static void free_storage_item(SESSION_STORAGE *storage) {
	if (storage == NULL) {
		return;
	}
	if (storage->key != NULL) {
		MY_FREE(storage->key);
	}
	if (storage->value != NULL && storage->value_free) {
		storage->value_free(storage->value);
	}
	MY_FREE(storage);
}
int session_unset_storage(HTTP_SESSION *p_session, const char *name) {
	SESSION_STORAGE *p_pre = NULL, *p_cur = NULL, *p_next = NULL;

	if (!name || name[0] == '\0') {
		return -1;
	}
	p_pre = NULL;
	for (p_cur = p_session->storage; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		if (0 == strcmp(p_cur->key, name)) {
			if (p_pre == NULL) {
				p_session->storage = p_next;
			} else {
				p_pre->next = p_next;
			}
			free_storage_item(p_cur);
		} else {
			p_pre = p_cur;
		}
	}
	return 0;
}
int session_set_storage(HTTP_SESSION *p_session, const char *name, void *value, void (*value_free)(void *)) {
	int ret = -1;
	SESSION_STORAGE *p_new = NULL, *p_tail = NULL;

	if (name == NULL || name[0] == '\0') {
		goto func_end;
	}
	session_unset_storage(p_session, name); /* cover the old data */
	/* add information to p_session->storage */
	p_new = (SESSION_STORAGE *)MY_MALLOC(sizeof(SESSION_STORAGE));
	if (p_new == NULL) {
		goto func_end;
	}
	memset(p_new, 0x00, sizeof(SESSION_STORAGE));
	p_new->value = value;
	p_new->value_free = value_free;
	p_new->key = strdup(name);
	if (p_new->key == NULL) {
		goto func_end;
	}
	if (p_session->storage == NULL) {
		p_session->storage = p_new;
	} else {
		for (p_tail = p_session->storage; ; p_tail = p_tail->next) {
			if (p_tail->next == NULL) {
				p_tail->next = p_new;
				break;
			}
		}
	}
	ret = 0;
func_end:
	if (ret != 0) {
		if (p_new != NULL) {
			free_storage_item(p_new);
		} else {
			if (value_free != NULL) {
				value_free(value);
			}
		}
	}
	return ret;
}
void *session_get_storage(HTTP_SESSION *p_session, const char *key, void *default_value) {
	SESSION_STORAGE *p_cur = NULL;
	for (p_cur = p_session->storage; p_cur != NULL; p_cur = p_cur->next) {
		if (0 == strcmp(p_cur->key, key)) {
			return p_cur->value;
		}
	}
	return default_value;
}
/* free all items and it's space */
int session_free_storage(SESSION_STORAGE **p_head) {
	SESSION_STORAGE *p_cur = NULL, *p_next = NULL;
	for (p_cur = *p_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		free_storage_item(p_cur);
	}
	*p_head = NULL;
	return 0;
}

/* free session and it's resource */
static int session_free(HTTP_SESSION **p_session) {
	if ((*p_session)->storage) {
		session_free_storage(&((*p_session)->storage));
	}
	MY_FREE(*p_session);
	*p_session = NULL;
	g_session_count--;
	return 0;
}

int session_login(HTTP_SESSION *p_session, const char *uname, const int level) {
	time_t now;
	now = time(0);
	p_session->heartbeat_expire = now + 12;
	p_session->isonline = 1;
	p_session->login_time = now;
	p_session->uname = strdup(uname);
	p_session->level = level;
	return 0;
}

int session_logout(HTTP_SESSION *p_session) {
	time_t now;
	now = time(0);
	p_session->isonline = 0;
	p_session->login_time = 0;
	p_session->expire = now + HTTP_SESSION_MAX_HOLD;
	if (p_session->storage) {
		session_free_storage(&(p_session->storage));
	}
	return 0;
}

void session_timeout_check(void) {
	HTTP_SESSION *p_pre = NULL, *p_cur = NULL, *p_next = NULL;
	time_t now;
	now = time(0);
	p_pre = NULL;
	for (p_cur = g_http_sessions; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		if (p_cur->isonline) {
			if (0 && now >= p_cur->heartbeat_expire) {
				printf("user \"%s\" logout because of no heartbeat.\n", p_cur->uname);
				session_logout(p_cur);
			} else if (now >= p_cur->expire) {
				printf("user \"%s\" logout because of timeout.\n", p_cur->uname);
				session_logout(p_cur);
			}
			p_pre = p_cur;
		} else {
			if (now >= p_cur->expire) {
				printf("session \"%s\" timeout.\n", p_cur->session_id);
				if (p_pre == NULL) {
					g_http_sessions = p_next;
				} else {
					p_pre->next = p_next;
				}
				session_free(&p_cur);
			} else {
				p_pre = p_cur;
			}
		}
	}
}

/* update session time of expires */
void update_session_expires(HTTP_FD *p_link) {
	HTTP_SESSION *p_session = NULL;
	time_t now;
	now = time(0);
	p_session = p_link->session;
	p_session->active_time = now;
	if (p_session->isonline) {
		p_session->expire = now + HTTP_SESSION_TIMEOUT;
	} else {
		p_session->expire = now + HTTP_SESSION_MAX_HOLD;
	}
}

/* find the session of link, create if not found */
int set_session(HTTP_FD *p_link) {
	const char *session_id = NULL;
	char str_temp[128], sha1_output[41];
	time_t now;
	HTTP_SESSION *p_cur = NULL;

	now = time(0);
	p_link->session = NULL;
	session_id = web_cookie_str(p_link, "mark", NULL);
	if (session_id != NULL) {
		for (p_cur = g_http_sessions; p_cur != NULL; p_cur = p_cur->next) {
			if (0 == strcmp(p_cur->session_id, session_id) && 0 == strcmp(p_cur->ip, p_link->ip_peer)) {
				p_link->session = p_cur;
				break;
			}
		}
	}
	if (p_link->session == NULL) {
		if (HTTP_SESSION_MAX && g_session_count >= HTTP_SESSION_MAX) {
			web_busy_response(p_link);
			return 1;
		} else {
#if 0
			if (session_id != NULL) { /* use session_id from request */
				p_link->session = append_session(session_id, p_link->ip_peer);
			} else
#endif
			{
				snprintf(str_temp, sizeof(str_temp) - 1, "ip:%s,port:%u,time:%ld,rand:%d", p_link->ip_peer, p_link->port_peer, clock(), rand());
				tt_sha1_hex((unsigned char *)str_temp, strlen(str_temp), sha1_output);
				p_link->session = append_session(sha1_output, p_link->ip_peer);
				if (p_link->session != NULL) {
					snprintf(str_temp, sizeof(str_temp) - 1, "mark=%s; path=/; HttpOnly;", sha1_output);
					web_set_header(p_link, "Set-Cookie", str_temp);
				}
			}
			if (p_link->session == NULL) {
				web_fin(p_link, 500);
				return 1;
			}
		}
	}
	p_link->session->heartbeat_expire = now + 12;
	if (0 == strcmp(p_link->path, "/cgi/heartbeat.json")) {
		web_printf(p_link, "{\"state\":\"%s\"}", p_link->session->isonline ? "online" : "offline");
		web_fin(p_link, 200);
		return 1;
	}
	return 0;
}
