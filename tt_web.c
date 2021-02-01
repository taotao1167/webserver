#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#ifdef _WIN32
	#include <ws2tcpip.h>
#else
	#define __USE_GNU
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
#endif
#include <event2/bufferevent.h>
#ifdef WITH_SSL
#include <event2/bufferevent_ssl.h>
#endif
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#ifdef EVENT__HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <event2/event_struct.h>

#ifndef __TT_PLATFORM_H__
#include "tt_platform.h"
#endif
#ifndef __TT_BUFFER_H__
#include "tt_buffer.h"
#endif
#ifndef __TT_WEB_H__
#include "tt_web.h"
#endif
#ifndef __TT_FILE_H__
#include "tt_file.h"
#endif
#ifndef __TT_SHA1_H__
#include "tt_sha1.h"
#endif
#ifndef __TT_BASE64_H__
#include "tt_base64.h"
#endif
#ifndef __TT_MSGQUEUE_H__
#include "tt_msgqueue.h"
#endif
#ifndef __TT_SESSION_H__
#include "tt_session.h"
#endif

#define emergency_printf(fmt,...) printf("%s %d: ", __FILE__, __LINE__);printf(fmt, ##__VA_ARGS__)
//#define emergency_printf(fmt,...)
#define error_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
//#define error_printf(fmt,...)
#define notice_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
//#define notice_printf(fmt, ...)
#if 0  // print timestamp, for optimize server
	#include <sys/time.h>
	struct timeval g_tv;
	#define debug_printf(fmt, ...) gettimeofday(&g_tv, NULL); printf("[%ld.%06ld] ", g_tv.tv_sec, g_tv.tv_usec); printf(fmt, ##__VA_ARGS__)
#else
	#define debug_printf(fmt, ...)
	// #define debug_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
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

/* map status code and status string */
typedef struct ST_HTTP_CODE_MAP {
	const int code;
	const char *status_text;
	const char *entity;
}HTTP_CODE_MAP;

/* map mime type */
typedef struct ST_HTTP_MIME_MAP {
	const char *suffix;
	const char *mime;
}HTTP_MIME_MAP;

typedef enum E_HTTP_JUDGE {
	JUDGE_COMPLETE = 1,
	JUDGE_CONTINUE,
	JUDGE_ERROR
}E_HTTP_JUDGE;

typedef struct ST_HTTP_INNER_MSG {
	char *name; /* message name */
	int is_sync; /* 0 means async, not 0 means sync */
	int (*callback)(int ret, void *arg); /* callback function, NULL means block */
	void *arg; /* args of callback */
	MSG_Q *resp_msgq; /* message queue for read return value of callback */
	int ref_cnt; /* reference count of message queue */
	void *payload; /* payload */
	size_t payload_len; /* payload length */
	struct ST_HTTP_INNER_MSG *next;
}HTTP_INNER_MSG;

typedef struct ST_HTTP_CALLBACK_RESPONSE {
	int ret; /* return value of callback */
}HTTP_CALLBACK_RESPONSE;

#ifdef WITH_WEBSOCKET
extern int ws_dispatch(HTTP_FD *p_link, E_WS_EVENT evt);
typedef enum E_WS_DECODE_RET {
	WS_DECODE_CLOSED = 1,
	WS_DECODE_PING,
	WS_DECODE_PONG,
	WS_DECODE_NEEDMORE,
	WS_DECODE_CHUNKED,
	WS_DECODE_COMPLETE,
	WS_DECODE_AGAIN,
	WS_DECODE_ERROR,
}E_WS_DECODE_RET;
#endif

extern int req_dispatch(HTTP_FD *p_link);
extern int msg_dispatch(const char *name, void *buf, size_t len);
extern void tt_handler_register();

#ifdef WITH_SSL
SSL_CTX *g_default_ssl_ctx = NULL;
#endif

struct event_base *g_event_base = NULL;
WEB_SERVER *g_servers = NULL;
HTTP_FD *g_http_links = NULL;
evutil_socket_t g_msg_fd[2] = {0};

static const char *g_hostname = "Border Collie"; /* Server Name, will show at response */
static size_t g_init_recv_space = 1024; /* init length of HTTP_FD->recvbuf */
static size_t g_max_head_len = 8192; /* max value allowed of the length of http header, default 8K(8192), 0 means no limit */
static size_t g_max_entity_len = 104857600; /* max value allowed of the length of http entity, default 100M(104857600), 0 means no limit */
static time_t g_max_active_interval = 0; /* max interval allowed of the duration of recv, uint: second, default: 5, 0 means no limit */
static time_t g_recv_timeout = 0; /* max duration allowed per request, unit: second, default: 60, 0 means no limit */
static MSG_Q g_web_inner_msg; /* the msg queue of web server */
static HTTP_INNER_MSG *g_web_inner_msg_head = NULL; /* the msgs that wait free, will be freed if ref == 0 */

static const char *g_err_500_head = \
	"HTTP/1.1 500 Internal Server Error\r\n"\
	"Pragma: no-cache\r\n"\
	"Cache-Control: no-store\r\n"\
	"Content-Type: text/html\r\n"\
	"Content-Length: 47\r\n"\
	"Connection: close\r\n"\
	"\r\n";

static const char *g_err_500_entity = \
	"<html><h1>500 Internal Server Error</h1></html>";

#ifdef WITH_SSL
const char *g_default_ca_cert = \
		"-----BEGIN CERTIFICATE-----\r\n"\
		"MIIFpTCCA42gAwIBAgIJANte1W91z60qMA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\r\n"\
		"BAYTAlhYMQ0wCwYDVQQIDARYWFhYMQ0wCwYDVQQHDARYWFhYMR4wHAYDVQQKDBVY\r\n"\
		"WFhYIFRlY2hub2xvZ3kuIEx0ZC4xGzAZBgNVBAMMElhYWFggVGVjaG5vbG9neSBD\r\n"\
		"QTAgFw0yMDA5MDExMDA4MjJaGA8yMTIwMDgwODEwMDgyMlowaDELMAkGA1UEBhMC\r\n"\
		"WFgxDTALBgNVBAgMBFhYWFgxDTALBgNVBAcMBFhYWFgxHjAcBgNVBAoMFVhYWFgg\r\n"\
		"VGVjaG5vbG9neS4gTHRkLjEbMBkGA1UEAwwSWFhYWCBUZWNobm9sb2d5IENBMIIC\r\n"\
		"IjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA3uJgaMYf36iBJj0wd1+3n2L/\r\n"\
		"I2QsJ2xxriyTPlxF/sqaf6kylvyj6x++ZyeUI/VgaHtLIBkrVNOwdollsoQbEp0h\r\n"\
		"c8NTBB/fMVZ/kZ3CSZnqCxzCwcUebq8AadKOUSU08LLhJwzERfDQHxhjpnqmRG0u\r\n"\
		"lBhA/SRgCfZ1bSqwjJNasGCMf31Ry0hM5ENNbBSmRfUW9KOFTyKd5qcoxSVt9bSe\r\n"\
		"tDGPzLk0pRF1EuvRENAJBtVoucTflHrDKdLHVIFaDOfY+/9Y3G51R8/60+ZIYoXi\r\n"\
		"PqBZUVkovh28ruPdQYxKT6CoWbLwrHMYxylwTdbBh8NTw0yHLgANiHrCgg6OLrbd\r\n"\
		"t3SPzoaSdzncYS1F1Sxa2Cwxyx6eF5XnsNx0oZMns0n2hkMvJu6Uacw2c1TIUlGa\r\n"\
		"AUmCZt+wjnBVlx9T5YJbY6i9sr20mnfuZ5hQsf9P6B7/nU0NCvmOjxc6v2easLRZ\r\n"\
		"EdQUKiBmRH/U9gXEJJKv+JE1QTXRppczo1ILhhEaOUBrrB5k8+FESphVhXk/kXiN\r\n"\
		"N5sq6LqyWKGDTbj8pWkV+xct91uNZ3Mj0UVsc+m62Tbmv/kpx68+rI4Pk5TU0hkK\r\n"\
		"fdUtSzx1mk70UKXO/7rjTPL9hqbrCufEICZjj1gUIcST1rgy+nhkX0btSFJYv/yF\r\n"\
		"zdmhM/mDO9i/otT2S4MCAwEAAaNQME4wHQYDVR0OBBYEFFlwpKmpIJFNDLUwQ8xG\r\n"\
		"nIQzZgnAMB8GA1UdIwQYMBaAFFlwpKmpIJFNDLUwQ8xGnIQzZgnAMAwGA1UdEwQF\r\n"\
		"MAMBAf8wDQYJKoZIhvcNAQELBQADggIBAI2W57gJwYfdBy3MILx6IfxkyJgr0d6Z\r\n"\
		"oze8gggtcryK3DVDLIA9FL44yCvRL9YQaHK17f/DC/bhxT6acovkScSapKck80eV\r\n"\
		"ZPOsngtABUp6SCpo2AJpD3+afJLfAgmnv7x/dd79suPw0Qqq5bn3/qmIO8XUB36L\r\n"\
		"989F/fKmGrPuaXXp/2OiDglTwpsYnDGIvivb4bd4tGmmoP75GJgh1lapkdNgXTcS\r\n"\
		"EQz8JLiUyDpOHsQnZ0lZZTtsUWYMdAH4TD2pqbJ2K/yuk4ifrSuqKOUz3Eds4/2E\r\n"\
		"MqfM9U/s2mDrWGyW2QP/ox3ZL+QxkVJb9wjGT8bTzdv7JwTNdl9BLEzBckSxx/9B\r\n"\
		"43Xo6qx3U60j84R7pr/azADGQJkjTGjy0hdTYVYJJLltIjUgk3Pcz2MnrULc8VNN\r\n"\
		"EbZUGDxstlj1usm/QQoY7u79HRhtl558sa8ZJi4WRH0FJRx1Dy2hpclXDRvCuyB6\r\n"\
		"dqH1RD6IBN1M/BuT3Zvq5iqlGQg/Pnuhs+atQ6HrJJhDLqN2Jm3o/TVJYzUmBJmV\r\n"\
		"7n90ObOGBHFYu7YjwhvNlUqbZK5thU1iThclxRERUWT2WIOrGgyl9qjkQObPWl0U\r\n"\
		"kwROx3ui07BNf5z+Tru7LPT8407ryUihhuoN0xZQboLXxUcrRYJlWJzTGdNX7VDW\r\n"\
		"YaP1oMTn4zcu\r\n"\
		"-----END CERTIFICATE-----\r\n";
const char *g_default_svr_cert = \
		"-----BEGIN CERTIFICATE-----\r\n"\
		"MIIFhjCCA26gAwIBAgIJAKdR9zy4dcd6MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\r\n"\
		"BAYTAlhYMQ0wCwYDVQQIDARYWFhYMQ0wCwYDVQQHDARYWFhYMR4wHAYDVQQKDBVY\r\n"\
		"WFhYIFRlY2hub2xvZ3kuIEx0ZC4xGzAZBgNVBAMMElhYWFggVGVjaG5vbG9neSBD\r\n"\
		"QTAeFw0yMDA5MDExMDA4MzlaFw0zMDA4MzAxMDA4MzlaMGQxCzAJBgNVBAYTAlhY\r\n"\
		"MQ0wCwYDVQQIDARYWFhYMQ0wCwYDVQQHDARYWFhYMR4wHAYDVQQKDBVYWFhYIFRl\r\n"\
		"Y2hub2xvZ3kuIEx0ZC4xFzAVBgNVBAMMDm15Y29tcGFueS5zb2Z0MIICIjANBgkq\r\n"\
		"hkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA9Y/HgQl7v0jsrXk+MORg/FG7FV1KFbAy\r\n"\
		"JFlOlTQJxuaRbQyBo0xeKPFAweXOFxogmAK8XHC5rxWIfiQG7OfL3wkYq88A9tCH\r\n"\
		"STUGCBHQ94yYAgOOsjITtvmm5oGT9M+VHRth3rzHDtQAtnAHjhqAVVxaPT4dc+ja\r\n"\
		"8dRNSHAe+i9iwTGNsCGa13Hdc5fe0KKbXVAQyphY990U97oeNoZZc7d3WnMsErPx\r\n"\
		"t62IIKU8Gw3VuG8YLeMxbyg9P82j64k4WjQafyVtl8AK+Y4GSL9Sd8/Sjcl83g61\r\n"\
		"5v5CppBxHnkLa+0kCCTEebTq45jzr+U/wf50swqiaUo3Y1XGL4h9lvuGwAo7b4fn\r\n"\
		"ryH9Feuv4eBGhNJEN6l7Uby3BZLPN7JHmPzFmZhtSGXTMuSDbzUJOhTtloYcXFBk\r\n"\
		"S3g+RUqxotSdrHxa564bDcieQjK/C+JY/vjfiHy+sn0+if0pJ74Ccp6Al9ZAygqa\r\n"\
		"Kmgkd6a6J9arCC0nrrWPVwMiqxfZkqBoFitzZyYpze87vavWhlF+scGsVkxLLh9v\r\n"\
		"mS15nWc3LzadKfCdgkHO8ouw3velCT/BZzcanwMDRPlMWRgeFVmf/IxqSwdFgU6i\r\n"\
		"6By1TW3TqNLotwAV2NWeUgv7EBTimJ7HEYDrTe11jb6FEM7EqI8KFXcH0EYBYtQL\r\n"\
		"fvvg724KVRcCAwEAAaM3MDUwMwYDVR0RBCwwKoIObXljb21wYW55LnNvZnSCEnd3\r\n"\
		"dy5teWNvbXBhbnkuc29mdIcEwKg4ZTANBgkqhkiG9w0BAQsFAAOCAgEApAK7U6fN\r\n"\
		"BgYigqPDDb2nmhBnjNAiorVCMXdnYQ7krxLy9E0dYE5BiuoWDIxoSRgmp+S5Enza\r\n"\
		"ThL6hPAgugu9ZKZ5vVrF/ksEG1fXqtY4Eyn2EqJMvdihTSLgAgWx0LjiyKucUCCc\r\n"\
		"OdKj80QfbcZDdWllmcXaawdyk2bMkhd5Ujy7Exd2I+x6JkgWmpwggC0sx/VwvK9E\r\n"\
		"uPEVBoQgJi2vndikuA6I5Yto0PIfwx6sBDwtfX4bJPVJIWac30Tj3UTrDbJX/YAP\r\n"\
		"a4ggr/IiEhKy/PXoBktoaMu4ggjmIoWiE+yHidhdMGCbfuPgY/7qdzbkD4cVYUnM\r\n"\
		"b/iBoM0UUojRJGD0gpx8oicctcISm41sSHPLf8HIGHLvZWZRLwJV5Bce19ANMjVI\r\n"\
		"pS5ZvDb/O2DR0vD0YGjho4ZSKaxlyNYKKPkZhPb66+6SlwQqlTHbHbR4scxxwSEF\r\n"\
		"HfhreSfYlkfB7MruPBXMczez6sEoz62k60kwjiK3tK25YLg2liaFhxrM+8rnGKh8\r\n"\
		"1itkQ7Nm4eUBMLJ3Y0yk86jFJuNHaa9yci2UFsADrm9l3Ypd7bvGsdRFJIGyK9xr\r\n"\
		"4uM3mT9xXRqWH5ZlkpRgKPDRF3ZTzfVwRTp+zLvHai4X+5ujHQBNkgRHORWVuDPl\r\n"\
		"olcCJsG2mNv0PHagm/55B/WqTFMpFLMJ3dM=\r\n"\
		"-----END CERTIFICATE-----\r\n";
const char *g_default_privkey = \
		"-----BEGIN RSA PRIVATE KEY-----\r\n"\
		"MIIJKQIBAAKCAgEA9Y/HgQl7v0jsrXk+MORg/FG7FV1KFbAyJFlOlTQJxuaRbQyB\r\n"\
		"o0xeKPFAweXOFxogmAK8XHC5rxWIfiQG7OfL3wkYq88A9tCHSTUGCBHQ94yYAgOO\r\n"\
		"sjITtvmm5oGT9M+VHRth3rzHDtQAtnAHjhqAVVxaPT4dc+ja8dRNSHAe+i9iwTGN\r\n"\
		"sCGa13Hdc5fe0KKbXVAQyphY990U97oeNoZZc7d3WnMsErPxt62IIKU8Gw3VuG8Y\r\n"\
		"LeMxbyg9P82j64k4WjQafyVtl8AK+Y4GSL9Sd8/Sjcl83g615v5CppBxHnkLa+0k\r\n"\
		"CCTEebTq45jzr+U/wf50swqiaUo3Y1XGL4h9lvuGwAo7b4fnryH9Feuv4eBGhNJE\r\n"\
		"N6l7Uby3BZLPN7JHmPzFmZhtSGXTMuSDbzUJOhTtloYcXFBkS3g+RUqxotSdrHxa\r\n"\
		"564bDcieQjK/C+JY/vjfiHy+sn0+if0pJ74Ccp6Al9ZAygqaKmgkd6a6J9arCC0n\r\n"\
		"rrWPVwMiqxfZkqBoFitzZyYpze87vavWhlF+scGsVkxLLh9vmS15nWc3LzadKfCd\r\n"\
		"gkHO8ouw3velCT/BZzcanwMDRPlMWRgeFVmf/IxqSwdFgU6i6By1TW3TqNLotwAV\r\n"\
		"2NWeUgv7EBTimJ7HEYDrTe11jb6FEM7EqI8KFXcH0EYBYtQLfvvg724KVRcCAwEA\r\n"\
		"AQKCAgAxsgJPA+tOP+TeCo5ieKWECspQvYtzri/HmWdE3L4pyZmetww5TwgzQve3\r\n"\
		"BdnaZQg5mzwhfRKm8zWnSYU5RoPExGOAyajJSlRGpXtuAKUIpCRTLAMh3u6MdiOn\r\n"\
		"vIuD3MqxlEwPNqcWQiNOyIdO4+Aa5lDA2iQ9w98jV7K4OTCbsyFKNcwV3AcpGLsX\r\n"\
		"XKHUGURV8xEi3wgKSWyn4buhjzjlKTj3Th2aP15orK7C5I2wb/FTnPAIwVTMyRj0\r\n"\
		"I5rL3P5Riv/dwWZXcp8F+A8fg9CdvKpA1Yj2q4V2lPhKGys5XofINLq5By/X7Zwn\r\n"\
		"qVmPvFZV3Tgiz8Eki8d/y+kKz5WSpiANu39ovcYF9UqlpKn0h9WBZK9j11Z8n+m/\r\n"\
		"I00vi98++6pZox81v37GLgbVDH/yBf4Bl4oFNgY9UC/T/Se7lLMOmOlZhpqYs6lI\r\n"\
		"rc2xyYHXdvu4zuRk+o9on9lCn9Zl7PUc18dOKc+iisZuXff4kPduhYVZVtqUjtuI\r\n"\
		"Jie28ca6rVFl2kkvI2sjQQTiOq9rOFDAbEnf7liFpsY72FSjeoh+w1TlPKvsLz2T\r\n"\
		"4lfH7Jig7NQsL4Ik61eIbzEG0+MruFDM979Q8PGvtT1zAaHHrERtKmQCUlpY5sKO\r\n"\
		"wUXW8pT9WPPe+OdAP4rR43ZDtl8TJhAm32V6rRcGKMeCF9Dm6QKCAQEA/oiJbTIl\r\n"\
		"/ir54tWtoE1El5W04k7IalL1OuQW/7hobrFIAzF1LCFKJUXDi25An18T/ILSMWEf\r\n"\
		"gkN8eZEjS9lQnrvUNN+ADr7QZPIBnIWMtsX+OmYtJdm/KQhuwVOlsU23/W9ZaqY3\r\n"\
		"4qtF26Ete3ZiKsgLPABrZKfRiVu0P2Is98HuNOUP77q4VW4ZSqIG/xZ4kKOeRAwL\r\n"\
		"u9wuDMqn5WZ69u8yEh443jawXWiyrGfI/NdT2nIQEJn8K+NzlBzDj6IixXe8K7et\r\n"\
		"t94DbPxolVRejI0AyW+tqOBYeccrwn4qF0FP2sVG3xlct0DiDFdirAymqRFyqwig\r\n"\
		"dRFHeX6+fuxr7QKCAQEA9voCHwMRRQvolV0MZHw+UrUmmUw5hAUsxBlm4cHOrxBO\r\n"\
		"fJfNxJ/6Vu647it0bnZ3vkbpg9rIESKnsCdeKNnjwdPOg1t4m+cmKg6leckrB5pR\r\n"\
		"nz13n8E5r1jYmVCgCvL2cEp6QhA8ZVEc5vshyCmlfxtDQmqhBb6h10YCOGsZlLKA\r\n"\
		"IgWzJz4gdMH9eU91CtfASkMXteY51h+Wm+OfKL9RHH4r1T0qvwFNQcMVp4cYON3i\r\n"\
		"2P+0sCk7kj2vIOGlE/m6SVcsRc4WmY5GoBDARGYEpvsxv6Zn5Cx0HcPpf30691Hf\r\n"\
		"4HSCnIOBowLqNpn978CpIcjLmIhr2WokrK4lcXZMkwKCAQEA58YyxcE9gWVm6FxE\r\n"\
		"30yc4h+Ld+vfaZoi/j5GhxNH3UwHVAkMNmQV3TzapDOAGt98FpqVhit3gqmrRhOY\r\n"\
		"fW/kjrZgtMi0FjlBEDqipwGhMam3Jzx1bhsKi/XGzLo7jelEWj2zwX6cMwZc9XKe\r\n"\
		"NOeG2ogQWtHS+PQrxJVB29mtMupf28Cds2FmizxXQXOrRUsT/DlRW4tQf/8t264K\r\n"\
		"FsUrwYHsKT1l8UsBa/1zfbR/+2SkUa093bAqRVi3nS5YEYnFeeTe7zwxrtU2b16k\r\n"\
		"aR5LflbDWaodC0NGrZVkzpUOBOEPMxoiv/KbT6+AZ1aHXj2i4R8nOIEBg7h5SjPU\r\n"\
		"iUUFvQKCAQBxAJ8r1Vl3gCVhAfwDlh1l9mC8z3WZhEyhRQYf0W9iBlIDC+ooRxJh\r\n"\
		"6ERRJSzWxfmv4/5ASEA2JucW4Exi4J8nNmRNEfMzUiXSJHVuLa80gVVLSPKM8/oS\r\n"\
		"0tH4BD3WBKjhZBJgW9FmgiB5RvqD/Rot+ClkICmg2UJvnyaw7b/8m1epOXZpRDHR\r\n"\
		"elkhwktJNMJUuDIHFlnsMPPWS+Ws3SydntUrchs23b+dkUc9KcDZdByxMP5Zg+Qv\r\n"\
		"221f3B6N7ztfBzXIkq0ggUqrPj7ajNsgE9sFV3w9mGFgpR0XKhNqydlnd6ezOTB9\r\n"\
		"h33mkbrwkfRQ8vDEz9aY2coCAIQ9T+VlAoIBAQDkza4dsul8VgBNKqEVq1NCqZfa\r\n"\
		"n2XhCA2CMBdP7MhnSXN4BIl1SgQont8xUIqSGDDu3QmmtAwI5A4DTiQfeS6J5li+\r\n"\
		"+5uQdyHHBS6H4EMvpwXyJGGE7rzyBMWBuN8piFEKqdk5p7cGKe7iGYwTG42TScqD\r\n"\
		"+X7oB+mJlaQ2/VyYDrVpG9mu+5VsO2jVd0DPpRf6q+z0WuRuLAy2yc2q3EfSNi6y\r\n"\
		"ZV5BMQXkqoriS+EVorqgZV/7oguiizFj5zTfYExD9yigmhT78p6kJPi2ZNxlQLmh\r\n"\
		"Acnc5GeVo286syYQ5niFb8F2bPuFgqcIJZaKggoNDSXSir4sl+XgX4uuzXKL\r\n"\
		"-----END RSA PRIVATE KEY-----\r\n";
#endif /* endof WITH_SSL */

void hexdump(void *_buf, size_t size) {
	unsigned char *buf = (unsigned char *)_buf;
	size_t i = 0, offset = 0;
	char tmp = '\0';
	for (offset = 0; offset < size; offset += 16) {
		printf("%08x: ", (unsigned int)offset);
		for (i = 0; i < 16; i++) {
			if (offset + i < size) {
				printf("%02x ", *(buf + offset + i));
			} else {
				printf("   ");
			}
		}
		printf("  ");
		for (i = 0; i < 16; i++) {
			if (offset + i < size) {
				tmp = *(buf + offset + i);
				if (tmp < ' ' || tmp > '~') {
					printf(".");
				} else {
					printf("%c", tmp);
				}
			}
		}
		printf("\n");
	}
}
/*
../aaa => /aaa
/../aaa => /aaa
/aaa/../bbb => /bbb
aaa/./bbb => /aaa/bbb
aaa/../bbb => /bbb
aaa/bbb/ccc => aaa/bbb/ccc
*/
static int path_merge(char *dst, const char *src) {
	const char *p_start = NULL, *p_end = NULL;
	char *p_dstchar = NULL;

	for (p_dstchar = dst, p_end = p_start = src; ; ) {
		if (*p_end == '/' || *p_end == '\0') { /* TODO for windows '\\' */
			if (p_end == p_start + 2 && *p_start == '.' && *(p_start + 1) == '.') { /* ../ */
				if (p_dstchar > dst && *(p_dstchar - 1) == '/') {
					p_dstchar -= 1;
				}
				while (1) {
					if (p_dstchar == dst) {
						*p_dstchar = '/';
						p_dstchar++;
						break;
					} else if (*(p_dstchar - 1) == '/') {
						break;
					} else {
						p_dstchar--;
					}
				}
			} else if (p_end == p_start + 1 && *p_start == '.') { /* ./ */
				if (*dst != '/') {
					memmove(dst + 1, dst, p_dstchar - dst + 1);
					p_dstchar++;
					*dst = '/';
				}
			} else {
				memmove(p_dstchar, p_start, p_end - p_start + 1);
				p_dstchar += p_end - p_start + 1;
			}
			if (*p_end == '\0') {
				break;
			}
			p_end++;
			p_start = p_end;
			continue;
		}
		p_end++;
	}
	return 0;
}
/* urldecode, src and dst allow same value */
static int urldecode(char *dst, const char *src) {
	const char *p_srcchar = NULL;
	char *p_dstchar = NULL;
	unsigned char tmp_char = '\0';
	for (p_dstchar = dst, p_srcchar = src; *p_srcchar != '\0'; ) {
		if (*p_srcchar == '+') {
			*p_dstchar = ' ';
			p_srcchar++;
			p_dstchar++;
		} else if (*p_srcchar == '%' && isxdigit(*(p_srcchar + 1)) && isxdigit(*(p_srcchar + 2))) {
			if (*(p_srcchar + 1) >= '0' && *(p_srcchar + 1) <= '9') {
				tmp_char = *(p_srcchar + 1) - '0';
			} else if (*(p_srcchar + 1) >= 'A' && *(p_srcchar + 1) <= 'Z') {
				tmp_char = *(p_srcchar + 1) - 'A' + 10;
			} else {
				tmp_char = *(p_srcchar + 1) - 'a' + 10;
			}
			tmp_char <<= 4;
			if (*(p_srcchar + 2) >= '0' && *(p_srcchar + 2) <= '9') {
				tmp_char |= *(p_srcchar + 2) - '0';
			} else if (*(p_srcchar + 2) >= 'A' && *(p_srcchar + 2) <= 'Z') {
				tmp_char |= *(p_srcchar + 2) - 'A' + 10;
			} else {
				tmp_char |= *(p_srcchar + 2) - 'a' + 10;
			}
			*p_dstchar = tmp_char;
			p_srcchar += 3;
			p_dstchar++;
		} else {
			*p_dstchar = *p_srcchar;
			p_srcchar++;
			p_dstchar++;
		}
	}
	*p_dstchar = '\0';
	return 0;
}

/* some danger character need escaped, do not need free dst to avoid malloc again */
const char *htmlencode(const char * src) {
	static char *dst = NULL;
	static unsigned int dst_space = 0;
	unsigned int i = 0, j = 0, len = 0;
	len = strlen(src);
	if (len * 6 + 1 > dst_space) {/* most terrible situation is 6 multiple length */
		if (dst_space == 0) {
			dst_space = 128; /* at least 128 bytes */
		}
		while (len * 6 + 1 > dst_space) {
			dst_space <<= 1;
		}
		if (dst == NULL) {
			dst = (char *)MY_MALLOC(dst_space);
		} else {
			dst = (char *)MY_REALLOC(dst, dst_space);
		}
		if (dst == NULL) {
			emergency_printf("malloc/realloc failed!\n");
			return "";
		}
	}
	memset(dst, 0, dst_space);
	for(i = 0; i < len; i++) {
		if ('>' == src[i]) {
			j += sprintf(dst + j, "&gt;");
		} else if ('<' == src[i]) {
			j += sprintf(dst + j, "&lt;");
		} else if ('&' == src[i]) {
			j += sprintf(dst + j, "&amp;");
		} else if ('\"' == src[i]) {
			j += sprintf(dst + j, "&quot;");
		} else if ('\'' == src[i]) {
			j += sprintf(dst + j, "&#39;");
		} else {
			j += sprintf(dst + j, "%c", src[i]);
		}
	}
	return dst;
}
/* free kypair, but not free the space of key and value */
static int free_kvpair_shallow(HTTP_KVPAIR **p_head) {
	HTTP_KVPAIR *p_cur = NULL, *p_next = NULL;
	for (p_cur = *p_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		MY_FREE(p_cur);
	}
	*p_head = NULL;
	return 0;
}

/* free kypair and the space of key and value */
static int free_kvpair_deep(HTTP_KVPAIR **p_head) {
	HTTP_KVPAIR *p_cur = NULL, *p_next = NULL;
	for (p_cur = *p_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		if (p_cur->key != NULL) {
			MY_FREE(p_cur->key);
		}
		if (p_cur->value != NULL) {
			MY_FREE(p_cur->value);
		}
		MY_FREE(p_cur);
	}
	*p_head = NULL;
	return 0;
}

static int free_ranges(HTTP_RANGE **p_head) {
	HTTP_RANGE *p_cur = NULL, *p_next = NULL;
	for (p_cur = *p_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		MY_FREE(p_cur);
	}
	*p_head = NULL;
	return 0;
}

static int free_files(HTTP_FILES **p_head) {
	HTTP_FILES *p_cur = NULL, *p_next = NULL;
	for (p_cur = *p_head; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		MY_FREE(p_cur);
	}
	*p_head = NULL;
	return 0;
}

/* get information of header, return default_value if not found in p_link->header_data */
const char *web_header_str(HTTP_FD *p_link, const char *key, const char *default_value) {
	HTTP_KVPAIR *p_cur = NULL;
	const char *p_keypos = NULL, *p_inkeypos = NULL;
	int is_same = 1;
	for (p_cur = p_link->header_data; p_cur != NULL; p_cur = p_cur->next) {
		is_same = 1;
		for (p_keypos = p_cur->key, p_inkeypos = key; ;p_keypos++, p_inkeypos++) {
			if (*p_keypos == *p_inkeypos) {
			} else if (isalpha(*p_keypos) && ((*p_keypos) & 0xdf) == ((*p_inkeypos) & 0xdf)) {
				/* the diffrence between upper character and the lower is 0x20, so it will equal if not compare the diffrent bit */
			} else if (*p_keypos == '-' && *p_inkeypos == '_') {
			} else if (*p_keypos == '_' && *p_inkeypos == '-') {
			} else {
				is_same = 0;
				break;
			}
			if (*p_keypos == '\0') {
				break;
			}
		}
		if (is_same) {
			return p_cur->value;
		}
	}
	return default_value;
}

/* get information of headers defined by user, return default_value if not found in p_link->cnf_header */
static const char *web_cnf_header_str(HTTP_FD *p_link, const char *key, const char *default_value) {
	HTTP_KVPAIR *p_cur = NULL;
	const char *p_keypos = NULL, *p_inkeypos = NULL;
	int is_same = 1;
	for (p_cur = p_link->cnf_header; p_cur != NULL; p_cur = p_cur->next) {
		is_same = 1;
		for (p_keypos = p_cur->key, p_inkeypos = key; ;p_keypos++, p_inkeypos++) {
			if (*p_keypos == *p_inkeypos) {
			} else if (isalpha(*p_keypos) && ((*p_keypos) & 0xdf) == ((*p_inkeypos) & 0xdf)) {
				/* the diffrence between upper character and the lower is 0x20, so it will equal if not compare the diffrent bit */
			} else if (*p_keypos == '-' && *p_inkeypos == '_') {
			} else if (*p_keypos == '_' && *p_inkeypos == '-') {
			} else {
				is_same = 0;
				break;
			}
			if (*p_keypos == '\0') {
				break;
			}
		}
		if (is_same) {
			return p_cur->value;
		}
	}
	return default_value;
}

/* get information of query, return default_value if not found in p_link->query_data */
const char *web_query_str(HTTP_FD *p_link, const char *key, const char *default_value) {
	HTTP_KVPAIR *p_cur = NULL;
	for (p_cur = p_link->query_data; p_cur != NULL; p_cur = p_cur->next) {
		if (0 == strcmp(p_cur->key, key)) {
			return p_cur->value;
		}
	}
	return default_value;
}

/* get information of cookie, return default_value if not found in p_link->cookie_data */
const char *web_cookie_str(HTTP_FD *p_link, const char *key, const char *default_value) {
	HTTP_KVPAIR *p_cur = NULL;
	for (p_cur = p_link->cookie_data; p_cur != NULL; p_cur = p_cur->next) {
		if (0 == strcmp(p_cur->key, key)) {
			return p_cur->value;
		}
	}
	return default_value;
}

/* get information of post, return default_value if not found in p_link->post_data */
const char *web_post_str(HTTP_FD *p_link, const char *key, const char *default_value) {
	HTTP_KVPAIR *p_cur = NULL;
	for (p_cur = p_link->post_data; p_cur != NULL; p_cur = p_cur->next) {
		if (0 == strcmp(p_cur->key, key)) {
			return p_cur->value;
		}
	}
	return default_value;
}

/* get information of files uploaded by peer, return NULL if not found in p_link->file_data, do not free even return not NULL */
const HTTP_FILES *web_file_data(HTTP_FD *p_link, const char *key) {
	HTTP_FILES *p_cur = NULL;
	for (p_cur = p_link->file_data; p_cur != NULL; p_cur = p_cur->next) {
		if (0 == strcmp(p_cur->key, key)) {
			return p_cur;
		}
	}
	return NULL;
}

/* free information alloced by call web_file_list */
int web_file_list_free(HTTP_FILES **p_head) {
	return free_files(p_head);
}

/* get information of multi files uploaded by peer, return NULL if not found in p_link->file_data, need caller free by call web_file_list_free */
HTTP_FILES *web_file_list(HTTP_FD *p_link, const char *key) {
	HTTP_FILES *p_cur = NULL;
	HTTP_FILES *p_head = NULL, *p_new = NULL, *p_tail = NULL;
	for (p_cur = p_link->file_data; p_cur != NULL; p_cur = p_cur->next) {
		if (0 != strcmp(p_cur->key, key)) {
			continue;
		}
		p_new = (HTTP_FILES *)MY_MALLOC(sizeof(HTTP_FILES));
		if (p_new == NULL) {
			emergency_printf("malloc failed!\n");
			free_files(&p_head);
			return NULL;
		}
		if (p_head == NULL) {
			p_head = p_new;
		} else {
			p_tail->next = p_new;
		}
		p_tail = p_new;
		memset(p_new, 0x00, sizeof(HTTP_FILES));
		p_new->key = p_cur->key;
		p_new->fname = p_cur->fname;
		p_new->ftype = p_cur->ftype;
		p_new->fcontent = p_cur->fcontent;
		p_new->fsize = p_cur->fsize;
	}
	return p_head;
}

/* parse the information of files uploaded by peer */
static int parse_files(HTTP_FD *p_link, unsigned char *p_entity, unsigned int u_entitylen, const char *p_boundary, E_HTTP_JUDGE *judge_result, int *http_code) {
	char *p_boundary_start = NULL, *p_boundary_split = NULL, *p_boundary_end = NULL;
	char *p_key = NULL, *p_value = NULL, *p_fkey = NULL, *p_fname = NULL, *p_ftype = NULL;
	unsigned char *p_fcontent = NULL;
	unsigned char *p_start = NULL, *p_end = NULL, *p_entityend = NULL;
	int fsize = 0, boundary_split_len = 0, boundary_end_len = 0;
	char *p_start_dis = NULL, *p_end_dis = NULL, *p_key_dis = NULL, *p_value_dis = NULL;
	int ret = 0, isend = 0, i = 0, boundary_match = 0;
	HTTP_KVPAIR *p_newdata = NULL, *p_taildata = NULL;
	HTTP_FILES *p_newfile = NULL, *p_tailfile = NULL;

	/* save boundary for later use */
	p_boundary_start = (char *)MY_MALLOC(strlen(p_boundary) + 5);
	if (p_boundary_start == NULL) {
		emergency_printf("malloc failed!\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 500;
		ret = -1;
		goto exit_fn;
	}
	sprintf(p_boundary_start, "--%s\r\n", p_boundary);
	p_boundary_split = (char *)MY_MALLOC(strlen(p_boundary) + 7);
	if (p_boundary_split == NULL) {
		emergency_printf("malloc failed!\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 500;
		ret = -1;
		goto exit_fn;
	}
	sprintf(p_boundary_split, "\r\n--%s\r\n", p_boundary);
	boundary_split_len = strlen(p_boundary_split);
	p_boundary_end = (char *)MY_MALLOC(strlen(p_boundary) + 9);
	if (p_boundary_end == NULL) {
		emergency_printf("malloc failed!\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 500;
		ret = -1;
		goto exit_fn;
	}
	sprintf(p_boundary_end, "\r\n--%s--\r\n", p_boundary);
	boundary_end_len = strlen(p_boundary_end);
	if (0 != memcmp(p_entity, p_boundary_start, strlen(p_boundary_start))) {
		notice_printf("'boundary_start' not found at begin of entity.\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 400;
		ret = -1;
		goto exit_fn;
	}
	p_start = p_entity + strlen(p_boundary_start);
	while (1) { /* get part splited by boundary by this loop */
		while (1) {/* get file name and file type from part by this loop */
			for (p_end = p_start; *p_end != '\0' && *p_end != ':'; p_end++);/* p_end point to ':' */
			if (*p_end != ':') {
				notice_printf("':' not found at end of header define key.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 400;
				ret = -1;
				goto exit_fn;
			}
			*p_end = '\0';
			p_key = (char *)p_start;

			for(p_start = p_end + 1; *p_start == ' ' || *p_start == '\t'; p_start++);/* skip SPACE and TAB after ':', p_start point to the start of value */

			for (p_end = p_start; *p_end != '\0' && *p_end != '\r' && *p_end != '\n'; p_end++);/* value exclue new line, p_end point to line end */
			if (*p_end != '\r' || *(p_end + 1) != '\n') {
				notice_printf("CRLF not found at end of multi header define value.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 400;
				ret = -1;
				goto exit_fn;
			}
			*p_end = '\0';
			p_value = (char *)p_start;

			debug_printf("multipart: '%s' => '%s'\n", p_key, p_value);
			if (0 == strcasecmp(p_key, "Content-Disposition")) {
				p_start_dis = p_value;
				for (p_end_dis = p_start_dis; *p_end_dis != '\0' && *p_end_dis != ';'; p_end_dis++);/* Disposition-type end with ':' */
				if (*p_end_dis != ';') {
					notice_printf("';' not found at end of Disposition-type.\n");
					*judge_result = JUDGE_ERROR;
					*http_code = 400;
					ret = -1;
					goto exit_fn;
				}
				/* skip SPACE and TAB after ':' */
				for(p_start_dis = p_end_dis + 1; *p_start_dis == ' ' || *p_start_dis == '\t'; p_start_dis++);
				while (1) {/* get key and filename by parse Content-Disposition by this loop */
					for (p_end_dis = p_start_dis; *p_end_dis != '\0' && *p_end_dis != '='; p_end_dis++);/* p_end_dis point to '=' */
					if (*p_end_dis != '=' || *(p_end_dis + 1) != '\"') {
						notice_printf("'=\"' not found at end of disposition key.\n");
						*judge_result = JUDGE_ERROR;
						*http_code = 400;
						ret = -1;
						goto exit_fn;
					}
					*p_end_dis = '\0';

					p_key_dis = p_start_dis;
					p_start_dis = p_end_dis + 2; /* skip '=\"' */
					for (p_end_dis = p_start_dis; *p_end_dis != '\0' && *p_end_dis != '\"'; p_end_dis++);/* p_end_dis point to '\"' */
					if (*p_end_dis != '\"') {
						notice_printf("'\"' not found at end of disposition value.\n");
						*judge_result = JUDGE_ERROR;
						*http_code = 400;
						ret = -1;
						goto exit_fn;
					}
					*p_end_dis = '\0';
					p_value_dis = p_start_dis;

					debug_printf("disposition: '%s' => '%s'\n", p_key_dis, p_value_dis);
					if (0 == strcasecmp(p_key_dis, "name")) {
						p_fkey = p_value_dis;
						p_value_dis = NULL;
					} else if (0 == strcasecmp(p_key_dis, "filename")) {
						p_fname = p_value_dis;
						p_value_dis = NULL;
					} else {/* the key does matter, skip it to parse next */
					}
					if (*(p_end_dis + 1) == ';') {
						/* skip SPACE and TAB after ';' */
						for(p_start_dis = p_end_dis + 2; *p_start_dis == ' ' || *p_start_dis == '\t'; p_start_dis++);
					} else {
						break;
					}
				}
			} else if (0 == strcasecmp(p_key, "Content-Type")) {
				p_ftype = p_value;
				p_value = NULL;
			} else {/* the key does matter, skip it to parse next */
			}
			if (*(p_end + 2) == '\r' && *(p_end + 3) == '\n') {
				p_start = p_end + 4;
				break;
			} else {
				p_start = p_end + 2;
			}
		}
		isend = 0; /* mark all entity is parsed or not */
		p_end = p_start;
		p_entityend = p_entity + u_entitylen;
		while (1) {/* get the file end by this loop */
			boundary_match = 1;
			for (i = 0; i < boundary_split_len; i++) {
				if (*(p_end + i) != *(p_boundary_split + i)) {
					boundary_match = 0;
					break;
				}
			}
			if (boundary_match) {
				fsize = p_end - p_start;
				p_fcontent = p_start;
				break;
			}
			if (p_end + boundary_end_len == p_entityend) {
				boundary_match = 1;
				for (i = 0; i < boundary_end_len; i++) {
					if (*(p_end + i) != *(p_boundary_end + i)) {
						boundary_match = 0;
						break;
					}
				}
				if (boundary_match) {
					fsize = p_end - p_start;
					p_fcontent = p_start;
					isend = 1;
					break;
				} else {
					notice_printf("'boundary_end' not found at end of entity.\n");
					*judge_result = JUDGE_ERROR;
					*http_code = 500;
					ret = -1;
					goto exit_fn;
				}
			} else {
				p_end++;
			}
		}
		if (p_fname != NULL) { /* put file into p_link->file_data if is file */
			p_newfile = (HTTP_FILES *)MY_MALLOC(sizeof(HTTP_FILES));
			if (p_newfile == NULL) {
				emergency_printf("malloc failed!\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 500;
				ret = -1;
				goto exit_fn;
			}
			memset(p_newfile, 0x00, sizeof(HTTP_FILES));
			p_newfile->key = p_fkey;
			p_fkey = NULL;
			p_newfile->fname = p_fname;
			p_fname = NULL;
			p_newfile->ftype = p_ftype;
			p_ftype = NULL;
			p_fcontent[fsize] = '\0'; /* file end with '\0' too, avoid malloc again and copy for text file */
			p_newfile->fcontent = p_fcontent;
			p_fcontent = NULL;
			p_newfile->fsize = fsize;
			fsize = 0;
			if (p_link->file_data == NULL) {
				p_link->file_data = p_newfile;
			} else {
				p_tailfile->next = p_newfile;
			}
			p_tailfile = p_newfile;
			p_newfile = NULL;
		} else { /* put data into p_link->post_data if is not file */
			p_newdata = (HTTP_KVPAIR *)MY_MALLOC(sizeof(HTTP_KVPAIR));
			if (p_newdata == NULL) {
				emergency_printf("malloc failed!\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 500;
				ret = -1;
				goto exit_fn;
			}
			memset(p_newdata, 0x00, sizeof(HTTP_KVPAIR));
			p_newdata->key = p_fkey;
			p_fkey = NULL;
			p_fcontent[fsize] = '\0';
			p_newdata->value = (char *)p_fcontent;
			p_fcontent = NULL;
			fsize = 0;
			if (p_link->post_data == NULL) {
				p_link->post_data = p_newdata;
			} else {
				p_taildata->next = p_newdata;
			}
			p_taildata = p_newdata;
			p_newdata = NULL;
			if (p_ftype) {
				MY_FREE(p_ftype);
				p_ftype = NULL;
			}
		}
		if (!isend) {
			p_start = p_end + strlen(p_boundary_split);
		} else {
			break;
		}
	}
exit_fn:
	if (p_boundary_start != NULL) {
		MY_FREE(p_boundary_start);
	}
	if (p_boundary_split != NULL) {
		MY_FREE(p_boundary_split);
	}
	if (p_boundary_end != NULL) {
		MY_FREE(p_boundary_end);
	}
	return ret;
}

/* parse http message, recvbuf will changed to be argumeng pool while parse */
static int parse_http(HTTP_FD *p_link, E_HTTP_JUDGE *judge_result, int *http_code) {
	char *p_key = NULL, *p_value = NULL, *p_start = NULL, *p_end = NULL, *p_range_start = NULL, *p_range_end = NULL;
	char *p_req_url = NULL, *p_query = NULL, *p_contype = NULL, *p_boundary = NULL, *p_cookie = NULL, *p_range = NULL;
	unsigned char *p_entity = NULL;
	unsigned long u_entitylen = 0;
	HTTP_KVPAIR *p_new = NULL, *p_tail = NULL;
	HTTP_RANGE *p_range_new = NULL, *p_range_tail = NULL;
	int is_multipart = 0, is_over = 0;

	p_start = (char *)p_link->recvbuf;
	for (p_end = p_start; *p_end != ' ' && *p_end != '\0'; p_end++);/* p_end point to the end of method */
	if (*p_end != ' ') {
		notice_printf("SP not found at end of method.\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 400;
		return -1;
	}
	*p_end = '\0';
	p_link->method = p_start; /* get method */

	p_start = p_end + 1;
	for (p_end = p_start; *p_end != ' ' && *p_end != '\0'; p_end++);/* p_end point to the end of url */
	if (*p_end != ' ') {
		notice_printf("SP not found at end of url.\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 400;
		return -1;
	}
	*p_end = '\0';
	p_req_url = p_start; /* get req_url */

	p_start = p_end + 1;
	for (p_end = p_start; *p_end != '\r' && *p_end != '\n' && *p_end != '\0'; p_end++);/* p_end point to the end of http version */
	if (*p_end != '\r' || *(p_end + 1) != '\n') {
		notice_printf("CRLF not found at end of version.\n");
		*judge_result = JUDGE_ERROR;
		*http_code = 400;
		return -1;
	}
	*p_end = '\0';
	p_link->http_version = p_start; /* get http_version */
	p_start = p_end + 2;

	if (0 == strcmp(p_start, "\r\n")) {
		p_start += 2;
	} else {
		while (1) {
			for (p_end = p_start; *p_end != ':' && *p_end != '\0'; p_end++);/* p_end point to the end of key */
			if (*p_end != ':') {
				notice_printf("':' not found at end of header define key.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 400;
				return -1;
			}
			*p_end = '\0';
			p_key = p_start;

			for (p_start = p_end + 1; *p_start == ' ' || *p_start == '\t'; p_start++); /* skip SPACE and TAB after ':' */

			for (p_end = p_start; *p_end != '\r' && *p_end != '\n' && *p_end != '\0'; p_end++);/* p_end point to ehd end of value */
			if (*p_end != '\r' || *(p_end + 1) != '\n') {
				notice_printf("CRLF not found at end of header define value.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 400;
				return -1;
			}
			*p_end = '\0';
			p_value = p_start;

			/* add pair to p_link->header_data */
			p_new = (HTTP_KVPAIR *)MY_MALLOC(sizeof(HTTP_KVPAIR));
			if (p_new == NULL) {
				emergency_printf("malloc failed!\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 500;
				return -1;
			}
			memset(p_new, 0x00, sizeof(HTTP_KVPAIR));
			debug_printf("hfield: '%s' => '%s'\n", p_key, p_value);
			p_new->key = p_key;
			p_new->value = p_value;
			if (p_link->header_data == NULL) {
				p_link->header_data = p_new;
			} else {
				p_tail->next = p_new;
			}
			p_tail = p_new;
			if (*(p_end + 2) == '\r' && *(p_end + 3) == '\n') {
				p_start = p_end + 4;
				break;
			} else {
				p_start = p_end + 2;
			}
		}
	}
	p_entity = (unsigned char *)p_start;/* save current position, will be used when parse entity */
	u_entitylen = p_link->recvbuf + p_link->recvbuf_len - p_entity;
	for (p_start = p_end = p_req_url; *p_end != '\0' && *p_end != '?'; p_end++);/* p_end point to the end of path */
	if (*p_end == '?') {
		*p_end = '\0';
		p_end++;/* skip '?', point to the start of query */
		p_query = p_end;
	}

	p_link->path = p_start;
	urldecode(p_link->path, p_link->path);
	path_merge(p_link->path, p_link->path);
	/* parse query, save information to p_link->query_data */
	p_start = p_query;
	if (p_start != NULL) {
		is_over = 0;
		while (1) {
			for (p_end = p_start; *p_end != '\0' && *p_end != '='; p_end++);/* p_end point to '=' */
			if (*p_end != '=') {
				debug_printf("'=' not found at end of query key.\n");
				break;
			}
			*p_end = '\0';
			p_key = p_start;

			p_start = p_end + 1; /* skip '=' */
			for (p_end = p_start; *p_end != '\0' && *p_end != '&'; p_end++);/* p_end point to '&' or '\0' */
			if (*p_end != '&') {
				is_over = 1;
			}
			*p_end = '\0';
			urldecode(p_start, p_start);
			p_value = p_start;

			/* add pair to p_link->query_data */
			p_new = (HTTP_KVPAIR *)MY_MALLOC(sizeof(HTTP_KVPAIR));
			if (p_new == NULL) {
				emergency_printf("malloc failed!\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 500;
				return -1;
			}
			memset(p_new, 0x00, sizeof(HTTP_KVPAIR));
			debug_printf("query: '%s' => '%s'\n", p_key, p_value);
			p_new->key = p_key;
			p_new->value = p_value;
			if (p_link->query_data == NULL) {
				p_link->query_data = p_new;
			} else {
				p_tail->next = p_new;
			}
			p_tail = p_new;

			if (!is_over) {
				*p_end = '\0';
				p_start = p_end + 1; /* get '&' means not complete yet, skip '&' and continue */
				continue;
			} else {
				break;
			}
		}
	}

	/* parse cookie, save information to p_link->cookie_data */
	p_cookie = (char *)web_header_str(p_link, "Cookie", NULL);
	if (p_cookie != NULL) {
		is_over = 0;
		p_start = p_cookie;
		while (1) {
			for (p_end = p_start; *p_end != '\0' && *p_end != '='; p_end++);/* p_end point to '=' */
			if (*p_end != '=') {
				notice_printf("'=' not found at end of cookie key.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 400;
				return -1;
			}
			*p_end = '\0';
			p_key = p_start;

			p_start = p_end + 1; /* skip '=' */
			for (p_end = p_start; *p_end != '\0' && *p_end != ';'; p_end++);/* p_end point to ';' or '\0' */
			if (*p_end != ';') {
				is_over = 1;
			}
			*p_end = '\0';
			p_value = p_start;

			/* add pair to p_link->cookie_data */
			p_new = (HTTP_KVPAIR *)MY_MALLOC(sizeof(HTTP_KVPAIR));
			if (p_new == NULL) {
				emergency_printf("malloc failed!\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 500;
				return -1;
			}
			memset(p_new, 0x00, sizeof(HTTP_KVPAIR));
			debug_printf("cookie: '%s' => '%s'\n", p_key, p_value);
			p_new->key = p_key;
			p_new->value = p_value;
			if (p_link->cookie_data == NULL) {
				p_link->cookie_data = p_new;
			} else {
				p_tail->next = p_new;
			}
			p_tail = p_new;

			if (!is_over) { /* get ';' means not complete yet, skip ';' and ' ' and continue */
				p_end++;
				while (*p_end == ' ' || *p_end == '\t') {
					p_end++;
				}
				p_start = p_end;
				continue;
			} else {
				break;
			}
		}
	}


	/* parse range, result save to range_data */
	p_range = (char *)web_header_str(p_link, "Range", NULL);
	if (p_range != NULL) {
		/* strlen("bytes=") == 6 */
		if (strncmp("bytes=", p_range, 6) != 0) {
			notice_printf("'bytes=' not found at start of range.\n");
			*judge_result = JUDGE_ERROR;
			*http_code = 416;
			return -1;
		}
		is_over = 0;
		p_start = p_range + 6;
		while (1) {
			for (p_end = p_start; isdigit(*p_end); p_end++); /* p_end point to '-''' */
			if (*p_end != '-') {
				notice_printf("'-' not found at range.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 416;
				return -1;
			}
			*p_end = '\0';
			p_range_start = p_start;

			p_start = p_end + 1; /* skip '-' */
			for (p_end = p_start; isdigit(*p_end); p_end++); /* p_end point to ',' or '\0' */
			if (*p_end != ',' && *p_end != '\0') {
				notice_printf("',' or '\\0' not found at end of a range.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 416;
				return -1;
			}
			if (*p_end == '\0') {
				is_over = 1;
			}
			*p_end = '\0';
			p_range_end = p_start;

			/* save result to chained list p_link->range_data */
			p_range_new = (HTTP_RANGE *)MY_MALLOC(sizeof(HTTP_RANGE));
			if (p_range_new == NULL) {
				emergency_printf("malloc failed!\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 500;
				return -1;
			}
			memset(p_range_new, 0x00, sizeof(HTTP_RANGE));
			debug_printf("range: '%s' to '%s'\n", p_range_start, p_range_end);
			if (p_range_start[0] != '\0') {
				p_range_new->start= atol(p_range_start);
			} else {
				p_range_new->start = RANGE_NOTSET;
			}
			if (p_range_end[0] != '\0') {
				p_range_new->end = atol(p_range_end);
			} else {
				p_range_new->end = RANGE_NOTSET;
			}
			if (p_range_new->start == RANGE_NOTSET && p_range_new->end == RANGE_NOTSET) {
				MY_FREE(p_range_new);
				*judge_result = JUDGE_ERROR;
				*http_code = 416;
				return -1;
			}
			if (p_range_tail == NULL) {
				p_link->range_data = p_range_new;
			} else {
				p_range_tail->next = p_range_new;
			}
			p_range_tail = p_range_new;

			if (!is_over) {
				p_end++;
				p_start = p_end;
				continue;
			} else {
				break;
			}
		}
	}

	/* parse entity if entity not null, save information to p_link->post_data and p_link->file_data */
	if (p_entity[0] != '\0') {
		if (0 == p_link->content_len) {
			notice_printf("no content.\n");
			*judge_result = JUDGE_ERROR;
			*http_code = 411;
			return -1;
		}
		debug_printf("ready for parse entity\n");
		p_contype = (char *)web_header_str(p_link, "Content-Type", "");
		/* strlen("multipart/form-data; boundary=") == 30 */
		if (0 == strncmp("multipart/form-data; boundary=", p_contype, 30)) {
			p_boundary = p_contype + 30;
			if (*p_boundary == '\0') {
				notice_printf("boundary is empty.\n");
				*judge_result = JUDGE_ERROR;
				*http_code = 400;
				return -1;
			}
			is_multipart = 1;
		}
		if (is_multipart) {
			debug_printf("before parse_files\n");
			parse_files(p_link, p_entity, u_entitylen, p_boundary, judge_result, http_code);
			debug_printf("after parse_files\n");
		} else {
			is_over = 0;
			p_start = (char *)p_entity;
			while (1) {
				for (p_end = p_start; *p_end != '\0' && *p_end != '='; p_end++);/* p_end point to '=' */
				if (*p_end != '=') {
					notice_printf("'=' not found at end of post key.\n");
					*judge_result = JUDGE_ERROR;
					*http_code = 400;
					return -1;
				}
				*p_end = '\0';
				p_key = p_start;

				p_start = p_end + 1; /* skip '=' */
				for (p_end = p_start; *p_end != '\0' && *p_end != '&'; p_end++);/* p_end point to '&' or '\0' */
				if (*p_end != '&') {
					is_over = 1;
				}
				*p_end = '\0';
				urldecode(p_start, p_start);
				p_value = p_start;

				/* save information to p_link->post_data */
				p_new = (HTTP_KVPAIR *)MY_MALLOC(sizeof(HTTP_KVPAIR));
				if (p_new == NULL) {
					emergency_printf("malloc failed!\n");
					*judge_result = JUDGE_ERROR;
					*http_code = 500;
					return -1;
				}
				memset(p_new, 0x00, sizeof(HTTP_KVPAIR));
				debug_printf("post: '%s' => '%s'\n", p_key, p_value);
				p_new->key = p_key;
				p_new->value = p_value;
				if (p_link->post_data == NULL) {
					p_link->post_data = p_new;
				} else {
					p_tail->next = p_new;
				}
				p_tail = p_new;

				if (!is_over) {
					p_start = p_end + 1; /* get '&' means not complete yet, skip '&' and continue */
					continue;
				} else {
					break;
				}
			}
		}
	}
	return 0;
}

/* judge http request is complete or not, call parse_http to get information if is complete */
static int judge_req(HTTP_FD *p_link, E_HTTP_JUDGE *judge_result, int *http_code) {
	char p_key[16], p_value[16], *p_headend = NULL;
	const char *p_start = NULL, *p_end = NULL;
	int is_complete = 0;
	unsigned int cur_entity_len = 0, head_len = 0;

	*http_code = 0;
	p_headend = strstr((char *)(p_link->recvbuf), "\r\n\r\n");
	if (p_headend == NULL) {
		if (g_max_head_len && p_link->recvbuf_len > g_max_head_len) {
			*http_code = 414;
			*judge_result = JUDGE_ERROR;
		} else {
			*judge_result = JUDGE_CONTINUE;
		}
		goto exit_fn;
	}
	head_len = p_headend - (char *)(p_link->recvbuf);
	if (strncmp((char *)(p_link->recvbuf), "GET ", 4) && p_link->content_len == 0) {
		/* parse again is unnecessary if method is GET or Content-Length is parsed */
		p_start = strstr((char *)p_link->recvbuf, "\r\n");
		if (p_start != p_headend) {
			p_start += 2;
			*p_headend = '\0';
			while (1) {
				for (p_end = p_start; *p_end != ':' && *p_end != '\0'; p_end++);/* find ':' */
				if (*p_end == '\0') {
					*http_code = 400;
					*judge_result = JUDGE_ERROR;
					goto exit_fn;
				}
				if (p_end - p_start == 14) { // strlen("Content-Length") is 14
					memcpy(p_key, p_start, 14);
					p_key[14] = '\0';
					if (0 == strcasecmp(p_key, "Content-Length")) {
						for (p_start = p_end + 1; *p_start == ' ' || *p_start == '\t'; p_start++);/* find SPACE or TAB */
						for (p_end = p_start; *p_end != '\r' && *p_end != '\0'; p_end++);/* find line end */
						if (p_end - p_start > 10) {
							*http_code = 413;
							*judge_result = JUDGE_ERROR;
							goto exit_fn;
						}
						memcpy(p_value, p_start, p_end - p_start);
						*(p_value + (p_end - p_start)) = '\0';
						p_link->content_len = (unsigned int)atoi(p_value);
						/* exit if Content-Length is founded */
						if (g_max_entity_len && p_link->content_len > g_max_entity_len) {
							*http_code = 413;
							*judge_result = JUDGE_ERROR;
							goto exit_fn;
						}
						/* get Content-Length, the size of request is clear, realloc enough space to avoid realloc again */
						if (p_link->recvbuf_space < p_link->content_len + (head_len + 4) + 1) {
							p_link->recvbuf_space = p_link->content_len + (head_len + 4) + 1;
							p_link->recvbuf = (unsigned char *)MY_REALLOC(p_link->recvbuf, p_link->recvbuf_space);
							if (p_link->recvbuf == NULL) {
								emergency_printf("realloc failed!\n");
								*http_code = 500;
								*judge_result = JUDGE_ERROR;
								goto exit_fn;
							}
						}
						break;
					}
				}
				for (p_end = p_start; *p_end != '\r' && *p_end != '\0'; p_end++);/* find line end */

				if (*p_end == '\0') {
					/* exit if Content-Length is not founded */
					break;
				} else {
					if (0 == strncmp(p_end, "\r\n", 2)) {
						p_start = p_end + 2;
					} else {
						*http_code = 400;
						*judge_result = JUDGE_ERROR;
						goto exit_fn;
					}
				}
			}
		}
	}
	if (p_link->content_len == 0) {
		*judge_result = JUDGE_COMPLETE;
		is_complete = 1;
		goto exit_fn;
	}
	cur_entity_len = p_link->recvbuf_len - (head_len + 4);
	p_link->need_len = p_link->content_len - cur_entity_len;
	if (p_link->need_len > 0) {
		*judge_result = JUDGE_CONTINUE;
		goto exit_fn;
	} else if (p_link->need_len == 0) {
		*judge_result = JUDGE_COMPLETE;
		is_complete = 1;
		goto exit_fn;
	} else {
		*http_code = 400;
		*judge_result = JUDGE_ERROR;
		goto exit_fn;
	}
exit_fn:
	if (head_len) {
		*(p_link->recvbuf + head_len) = '\r';
	}
	if (is_complete) { /* start parse if request is complete */
		p_link->state = STATE_RESPONSING;
		debug_printf("before parse_http\n");
		parse_http(p_link, judge_result, http_code);
		debug_printf("after parse_http\n");
	}
	return 0;
}

/* configure http header of response */
int web_set_header(HTTP_FD *p_link, const char *name, const char *value) {
	HTTP_KVPAIR *p_new = NULL, *p_tail = NULL;

	if (!name || name[0] == '\0') {
		return -1;
	}
	/* save information to p_link->cnf_header */
	p_new = (HTTP_KVPAIR *)MY_MALLOC(sizeof(HTTP_KVPAIR));
	if (p_new == NULL) {
		emergency_printf("malloc failed!\n");
		return -1;
	}
	memset(p_new, 0x00, sizeof(HTTP_KVPAIR));
	p_new->key = (char *)MY_MALLOC(strlen(name) + 1);
	if (p_new->key == NULL) {
		emergency_printf("malloc failed!\n");
		MY_FREE(p_new);
		return -1;
	}
	strcpy(p_new->key, name);
	if (value && value[0] != '\0') {
		p_new->value = (char *)MY_MALLOC(strlen(value) + 1);
		if (p_new->value == NULL) {
			emergency_printf("malloc failed!\n");
			MY_FREE(p_new->key);
			MY_FREE(p_new);
			return -1;
		}
		strcpy(p_new->value, value);
	}
	if (p_link->cnf_header == NULL) {
		p_link->cnf_header = p_new;
	} else {
		for (p_tail = p_link->cnf_header; p_tail->next != NULL; p_tail = p_tail->next);
		p_tail->next = p_new;
	}
	return 0;
}

/* cancel the configuration of http header */
int web_unset_header(HTTP_FD *p_link, const char *name) {
	HTTP_KVPAIR *p_pre = NULL, *p_cur = NULL, *p_next = NULL;
	const char *p_keypos = NULL, *p_inkeypos = NULL;
	int is_same = 1;

	if (!name || name[0] == '\0') {
		return -1;
	}
	p_pre = NULL;
	for (p_cur = p_link->cnf_header; p_cur != NULL; p_cur = p_next) {
		p_next = p_cur->next;
		is_same = 1;
		for (p_keypos = p_cur->key, p_inkeypos = name; ;p_keypos++, p_inkeypos++) {
			if (*p_keypos == *p_inkeypos) {
			} else if (isalpha(*p_keypos) && ((*p_keypos) & 0xdf) == ((*p_inkeypos) & 0xdf)) {
				/* the diffrence between upper character and the lower is 0x20, so it will equal if not compare the diffrent bit */
			} else if (*p_keypos == '-' && *p_inkeypos == '_') {
			} else if (*p_keypos == '_' && *p_inkeypos == '-') {
			} else {
				is_same = 0;
				break;
			}
			if (*p_keypos == '\0') {
				break;
			}
		}
		if (is_same) {
			if (p_pre == NULL) {
				p_link->cnf_header = p_next;
			} else {
				p_pre->next = p_next;
			}
			MY_FREE(p_cur->key);
			if (p_cur->value) {
				MY_FREE(p_cur->value);
			}
			MY_FREE(p_cur);
		} else {
			p_pre = p_cur;
		}
	}
	return 0;
}

int web_vprintf(HTTP_FD *p_link, const char *format, va_list args) {
	return tt_buffer_vprintf(&(p_link->response_entity), format, args);
}

int web_printf(HTTP_FD *p_link, const char *format, ...) {
	int rc;
	va_list args;
	va_start(args, format);
	rc = web_vprintf(p_link, format, args);
	va_end(args);
	return rc;
}

int web_write(HTTP_FD *p_link, const void *content, unsigned int content_len) {
	return tt_buffer_write(&(p_link->response_entity), content, content_len);
}

int web_no_copy(HTTP_FD *p_link, void *content, unsigned int used, unsigned int space, int is_malloced) {
	return tt_buffer_no_copy(&(p_link->response_entity), (unsigned char *)content, used, space, is_malloced);
}

const char *get_mime_type(const char *p_path) {
	int pre_is_slash = 0, i = 0;
	const char *p_char = NULL, *p_suffix = "";

	static HTTP_MIME_MAP mime_map[] = {
			{".html", "text/html"},
			{".htm", "text/html"},
			{".xhtml", "text/html"},
			{".js", "text/javascript"},
			{".wasm", "application/wasm"},
			{".css", "text/css"},
			{".txt", "text/plain"},
			{".gif", "image/gif"},
			{".jpg", "image/jpeg"},
			{".jpe", "image/jpeg"},
			{".jpeg", "image/jpeg"},
			{".png", "image/png"},
			{".ico", "image/x-icon"},
			{".bmp", "image/bmp"},
			{".mp4", "video/mp4"},
			{".mkv", "video/x-maltroska"},
			{".flv", "video/x-flv"},
			{".json", "application/json"},
			{".woff2", "application/x-font-woff"},
			{".woff", "application/x-font-woff"},
			{".svg", "image/svg+xml"},
			{".eot", "application/vnd.ms-fontobject"},
			{".otf", "application/octet-stream"},
			{".ttf", "application/octet-stream"},
			{".crt", "application/x-x509-ca-cert"},
			{".p12", "application/x-pkcs12"},
			{NULL, NULL}
		};

	pre_is_slash = 0;
	for (p_char = p_path; *p_char != '\0'; p_char++) {
		if (pre_is_slash) {
			pre_is_slash = 0;
			continue;
		}
		if (*p_char == '/') {
			pre_is_slash = 1;
			p_suffix = "";
		} else if (*p_char == '.' && *(p_char + 1) != '\0') {
			p_suffix = p_char;
		}
	}
	for (i = 0; mime_map[i].suffix != NULL; i++) {
		if (0 != strcasecmp(p_suffix, mime_map[i].suffix)) {
			continue;
		}
		return mime_map[i].mime;
	}
	return "application/octet-stream";
}
int web_fin(HTTP_FD *p_link, int http_code) {
	HTTP_KVPAIR *p_cur = NULL;
	int i = 0;
	const char *status_text = "(unknown)", *p_value = NULL;
	static HTTP_CODE_MAP code_map[] = {
			{100, "Continue", NULL},
			{101, "Switching Protocols", NULL},
			{200, "OK", NULL},
			{201, "Created", NULL},
			{202, "Accepted", NULL},
			{203, "Non-Authoritative Information", NULL},
			{204, "No Content", NULL},
			{205, "Reset Content", NULL},
			{206, "Partial Content", NULL},
			{300, "Multiple Choices", NULL},
			{301, "Moved Permanently", NULL},
			{302, "Found", NULL},
			{303, "See Other", NULL},
			{304, "Not Modified", NULL},
			{305, "Use Proxy", NULL},
			{306, "(Unused)", NULL},
			{307, "Temporary Redirect", NULL},
			{400, "Bad Request", NULL},
			{401, "Unauthorized", NULL},
			{402, "Payment Required", NULL},
			{403, "Forbidden", "<html><h1>403 Forbidden</h1></html>"},
			{404, "Not Found", "<html><h1>404 Not Found</h1></html>"},
			{405, "Method Not Allowed", "<html><h1>405 Method Not Allowed</h1></html>"},
			{406, "Not Acceptable", NULL},
			{407, "Proxy Authentication Required", NULL},
			{408, "Request Timeout", "<html><h1>408 Request Timeout</h1></html>"},
			{409, "Conflict", NULL},
			{410, "Gone", NULL},
			{411, "Length Required", NULL},
			{412, "Precondition Failed", NULL},
			{413, "Request entity Too Large", "<html><h1>413 Request entity Too Large</h1></html>"},
			{414, "Request-URI Too Long", "<html><h1>414 Request-URI Too Long</h1></html>"},
			{415, "Unsupported Media Type", NULL},
			{416, "Requested Range Not Satisfiable", NULL},
			{417, "Expectation Failed", NULL},
			{500, "Internal Server Error", NULL},
			{501, "Not Implemented", NULL},
			{502, "Bad Gateway", "<html><h1>502 Bad Gateway</h1></html>"},
			{503, "Service Unavailable", NULL},
			{504, "Gateway Timeout", NULL},
			{505, "HTTP Version Not Supported", NULL},
			{-1, NULL, NULL}
		};

	static const char *hfields[][2] = {
			{"Server", "unknown"},
			{"Accept-Ranges", "bytes"},
			{"Date", NULL},
			{"WWW-Authenticate", NULL},
			{"Location", NULL},
			{"Set-Cookie", NULL},
			{"Pragma", "no-cache"},
			{"Cache-Control", "no-store"},
			{"Content-Encoding", NULL},
			{"Content-Length", NULL},
			{"Content-Range", NULL},
			{"Content-Type", "text/html"},
			{"Content-Disposition", NULL},
			{"Etag", NULL},
			{"Last-Modified", NULL},
			{"Connection","Keep-Alive"},
			{NULL, NULL}
		};
	if (0 && http_code != 101 && http_code != 200 && http_code != 206 && http_code != 302 && http_code != 304) {
		printf("---------------------\n");
		printf("response code %d\n", http_code);
		hexdump(p_link->recvbuf, p_link->recvbuf_len);
	}
	if (p_link->response_head.used) {
		if (p_link->path == NULL) {
			printf("duplicate call \"web_fin\" @ \"(null)\"!\n");
		} else {
			printf("duplicate call \"web_fin\" @ \"%s\"!\n", p_link->path);
		}
		return -1;
	}
	if (http_code == 500) {
		tt_buffer_no_copy(&(p_link->response_head), (unsigned char *)g_err_500_head, strlen(g_err_500_head), 0, 0);
		tt_buffer_no_copy(&(p_link->response_entity), (unsigned char *)g_err_500_entity, strlen(g_err_500_entity), 0, 0);
	} else {
		/* find status by response code, auto generate entity if method is not HEAD and entify is undefined by user but found in code_map */
		for (i = 0; code_map[i].code != -1; i++) {
			if (http_code != code_map[i].code) {
				continue;
			}
			status_text = code_map[i].status_text;
			if (p_link->response_entity.used == 0 && code_map[i].entity != NULL) {
				web_no_copy(p_link, (unsigned char *)code_map[i].entity, strlen((const char *)code_map[i].entity), 0, 0);
			}
			break;
		}
		/* generate http version, response code and status */
		tt_buffer_printf(&(p_link->response_head), "HTTP/1.1 %d %s\r\n", http_code, status_text);
		for (i = 0; hfields[i][0] != NULL; i++) {
			p_value = web_cnf_header_str(p_link, hfields[i][0], "");
			if (p_value == NULL) { /* NULL means need remove */
				web_unset_header(p_link, hfields[i][0]);
				continue;
			} else if (p_value[0] == '\0') { /* "" means not modified by user */
				if (0 == strcasecmp(hfields[i][0], "Content-Length")) {
					tt_buffer_printf(&(p_link->response_head), "Content-Length: %" SIZET_FMT "\r\n", p_link->response_entity.used);
				} else if (0 == strcasecmp(hfields[i][0], "Content-Type")) {
					if (p_link->path == NULL || http_code != 200) {
						tt_buffer_printf(&(p_link->response_head), "Content-Type: text/html\r\n");
					} else {
						tt_buffer_printf(&(p_link->response_head), "Content-Type: %s\r\n", get_mime_type(p_link->path));
					}
				} else if (0 == strcasecmp(hfields[i][0], "Server")) {
					tt_buffer_printf(&(p_link->response_head), "Server: %s\r\n", g_hostname);
				} else if (0 == strcasecmp(hfields[i][0], "Connection")) {
					if (p_link->state == STATE_CLOSING) {
						tt_buffer_printf(&(p_link->response_head), "Connection: close\r\n");
					} else {
						tt_buffer_printf(&(p_link->response_head), "Connection: %s\r\n", hfields[i][1]);
					}
				} else {
					if (hfields[i][1]) {
						tt_buffer_printf(&(p_link->response_head), "%s: %s\r\n", hfields[i][0], hfields[i][1]);
					}
				}
			} else {
				tt_buffer_printf(&(p_link->response_head), "%s: %s\r\n", hfields[i][0], p_value);
				web_unset_header(p_link, hfields[i][0]);
			}
		}
		for (p_cur = p_link->cnf_header; p_cur != NULL; p_cur = p_cur->next) {
			tt_buffer_printf(&(p_link->response_head), "%s: %s\r\n", p_cur->key, p_cur->value);
		}
		tt_buffer_printf(&(p_link->response_head), "\r\n");
	}
	/* remove entity if method is HEAD */
	if (p_link->method != NULL && 0 == strcmp(p_link->method, "HEAD")) {
		p_link->response_entity.used = 0;
	}
	if (p_link->state != STATE_CLOSED) { /* should not active again after closed */
		p_link->state = STATE_SENDING;
	}
	p_link->send_state = SENDING_HEAD;
	return 0;
}

/* response server is busy */
int web_busy_response(HTTP_FD *p_link) {
	web_printf(p_link, "<h1>Server is too busy!</h1>");
	web_set_header(p_link, "Retry-After", "5");
	p_link->state = STATE_CLOSING;
	web_fin(p_link, 503);
	return 0;
}
#ifdef WITH_SSL
/* load certificate from memory */
static X509 *load_certificate_mem(const char *cert_b64) {
	BIO *bio_in = NULL;
	X509 *x509 = NULL;
	bio_in = BIO_new_mem_buf(cert_b64, strlen(cert_b64));
	if (bio_in == NULL) {
		emergency_printf("create bio failed.\n");
		return NULL;
	}
	x509 = PEM_read_bio_X509(bio_in, NULL, NULL, NULL);
	BIO_free(bio_in);
	bio_in = NULL;
	return x509;
}

/* define the callback of get password, or will use default callback PEM_def_callback and will block at "Enter PEM pass phrase:" */
static int my_pem_password_cb(char *buf, int num, int w, void *key) {
	if (key == NULL) {
		return -1;
	} else {
		return PEM_def_callback(buf, num, w, key);
	}
}

/* load private key that base64 encoded */
static EVP_PKEY *load_privatekey_mem(const char *key_b64, const char *password) {
	BIO *bio_in = NULL;
	EVP_PKEY *pkey = NULL;
	bio_in = BIO_new_mem_buf(key_b64, strlen(key_b64));
	if (bio_in == NULL) {
		emergency_printf("create bio failed.\n");
		return NULL;
	}
	pkey = PEM_read_bio_PrivateKey(bio_in, NULL, my_pem_password_cb, (void *)password);
	BIO_free(bio_in);
	bio_in = NULL;
	return pkey;
}

/* get issuer name of certificate */
static const char *get_x509_issuer_name(X509 *x509, const char *key){
	X509_NAME_ENTRY *ent = NULL;
	const ASN1_STRING *val = NULL;
	BIO *bio_out = NULL;
	X509_NAME *name = NULL;
	static char buf[256];
	int i = 0, cnt = 0;

	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL) {
		return "";
	}
	name = X509_get_issuer_name(x509);
	if (key && key[0]) {
		cnt = X509_NAME_entry_count(name);
		for (i = 0; i < cnt; i++) {
			ent = X509_NAME_get_entry(name, i);
			if (0 == strcmp(key, OBJ_nid2sn(OBJ_obj2nid(X509_NAME_ENTRY_get_object(ent))))) {
				val = X509_NAME_ENTRY_get_data(ent);
				BIO_write(bio_out, val->data, val->length);
				break;
			}
		}
	} else {
		X509_NAME_print_ex(bio_out, name, 0, XN_FLAG_SEP_SPLUS_SPC);
	}
	memset(buf, 0x00, sizeof(buf));
	BIO_read(bio_out, buf, sizeof(buf) - 1);
	BIO_free(bio_out);
	return (const char *)buf;
}

/* get subject name of certificate */
static const char *get_x509_subject_name(X509 *x509, const char *key){
	X509_NAME_ENTRY *ent = NULL;
	const ASN1_STRING *val = NULL;
	BIO *bio_out = NULL;
	X509_NAME *name = NULL;
	static char buf[256];
	int i = 0, cnt = 0;

	bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL) {
		return "";
	}
	name = X509_get_subject_name(x509);
	if (key && key[0]) {
		cnt = X509_NAME_entry_count(name);
		for (i = 0; i < cnt; i++) {
			ent = X509_NAME_get_entry(name, i);
			if (0 == strcmp(key, OBJ_nid2sn(OBJ_obj2nid(X509_NAME_ENTRY_get_object(ent))))) {
				val = X509_NAME_ENTRY_get_data(ent);
				BIO_write(bio_out, val->data, val->length);
				break;
			}
		}
	} else {
		X509_NAME_print_ex(bio_out, name, 0, XN_FLAG_SEP_SPLUS_SPC);
	}
	memset(buf, 0x00, sizeof(buf));
	BIO_read(bio_out, buf, sizeof(buf) - 1);
	BIO_free(bio_out);
	return (const char *)buf;
}

/* get the time of start of certificate */
static const char *get_x509_notBefore(X509 *x509){
	static char buf[256];
	BIO *bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL) {
		return "";
	}
	ASN1_TIME_print(bio_out, X509_get_notBefore(x509));
	memset(buf, 0x00, sizeof(buf));
	BIO_read(bio_out, buf, sizeof(buf) - 1);
	BIO_free(bio_out);
	return (const char *)buf;
}

/* get the time of end of certificate */
static const char *get_x509_notAfter(X509 *x509){
	static char buf[256];
	BIO *bio_out = BIO_new(BIO_s_mem());
	if (bio_out == NULL) {
		return "";
	}
	ASN1_TIME_print(bio_out, X509_get_notAfter(x509));
	memset(buf, 0x00, sizeof(buf));
	BIO_read(bio_out, buf, sizeof(buf) - 1);
	BIO_free(bio_out);
	return (const char *)buf;
}

static void cert_info_print(X509 *x509){
	const char *name = NULL;
	if (x509 == NULL) {
		return;
	}
	printf("version: V%ld\n", X509_get_version(x509) + 1);
	if ((name = get_x509_issuer_name(x509, "CN"))[0]) {
		printf("issuer: %s\n", name);
	} else if ((name = get_x509_issuer_name(x509, "OU"))[0]) {
		printf("issuer: %s\n", name);
	} else {
		printf("issuer: %s\n", get_x509_issuer_name(x509, "O"));
	}
	if ((name = get_x509_subject_name(x509, "CN"))[0]) {
		printf("subject: %s\n", name);
	} else if ((name = get_x509_subject_name(x509, "OU"))[0]) {
		printf("subject: %s\n", name);
	} else {
		printf("subject: %s\n", get_x509_subject_name(x509, "O"));
	}
	printf("time: %s ~ %s\n", get_x509_notBefore(x509), get_x509_notAfter(x509));
}

static int SSL_CTX_use_certificate_mem(SSL_CTX *ssl_ctx, const char *pem_cert) {
	int ret = 1;
	X509 *x509 = NULL;
	x509 = load_certificate_mem(pem_cert);
	if (x509 == NULL) {
		emergency_printf("read x509 failed.\n");
		return -1;
	}
	cert_info_print(x509);
	if (SSL_CTX_use_certificate(ssl_ctx, x509) <= 0) {
		emergency_printf("import certificate failed.\n");
		ret = -1;
	}
	X509_free(x509);
	return ret;
}

static int SSL_CTX_use_PrivateKey_mem(SSL_CTX *ssl_ctx, const char *privkey, const char *password) {
	int ret = 1;
	EVP_PKEY *pkey = NULL;
	pkey = load_privatekey_mem(privkey, password);
	if (pkey == NULL) {
		emergency_printf("read privatekey failed.\n");
		return -1;
	}
	if (SSL_CTX_use_PrivateKey(ssl_ctx, pkey) <= 0) {
		emergency_printf("import privkey failed.\n");
		ret = -1;
	}
	EVP_PKEY_free(pkey);
	return ret;
}

/* create SSL context, return NULL if failed */
SSL_CTX *create_ssl_ctx(const char *svr_cert, const char *privkey, const char *password) {
	SSL_CTX *ssl_ctx = NULL;
	int ret = 0;

	if (privkey == NULL) {
		privkey = svr_cert;
	}
	ssl_ctx = SSL_CTX_new(SSLv23_server_method());
	if (ssl_ctx == NULL) {
		emergency_printf("create ssl_ctx failed.\n");
		ret = -1;
		goto exit_fn;
	}
	if (SSL_CTX_use_certificate_mem(ssl_ctx, svr_cert) <= 0) {
		emergency_printf("import certificate failed.\n");
		ret = -1;
		goto exit_fn;
	}

	if (SSL_CTX_use_PrivateKey_mem(ssl_ctx, privkey, password) <= 0) {
		emergency_printf("import privkey failed.\n");
		ret = -1;
		goto exit_fn;
	}

	if (!SSL_CTX_check_private_key(ssl_ctx)) {
		emergency_printf("privkey is invalid.\n");
		ret = -1;
		goto exit_fn;
	}
exit_fn:
	if (ret != 0) {
		if (ssl_ctx != NULL) {
			SSL_CTX_free(ssl_ctx);
			ssl_ctx = NULL;
		}
	}
	return ssl_ctx;
}
#endif /* endof WITH_SSL */
static int reset_link_for_continue(HTTP_FD *p_link) {
	p_link->recvbuf_len = 0;
	p_link->content_len = 0;
	p_link->need_len = 0;
	p_link->state = STATE_RECVING;
	p_link->tm_last_active = p_link->tm_last_req = time(0);
	p_link->method = NULL;
	p_link->path = NULL;
	p_link->http_version = NULL;
	if (p_link->header_data) {
		free_kvpair_shallow(&(p_link->header_data));
	}
	if (p_link->cookie_data) {
		free_kvpair_shallow(&(p_link->cookie_data));
	}
	if (p_link->range_data) {
		free_ranges(&(p_link->range_data));
	}
	if (p_link->query_data) {
		free_kvpair_shallow(&(p_link->query_data));
	}
	if (p_link->post_data) {
		free_kvpair_shallow(&(p_link->post_data));
	}
	if (p_link->file_data) {
		free_files(&(p_link->file_data));
	}
	if (p_link->cnf_header) {
		free_kvpair_deep(&(p_link->cnf_header));
	}
	p_link->send_state = SENDING_HEAD;
	p_link->response_head.used = 0;
	p_link->response_entity.used = 0;
	if (p_link->recvbuf_space >= 8192) { /* free it if space > 8K, call malloc again if necessary */
		if (p_link->recvbuf) {
			MY_FREE(p_link->recvbuf);
			p_link->recvbuf = NULL;
		}
		p_link->recvbuf_space = 0;
	}
	return 0;
}

static int free_link_all(HTTP_FD *p_link) {
	reset_link_for_continue(p_link);
#ifdef WITH_SSL
	if (p_link->ssl) {
		/* libevent will free ssl */
		// SSL_free(p_link->ssl);
		p_link->ssl = NULL;
	}
#endif
	if (p_link->recvbuf) {
		MY_FREE(p_link->recvbuf);
		p_link->recvbuf = NULL;
	}
	p_link->recvbuf_space = 0;
	tt_buffer_free(&(p_link->response_head));
	tt_buffer_free(&(p_link->response_entity));
#ifdef WITH_WEBSOCKET
	tt_buffer_free(&(p_link->ws_recvq));
	tt_buffer_free(&(p_link->ws_sendq));
	tt_buffer_free(&(p_link->ws_data));
	tt_buffer_free(&(p_link->ws_response));
#endif
	if (p_link->bev != NULL) {
		bufferevent_free(p_link->bev);
		p_link->bev = NULL;
	}
	MY_FREE(p_link);
	return 0;
}

static void apply_change(HTTP_FD *p_link) {
	if (p_link->state == STATE_CLOSED) {
		if (g_http_links == p_link) {
			g_http_links = p_link->next;
		}
		if (p_link->next) {
			p_link->next->prev = p_link->prev;
		}
		if (p_link->prev) {
			p_link->prev->next = p_link->next;
		}
		free_link_all(p_link);
		p_link = NULL;
		return;
	}
	if (p_link->state == STATE_RECVING) {
		bufferevent_enable(p_link->bev, EV_READ);
		bufferevent_disable(p_link->bev, EV_WRITE);
	} else if (p_link->state == STATE_SENDING || p_link->state == STATE_CLOSING) {
		bufferevent_disable(p_link->bev, EV_READ);
		if (p_link->sending_len == 0) {
			if (p_link->send_state == SENDING_HEAD) {
				bufferevent_write(p_link->bev, p_link->response_head.content, p_link->response_head.used);
				p_link->sending_len = p_link->response_head.used;
			} else {
				bufferevent_write(p_link->bev, p_link->response_entity.content, p_link->response_entity.used);
				p_link->sending_len = p_link->response_entity.used;
			}
			bufferevent_enable(p_link->bev, EV_WRITE);
		} else {
			bufferevent_disable(p_link->bev, EV_WRITE);
		}
#ifdef WITH_WEBSOCKET
	} else if (p_link->state == STATE_WS_HANDSHAKE) {
		bufferevent_disable(p_link->bev, EV_READ);
		if (p_link->sending_len == 0) {
			if (p_link->send_state == SENDING_HEAD) {
				bufferevent_write(p_link->bev, p_link->response_head.content, p_link->response_head.used);
				p_link->sending_len = p_link->response_head.used;
			} else {
				bufferevent_write(p_link->bev, p_link->response_entity.content, p_link->response_entity.used);
				p_link->sending_len = p_link->response_entity.used;
			}
			bufferevent_enable(p_link->bev, EV_WRITE);
		}
	} else if (p_link->state == STATE_WS_CONNECTED) {
		bufferevent_enable(p_link->bev, EV_READ);
		if (p_link->ws_sendq.used > 0) {
			if (!p_link->sending_len) {
				bufferevent_write(p_link->bev, p_link->ws_sendq.content, p_link->ws_sendq.used);
				p_link->sending_len = p_link->ws_sendq.used;
				bufferevent_enable(p_link->bev, EV_WRITE);
			} else {
			}
		} else {
			bufferevent_disable(p_link->bev, EV_WRITE);
		}
#endif
	} else {
		emergency_printf("unexpected state %d!\n", p_link->state);
	}
}
static void event_cb_web(struct bufferevent *bev, short event, void *user_data) {
	HTTP_FD *p_link = (HTTP_FD *)user_data;

	if (event & BEV_EVENT_EOF) {
		debug_printf("Connection closed.\n");
		p_link->state = STATE_CLOSED;
	} else if (event & BEV_EVENT_ERROR) {
		debug_printf("Connectiont Error: %s\n", strerror(errno));
		p_link->state = STATE_CLOSED;
	} else if (event & BEV_EVENT_CONNECTED) {
		debug_printf("Connected.\n");
	} else {
		error_printf("no handler for event %04x\n", event);
	}
	apply_change(p_link);
}
static void http_read(HTTP_FD *p_link) {
	struct evbuffer *input = NULL;
	size_t recv_len = 0;
	int http_code = 500;	
	E_HTTP_JUDGE judge_result = JUDGE_ERROR;
	time_t tm_now;

	input = bufferevent_get_input(p_link->bev);
	if (input == NULL) {
		return;
	}
	recv_len = evbuffer_get_length(input);

	tm_now = time(0);
	p_link->tm_last_active = tm_now;
	if (p_link->recvbuf == NULL) {
		p_link->recvbuf_space = g_init_recv_space;
		p_link->recvbuf = (unsigned char *)MY_MALLOC(p_link->recvbuf_space);
		if (p_link->recvbuf == NULL) {
			emergency_printf("malloc failed!\n");
			web_fin(p_link, 500);
			return;
		}
	}
	if (p_link->recvbuf_len + recv_len >= p_link->recvbuf_space) {
		while (p_link->recvbuf_len + recv_len >= p_link->recvbuf_space) {
			p_link->recvbuf_space <<= 1;
		}
		p_link->recvbuf = (unsigned char *)MY_REALLOC(p_link->recvbuf, p_link->recvbuf_space);
		if (p_link->recvbuf == NULL) {
			emergency_printf("realloc failed!\n");
			web_fin(p_link, 500);
			return;
		}
	}
	bufferevent_read(p_link->bev, p_link->recvbuf + p_link->recvbuf_len, recv_len);
	p_link->recvbuf_len += recv_len;
	*(p_link->recvbuf + p_link->recvbuf_len) = '\0';
	if (p_link->content_len) {
		p_link->need_len -= recv_len;
	}
	// printf("--------------------------------\n");
	// hexdump(p_link->recvbuf, p_link->recvbuf_len);
	if (p_link->content_len && p_link->need_len != 0) {
		if (p_link->need_len > 0) {
			judge_result = JUDGE_CONTINUE;
		} else {
			http_code = 400;
			judge_result = JUDGE_ERROR;
		}
	} else {
		debug_printf("enter judge_req\n");
		judge_req(p_link, &judge_result, &http_code);
		debug_printf("leave judge_req\n");
	}
	debug_printf("judge:%d,%d\n", judge_result, http_code);
	if (judge_result == JUDGE_COMPLETE) {
		debug_printf("enter req_dispatch\n");
		req_dispatch(p_link);
		p_link->tm_last_req = tm_now;
		debug_printf("leave req_dispatch\n");
	} else if (judge_result == JUDGE_CONTINUE) {
		return;
	} else if (judge_result == JUDGE_ERROR) {
		web_set_header(p_link, "Connection", "close");
		p_link->state = STATE_CLOSING;
		web_fin(p_link, http_code);
	}
	return;
}

#ifdef WITH_WEBSOCKET
/*
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
*/
static E_WS_DECODE_RET ws_unpack(HTTP_FD *p_link) {
	int i = 0;
	unsigned char *buf = NULL, fin = 0, opcode = 0, *plain = NULL, *plain_tail = NULL, mask[4];
	size_t payload_len = 0, pre_datalen = 0;

	if (p_link->ws_recvq.used < 6) {
		return WS_DECODE_NEEDMORE;
	}
	buf = p_link->ws_recvq.content;
	fin = buf[0] & 0x80;
	opcode = buf[0] & 0x0f;
	/* opcode: 0x1: Text, 0x2: binary, 0x8: close connection, 0x9: ping, 0xA: pong */
	if (opcode == 0x08) {
		return WS_DECODE_CLOSED;
	}
	if (!(buf[1] & 0x80)) { /* data must be masked from client to server */
		return WS_DECODE_ERROR;
	}
	payload_len = buf[1] & 0x7f;
	if (payload_len == 126) { /* extended payload length is 16 bit */
		if (p_link->ws_recvq.used <= 125 + 8) { /* payload length is bigger the 125 if use 16 bit, 2+2 bytes (parsed) + 4 bytes (mask) */
			return WS_DECODE_NEEDMORE;
		}
		payload_len = buf[2] << 8 | buf[3] << 1;
		buf += 4;
	} else if (payload_len == 127) { /* extended payload length is 64 bit */
		if (p_link->ws_recvq.used <= 65535 + 14) { /* payload length is bigger the 65535 if use 64 bit, 2+8 bytes (parsed) + 4 bytes (mask) */
			return WS_DECODE_NEEDMORE;
		}
		payload_len = ((size_t)buf[2] << 56) | ((size_t)buf[3] << 48) | ((size_t)buf[4] << 40) | ((size_t)buf[5] << 32)\
			| (buf[6] << 24) | (buf[7] << 16) | (buf[8] << 8) | buf[9];
		buf += 10;
	} else {
		buf += 2;
	}
	memcpy(mask, buf, 4);
	buf += 4;
	if (p_link->ws_recvq.used - (buf - p_link->ws_recvq.content) < payload_len) {
		return WS_DECODE_NEEDMORE;
	}
	pre_datalen = p_link->ws_data.used;
	tt_buffer_write(&(p_link->ws_data), buf, payload_len); /* copy into ws_data, decode together */
	plain_tail = p_link->ws_data.content + p_link->ws_data.used;
	for (i = 0, plain = p_link->ws_data.content + pre_datalen; plain < plain_tail; plain++, i++) {
		*plain ^= mask[i%4];
	}
	buf += payload_len;
	memmove(p_link->ws_recvq.content, buf, p_link->ws_recvq.content + p_link->ws_recvq.used - buf + 1);
	p_link->ws_recvq.used -= buf - p_link->ws_recvq.content;
	if (p_link->ws_recvq.used > 0) {
		return WS_DECODE_AGAIN;
	}
	if (!fin) { /* FIN is not set */
		if (opcode == WS_OPCODE_CONTINUATION) {
			return WS_DECODE_CHUNKED;
		} else {
			return WS_DECODE_ERROR;
		}
	}
	if (opcode == 0x01 || opcode == 0x02) {
		return WS_DECODE_COMPLETE;
	}
	if (opcode == 0x09) {
		return WS_DECODE_PING;
	}
	if (opcode == 0x0A) {
		return WS_DECODE_PONG;
	}
	return WS_DECODE_ERROR;
}

int ws_vprintf(HTTP_FD *p_link, const char *format, va_list args) {
	return tt_buffer_vprintf(&(p_link->ws_response), format, args);
}

int ws_printf(HTTP_FD *p_link, const char *format, ...) {
	int rc;
	va_list args;
	va_start(args, format);
	rc = ws_vprintf(p_link, format, args);
	va_end(args);
	return rc;
}

int ws_write(HTTP_FD *p_link, const unsigned char *content, unsigned int content_len) {
	return tt_buffer_write(&(p_link->ws_response), content, content_len);
}

int ws_pack(HTTP_FD *p_link, unsigned char opcode) {
	unsigned char ws_head[10] = {0};
	unsigned int content_len = 0;

	content_len = p_link->ws_response.used;
	memset(ws_head, 0x00, sizeof(ws_head));
	ws_head[0] = 0x80 | opcode;
	if (content_len < 126) {
		ws_head[1] = (unsigned char)content_len;
		tt_buffer_write(&(p_link->ws_sendq), ws_head, 2);
	} else if (content_len < 65536) {
		ws_head[1] = 126;
		ws_head[2] = (unsigned char)((content_len >> 8) & 0xff);
		ws_head[3] = (unsigned char)(content_len & 0xff);
		tt_buffer_write(&(p_link->ws_sendq), ws_head, 4);
	} else {
		ws_head[1] = 127;
		ws_head[6] = (unsigned char)(content_len >> 24);
		ws_head[7] = (unsigned char)((content_len >> 16) & 0xff);
		ws_head[8] = (unsigned char)((content_len >> 8) & 0xff);
		ws_head[9] = (unsigned char)(content_len & 0xff);
		tt_buffer_write(&(p_link->ws_sendq), ws_head, 10);
	}
	tt_buffer_write(&(p_link->ws_sendq), p_link->ws_response.content, content_len);
	if (p_link->ws_response.content != NULL) {
		p_link->ws_response.content[0] = '\0';
	}
	p_link->ws_response.used = 0;
	apply_change(p_link);
	return 0;
}
static int upgrade_websocket(HTTP_FD *p_link) {
	p_link->send_cb = NULL;
	p_link->state = STATE_WS_CONNECTED;
	ws_dispatch(p_link, EVENT_ONOPEN);
	return 0;
}
int ws_handshake(HTTP_FD *p_link) {
	const char *ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	const char *header_connection = NULL, *header_upgrade = NULL, *header_secwskey = NULL;
	unsigned char sha1_output[20];
	char *str_temp = NULL, *b64_str = NULL;

	header_connection = web_header_str(p_link, "Connection", NULL);
	if (header_connection != NULL && strstr(header_connection, "Upgrade") != NULL) {
		header_upgrade = web_header_str(p_link, "Upgrade", NULL);
		if (header_upgrade != NULL && 0 == strcmp(header_upgrade, "websocket")) {
			header_secwskey = web_header_str(p_link, "Sec-WebSocket-Key", NULL);
			if (header_secwskey != NULL) {
				str_temp = (char *)MY_MALLOC(strlen(header_secwskey) + strlen(ws_guid) + 1);
				if (str_temp == NULL) {
					return -1;
				}
				strcpy(str_temp, header_secwskey);
				strcpy(str_temp + strlen(str_temp), ws_guid);
				tt_sha1_bin((unsigned char *)str_temp, strlen(str_temp), sha1_output);
				MY_FREE(str_temp);
				str_temp = NULL;
				tt_base64_encode(&b64_str, (unsigned char *)sha1_output, sizeof(sha1_output));
				web_set_header(p_link, "Connection", "Upgrade");
				web_set_header(p_link, "Upgrade", "websocket");
				web_set_header(p_link, "Sec-WebSocket-Accept", b64_str);
				MY_FREE(b64_str);
				web_fin(p_link, 101);
				p_link->state = STATE_WS_HANDSHAKE;
				p_link->send_cb = upgrade_websocket;
				return 0;
			}
		}
	}
	return -1;
}

static void ws_read(HTTP_FD *p_link) {
	struct evbuffer *input = NULL;
	size_t recv_len = 0;
	E_WS_DECODE_RET decode_ret = WS_DECODE_ERROR;

	input = bufferevent_get_input(p_link->bev);
	if (input == NULL) {
		return;
	}
	recv_len = evbuffer_get_length(input);

	if (0 != tt_buffer_swapto_malloced(&(p_link->ws_recvq), recv_len)) {
		p_link->state = STATE_CLOSED;
		return;
	}
	bufferevent_read(p_link->bev, p_link->ws_recvq.content + p_link->ws_recvq.used, recv_len);
	p_link->ws_recvq.used += recv_len;
	*(p_link->ws_recvq.content + p_link->ws_recvq.used) = '\0';
	do {
		decode_ret = ws_unpack(p_link);
		if (decode_ret == WS_DECODE_COMPLETE || decode_ret == WS_DECODE_AGAIN) {
			ws_dispatch(p_link, EVENT_ONMESSAGE);
			p_link->ws_data.content[0] = '\0';
			p_link->ws_data.used = 0;
		} else if (decode_ret == WS_DECODE_CLOSED) {
			/* ws_dispatch(p_link, EVENT_ONCLOSE); EVENT_ONCLOSE will detected after ws_read returned */
			p_link->state = STATE_CLOSED;
			break;
		} else if (decode_ret == WS_DECODE_ERROR) {
			ws_dispatch(p_link, EVENT_ONERROR);
			p_link->state = STATE_CLOSED;
			break;
		} else if (decode_ret == WS_DECODE_PING) {
			ws_dispatch(p_link, EVENT_ONPING);
			p_link->ws_data.content[0] = '\0';
			p_link->ws_data.used = 0;
		} else if (decode_ret == WS_DECODE_PONG) {
			ws_dispatch(p_link, EVENT_ONPONG);
			p_link->ws_data.content[0] = '\0';
			p_link->ws_data.used = 0;
		} else if (decode_ret == WS_DECODE_CHUNKED) {
			ws_dispatch(p_link, EVENT_ONCHUNKED);
		}
	} while (decode_ret == WS_DECODE_AGAIN);
	return;
}
#endif

static void read_cb_web(struct bufferevent *bev, void *user_data) {
	HTTP_FD *p_link = (HTTP_FD *)user_data;

	p_link->tm_last_active = time(0);
	if (p_link->state == STATE_RECVING) {
		http_read(p_link);
	} 
#ifdef WITH_WEBSOCKET
	else if (p_link->state == STATE_WS_CONNECTED) {
		ws_read(p_link);
		if (p_link->state == STATE_CLOSED) {
			ws_dispatch(p_link, EVENT_ONCLOSE);
		}
	}
#endif
	apply_change(p_link);
	return;
}

static void write_cb_web(struct bufferevent *bev, void *user_data) {
	struct evbuffer *output = NULL;
	size_t sndq_len = 0;
#ifdef WITH_WEBSOCKET
	size_t write_len = 0;
#endif
	HTTP_FD *p_link = (HTTP_FD *)user_data;

	p_link->tm_last_active = time(0);
	output = bufferevent_get_output(bev);
	if (output == NULL) {
		return;
	}
	sndq_len = evbuffer_get_length(output);
	if (sndq_len != 0) {
		return;
	}
#ifdef WITH_WEBSOCKET
	write_len = p_link->sending_len;
#endif
	p_link->sending_len = 0;
	if (p_link->state == STATE_SENDING || p_link->state == STATE_CLOSING
#ifdef WITH_WEBSOCKET
		 || p_link->state == STATE_WS_HANDSHAKE
#endif
			) {
		if (p_link->send_state == SENDING_HEAD && p_link->response_entity.used != 0) {
			p_link->send_state = SENDING_ENTITY;
		} else {
			if (p_link->send_cb != NULL) {
				debug_printf("enter send_cb\n");
				p_link->send_cb(p_link);
				debug_printf("leave send_cb\n");
			} else {
				if (p_link->state == STATE_CLOSING) {
					p_link->state = STATE_CLOSED;
				} else {
					reset_link_for_continue(p_link);
				}
			}
		}
	}
#ifdef WITH_WEBSOCKET
	else if (p_link->state == STATE_WS_CONNECTED) {
		p_link->ws_sendq.used -= write_len;
		memmove(p_link->ws_sendq.content, p_link->ws_sendq.content + write_len, p_link->ws_sendq.used + 1);
	}
#endif
	apply_change(p_link);
	return;
}

static int msg_queue_check() {
	int ret = 0;
	HTTP_INNER_MSG *p_inner = NULL, *p_pre = NULL, *p_next = NULL;
	HTTP_CALLBACK_RESPONSE *response = NULL;

	p_pre = NULL;
	p_next = NULL;
	/* clear msg that msg.msgq_ref == 0 */
	for (p_inner = g_web_inner_msg_head; p_inner != NULL; p_inner = p_next) {
		p_next = p_inner->next;
		if (p_inner->ref_cnt == 0) {
			if (p_inner->resp_msgq != NULL) {
				msgq_destroy(p_inner->resp_msgq);
				MY_FREE(p_inner->resp_msgq);
			}
			if (p_inner->name != NULL) {
				MY_FREE(p_inner->name);
			}
			if (p_inner->payload != NULL) {
				MY_FREE(p_inner->payload);
			}
			MY_FREE(p_inner);
			if (p_pre != NULL) {
				p_pre->next = p_next;
			} else {
				g_web_inner_msg_head = p_next;
			}
		} else {
			p_pre = p_inner;
		}
	}
	while (1) {
		p_inner = (HTTP_INNER_MSG *)msg_tryget(&g_web_inner_msg);
		if (p_inner == NULL) {
			break;
		}
		ret = msg_dispatch(p_inner->name, p_inner->payload, p_inner->payload_len);
		if (p_inner->is_sync) {
			response = (HTTP_CALLBACK_RESPONSE *)MY_MALLOC(sizeof(HTTP_CALLBACK_RESPONSE));
			if (response == NULL) {
				emergency_printf("malloc failed.\n");
			} else {
				response->ret = ret;
				msg_put(p_inner->resp_msgq, response);
			}
		} else {
			if (p_inner->callback) {
				p_inner->callback(ret, p_inner->arg);
			}
		}
		p_inner->next = NULL;
		if (g_web_inner_msg_head == NULL) {
			g_web_inner_msg_head = p_inner;
		} else {
			for (p_pre = g_web_inner_msg_head; p_pre->next != NULL; p_pre = p_pre->next);
			p_pre->next = p_inner;
		}
	}
	return 0;
}

int notify_web_getvalue(long int *len) {
	return msg_getvalue(&g_web_inner_msg, len);
}
static int inner_call(const char *name, void *payload, size_t payload_len, int is_sync, int (*callback)(int ret, void *arg), void *arg) {
	int ret = 0;
	struct timespec tmout;
	HTTP_INNER_MSG *p_inner = NULL;
	HTTP_CALLBACK_RESPONSE *response = NULL;

	p_inner = (HTTP_INNER_MSG *)MY_MALLOC(sizeof(HTTP_INNER_MSG));
	if (p_inner == NULL) {
		emergency_printf("malloc for p_inner failed.");
		ret = -1;
		goto exit;
	}
	memset(p_inner, 0x00, sizeof(HTTP_INNER_MSG));
	p_inner->is_sync = is_sync;
	if (is_sync) {
		p_inner->ref_cnt = 1;
	} else {
		p_inner->callback = callback;
		p_inner->arg = arg;
		p_inner->ref_cnt = 0;
	}
	if (name != NULL) {
		p_inner->name = (char *)MY_MALLOC(strlen(name) + 1);
		if (p_inner->name == NULL) {
			emergency_printf("malloc for p_inner->name failed.");
			ret = -1;
			goto exit;
		}
		strcpy(p_inner->name, name);
	} else {
		p_inner->name = NULL;
	}
	p_inner->payload = payload;
	p_inner->payload_len = payload_len;
	if (is_sync) {
		p_inner->resp_msgq = (MSG_Q *)MY_MALLOC(sizeof(MSG_Q));
		if (p_inner->resp_msgq == NULL) {
			ret = 0;
			goto exit;
		}
		msgq_init(p_inner->resp_msgq, 0);
	}
	if (0 != msg_put(&g_web_inner_msg, p_inner)) {
		emergency_printf("msg_put failed.\n");
		ret = -1;
		goto exit;
	}
	write(g_msg_fd[1], "0", 1);
	if (is_sync) {
		tmout.tv_sec = 1; /* timeout in 1 second */
		tmout.tv_nsec = 0;
		response = (HTTP_CALLBACK_RESPONSE *)msg_timedget(p_inner->resp_msgq, &tmout);
		if (response == NULL) {
			printf("msg_timedget timeout, webserver is busy or dead.\n");
			ret = -1;
			goto exit;
		}
		ret = response->ret;
		MY_FREE(response);
	}
exit:
	if (p_inner != NULL) {
		if (is_sync) {
			p_inner->ref_cnt = 0;
		}
	}
	return ret;
}
/* block until msg_dispatch complete or timeout, return callback return value or -1 if timeout */
int sync_call(const char *name, void *payload, size_t payload_len) {
	return inner_call(name, payload, payload_len, 1, NULL, NULL);
}
/* return immediately, callback will be called when msg_dispatch complete */
int notify_web(const char *name, void *payload, size_t payload_len, int (*callback)(int ret, void *arg), void *arg) {
	return inner_call(name, payload, payload_len, 0, callback, arg);
}
void msg_cb_web(evutil_socket_t fd, short event, void *user_data) {
	unsigned char tmp = 0;
	while (read(fd, &tmp, 1) > 0);
	msg_queue_check();
}

void listen_cb_web(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen, void *user_data) {
	int ret = -1;
	WEB_SERVER *server = (WEB_SERVER *)user_data;
	HTTP_FD *new_link = NULL;
#ifdef WITH_IPV6
	struct sockaddr_in6 local_addr6;
#endif
	struct sockaddr_in local_addr;
	socklen_t addr_len;
	int tcp_nodelay = 1;

	new_link = (HTTP_FD *)MY_MALLOC(sizeof(HTTP_FD));
	if (new_link == NULL) {
		emergency_printf("malloc failed!\n");
		goto exit;
	}
	memset(new_link, 0x00, sizeof(HTTP_FD));
#ifdef WITH_IPV6
	if (server->ip_version == 6) {
		addr_len = sizeof(struct sockaddr_in6);
		getsockname(fd, (struct sockaddr *)&local_addr6, &addr_len);
		inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)addr)->sin6_addr), new_link->ip_peer, sizeof(new_link->ip_peer));
		new_link->port_peer = ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
		inet_ntop(AF_INET6, &(local_addr6.sin6_addr), new_link->ip_local, sizeof(new_link->ip_local));
		new_link->port_local = ntohs(local_addr6.sin6_port);
	} else
#endif
	{
		addr_len = sizeof(struct sockaddr_in);
		getsockname(fd, (struct sockaddr *)&local_addr, &addr_len);
		inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr), new_link->ip_peer, sizeof(new_link->ip_peer));
		new_link->port_peer = ntohs(((struct sockaddr_in *)addr)->sin_port);
		inet_ntop(AF_INET, &(local_addr.sin_addr), new_link->ip_local, sizeof(new_link->ip_local));
		new_link->port_local = ntohs(local_addr.sin_port);
	}
	new_link->server = server;
	new_link->state = STATE_RECVING;
	new_link->tm_last_active = new_link->tm_last_req = time(0);
	tt_buffer_init(&(new_link->response_head));
	tt_buffer_init(&(new_link->response_entity));
#ifdef WITH_WEBSOCKET
	tt_buffer_init(&(new_link->ws_recvq));
	tt_buffer_init(&(new_link->ws_sendq));
	tt_buffer_init(&(new_link->ws_data));
	tt_buffer_init(&(new_link->ws_response));
#endif
#ifdef WITH_SSL
	if (server->is_ssl) {
		new_link->ssl = SSL_new(server->ssl_ctx);
		if (new_link->ssl == NULL) {
			emergency_printf("SSL_new failed!\n");
			goto exit;	
		}
		new_link->bev = bufferevent_openssl_socket_new(g_event_base, fd, new_link->ssl, BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
	} else
#endif
	{
		new_link->bev = bufferevent_socket_new(g_event_base, fd, BEV_OPT_CLOSE_ON_FREE);
	}
	if (new_link->bev == NULL) {
		emergency_printf("create bufferevent failed.\n");
		return;
	}

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&tcp_nodelay, sizeof(tcp_nodelay)) < 0) {
		emergency_printf("setsockopt failed.\n");
		return;
	}
	bufferevent_setcb(new_link->bev, read_cb_web, write_cb_web, event_cb_web, new_link);
	bufferevent_enable(new_link->bev, EV_READ);
	bufferevent_disable(new_link->bev, EV_WRITE);
	debug_printf("link from [%s]:%d.\n", new_link->ip_peer, new_link->port_peer);

	new_link->prev = NULL;
	new_link->next = g_http_links;
	if (g_http_links) {
		g_http_links->prev = new_link;
	}
	g_http_links = new_link;
	ret = 0;
exit:
	if (ret != 0) {
		if (new_link != NULL) {
			free_link_all(new_link);
		}
	}
	return;
}
static void timer_cb_web(evutil_socket_t fd, short event, void *user_data) {
	HTTP_FD *p_curlink = NULL, *p_next = NULL;
	time_t tm_now, tm_last_active, tm_last_req;

	tm_now = time(0);
	// debug_printf("now: %ld\n", tm_now);
	tm_last_active = (tm_now > g_max_active_interval) ? (tm_now - g_max_active_interval) : 0;
	tm_last_req = (tm_now > g_recv_timeout) ? (tm_now - g_recv_timeout) : 0;
	for (p_curlink = g_http_links; p_curlink != NULL; p_curlink = p_next) {
		p_next = p_curlink->next;
		if ((g_max_active_interval && p_curlink->tm_last_active < tm_last_active) \
			 || (g_recv_timeout && p_curlink->tm_last_req < tm_last_req)) {
			p_curlink->tm_last_active = tm_now;
			if (p_curlink->recvbuf_len == 0) {
				p_curlink->state = STATE_CLOSED;
			} else {
				p_curlink->state = STATE_CLOSING;
				web_fin(p_curlink, 408);
			}
			apply_change(p_curlink);
		}
	}
	session_timeout_check();
}
int destroy_server(WEB_SERVER *p_svr) {
	if (p_svr->listener != NULL) {
		evconnlistener_free(p_svr->listener);
	}
	if (p_svr->name != NULL) {
		MY_FREE(p_svr->name);
	}
	if (p_svr->root != NULL) {
		MY_FREE(p_svr->root);
	}
	MY_FREE(p_svr);
	return 0;
}
int destroy_server_by_id(int svr_id) {
	int ret = -1;
	WEB_SERVER *p_cursvr = NULL, *p_next = NULL;

	for (p_cursvr = g_servers; p_cursvr != NULL; p_cursvr = p_next) {
		p_next = p_cursvr->next;
		if (p_cursvr->id == svr_id) {
			if (g_servers == p_cursvr) {
				g_servers = p_cursvr->next;
			}
			if (p_cursvr->next) {
				p_cursvr->next->prev = p_cursvr->prev;
			}
			if (p_cursvr->prev) {
				p_cursvr->prev->next = p_cursvr->next;
			}
			destroy_server(p_cursvr);
			ret = 0;
			break;
		}
	}
	return ret;
}
static WEB_SERVER *create_server(const char *name, int ip_version, unsigned short port, const char *root, void (* listen_cb)(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *cli_addr, int socklen, void *user_data), void *ssl_ctx) {
	int ret = -1;
	struct sockaddr_in svr_addr;
#ifdef WITH_IPV6
	struct sockaddr_in6 svr_addr6;
#endif
	static int svr_id = 0;
	WEB_SERVER *new_server = NULL;

	if (g_event_base == NULL) {
		goto exit;
	}
	new_server = (WEB_SERVER *)MY_MALLOC(sizeof(WEB_SERVER));
	if (new_server == NULL) {
		goto exit;
	}
	memset(new_server, 0x00, sizeof(WEB_SERVER));
	new_server->ip_version = ip_version;
	new_server->port = port;
	if (root != NULL) {
		new_server->root = (char *)MY_MALLOC(strlen(root) + 1);
		if (new_server->root == NULL) {
			goto exit;
		}
		strcpy(new_server->root, root);
	}
	if (name != NULL) {
		new_server->name = (char *)MY_MALLOC(strlen(name) + 1);
		if (new_server->name == NULL) {
			goto exit;
		}
		strcpy(new_server->name, name);
	}
#ifdef WITH_SSL
	if (ssl_ctx != NULL) {
		new_server->ssl_ctx = (SSL_CTX *)ssl_ctx;
		new_server->is_ssl = 1;
	} else {
		new_server->is_ssl = 0;
	}
#endif
#ifdef WITH_IPV6
	if (ip_version == 6) {
		memset(&svr_addr6, 0x00, sizeof(svr_addr6));
		svr_addr6.sin6_family = AF_INET6;
		svr_addr6.sin6_port = htons(port);
		new_server->listener = evconnlistener_new_bind(g_event_base, listen_cb, (void *)new_server, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_BIND_IPV6ONLY, -1, (struct sockaddr *)&svr_addr6, sizeof(svr_addr6));
	} else
#endif
	{
		memset(&svr_addr, 0x00, sizeof(svr_addr));
		svr_addr.sin_family = AF_INET;
		svr_addr.sin_port = htons(port);
		new_server->listener = evconnlistener_new_bind(g_event_base, listen_cb, (void *)new_server, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr *)&svr_addr, sizeof(svr_addr));
	}
	if (new_server->listener == NULL) {
		emergency_printf("ipv%d: bind port %u failed.\n", ip_version, port);
		goto exit;
	} else {
		notice_printf("ipv%d: bind port %u success.\n", ip_version, port);
	}
	new_server->prev = NULL;
	new_server->next = g_servers;
	if (g_servers) {
		g_servers->prev = new_server;
	}
	g_servers = new_server;
	ret = 0;
exit:
	if (ret != 0) {
		if (new_server != NULL) {
			if (new_server->listener != NULL) {
				evconnlistener_free(new_server->listener);
			}
			if (new_server->root != NULL) {
				MY_FREE(new_server->root);
			}
			MY_FREE(new_server);
			new_server = NULL;
		}
	} else {
		new_server->id = ++svr_id;
	}
	return new_server;
}
WEB_SERVER *create_http(const char *name, int ip_version, unsigned short port, const char *root) {
	return create_server(name, ip_version, port, root, listen_cb_web, NULL);
}
#ifdef WITH_SSL
WEB_SERVER *create_https(const char *name, int ip_version, unsigned short port, const char *root, SSL_CTX *ssl_ctx) {
	return create_server(name, ip_version, port, root, listen_cb_web, ssl_ctx);
}
#endif
int web_server_run() {
	struct event msg_ev;
	struct event time_ev;
	struct timeval tv;

	g_event_base = event_base_new();
	if (g_event_base == NULL) {
		emergency_printf("event_base_new failed.\n");
		return -1;
	}
	event_assign(&time_ev, g_event_base, -1, EV_PERSIST, timer_cb_web, NULL);
	evutil_timerclear(&tv);
	tv.tv_sec = 1;
	event_add(&time_ev, &tv);

#ifdef _WIN32
#define LOCAL_SOCKETPAIR_AF AF_INET
#else
#define LOCAL_SOCKETPAIR_AF AF_UNIX
#endif
	msgq_init(&g_web_inner_msg, 0);
	if (evutil_socketpair(LOCAL_SOCKETPAIR_AF, SOCK_STREAM, 0, g_msg_fd) != 0) {
		printf("evutil_socketpair failed.\n");
		goto exit;
	}
	evutil_make_socket_nonblocking(g_msg_fd[0]);
	evutil_make_socket_nonblocking(g_msg_fd[1]);
	event_assign(&msg_ev, g_event_base, g_msg_fd[0], EV_READ | EV_PERSIST, msg_cb_web, NULL);
	if (event_add(&msg_ev, NULL) < 0) {
		printf("event_add failed.\n");
		goto exit;
	}

	create_http("default", 4, 20080, ".");
#ifdef WITH_IPV6
	create_http("default", 6, 20080, NULL);
#endif
#ifdef WITH_SSL
	g_default_ssl_ctx = create_ssl_ctx(g_default_svr_cert, g_default_privkey, NULL);
	create_https("default", 4, 20443, NULL, g_default_ssl_ctx);
#ifdef WITH_IPV6
	create_https("default", 6, 20443, NULL, g_default_ssl_ctx);
#endif
#endif /* WITH_SSL */
	event_base_dispatch(g_event_base);
	if (g_event_base) {
		event_base_free(g_event_base);
	}
exit:
#ifdef WITH_SSL
	SSL_CTX_free(g_default_ssl_ctx);
	g_default_ssl_ctx = NULL;
#endif
	return 0;
}
int init_webserver() {
	FILE *fin = NULL;
	unsigned char *fcontent = NULL;
	unsigned long fsize = 0;

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0201, &wsa_data);
#endif
	fin = fopen("package.bin", "rb");
	if (fin == NULL) {
		emergency_printf("open package.bin failed.\n");
		return -1;
	}
	fseek(fin, 0, SEEK_END);
	fsize = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	fcontent = (unsigned char *)MY_MALLOC(fsize);
	if (fcontent == NULL) {
		emergency_printf("malloc failed!\n");
	}
	fread(fcontent, fsize, 1, fin);
	fclose(fin);
	if (-1 == init_tree_info(fcontent, fsize)) {
		return -1; 
	}
	tt_handler_register();

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	return 0;
}

void *web_server_thread(void *para) {
	web_server_run(); // run web server
	pthread_exit(NULL);
	return NULL;
}

