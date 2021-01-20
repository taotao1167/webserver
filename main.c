#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#ifndef __TT_WEB_H__
#include "tt_web.h"
#endif

#define WORKDIR ""
#define PIDFILE WORKDIR"webserver.pid"

#ifdef WATCH_RAM
#include "tt_malloc_debug.h"
#define MY_MALLOC(x) my_malloc((x), __func__, __LINE__)
#define MY_FREE(x) my_free((x), __func__, __LINE__)
#define MY_REALLOC(x, y) my_realloc((x), (y), __func__, __LINE__)
#else
#define MY_MALLOC(x) malloc((x))
#define MY_FREE(x) free((x))
#define MY_REALLOC(x, y) realloc((x), (y))
#endif

pthread_t tWeb = 0;

/* 初始化动作：各个线程的初始化动作(TODO) */
int init_main() {
	if (0 != init_webserver()) { /* web服务器初始化 */
		return -1;
	}
	return 0;
}

#ifndef _WIN32
int start_daemon() {
	int ret = 0;
	FILE *fp = NULL;
	char pid[20] = {0};
	fp = fopen(PIDFILE, "r");
	if (fp != NULL) { /* 发现存在pid文件 */
		fread(pid, sizeof(pid) - 1, 1, fp);
		fclose(fp);
		ret = kill((pid_t)atoi(pid), 0); /* 用kill函数发送信号0以检查进程是否存在，不是要结束进程 */
		if (ret == -1) {
			if (EPERM == errno) { /* 没有权限执行kill */
				printf("check process failed. Permission denied.\n");
				return -1;
			} else if (ESRCH == errno) { /* 进程不存在 */
				remove(PIDFILE);
			}
		} else { /* kill返回0说明进程还在正常运行 */
			printf("already running.\n");
			return -1;
		}
	}
	printf("start process success.\n");
	if (daemon(1, 0) == -1) { /* 执行daemon函数后程序就会切到后台运行，TODO */
		printf("execute daemon failed.\n");
		return -1;
	}
	printf("execute daemon success.\n");
	fp = fopen(PIDFILE, "w");
	if (fp == NULL) {	/* pid文件没有写入权限 */
		printf("open PIDFILE failed. Permission denied.\n");
		return -1;
	}
	sprintf(pid, "%d", getpid());
	fwrite(pid, strlen(pid), 1, fp);
	fclose(fp);
	return 0;
}

int stop_daemon() {
	int ret = 0;
	FILE *fp = NULL;
	char pid[20] = {0};
	fp = fopen(PIDFILE, "r");
	if (fp == NULL) {
		printf("PIDFILE not found.\n");
		return -1;
	}
	fread(pid, sizeof(pid) - 1, 1, fp);
	ret = kill((pid_t)atoi(pid), SIGQUIT);
	if (ret == -1) {
		if (EPERM == errno) {
			printf("kill failed. Permission denied.\n");
		} else if (ESRCH == errno) {
			printf("process not running.\n");
		}
		return -1;
	} else {
		remove(PIDFILE);
		printf("stop process success.\n");
	}
	return 0;
}
#endif /* _WIN32 */

int main(int argc, char **argv) {
	int ret = 0;
#ifndef _WIN32
	if (argc < 2) {
		printf("usage: <program> start|stop\n");
		return -1;
	}
#endif
	if (0 != init_main()) { /* 初始化 */
		return -1;
	}
#ifndef _WIN32
	if (0 == strcmp("start", argv[1])) {
		if (0 != start_daemon()) {/* 将程序切换到后台运行 */
			return -1;
		}
	} else if (0 == strcmp("debug", argv[1])) {
		/* do nothing, 继续运行接下来的代码 */
	} else if (0 == strcmp("stop", argv[1])) {
		return stop_daemon();
	} else {
		printf("usage: <program> start|stop\n");
		return -1;
	}
#endif
	ret = pthread_create(&tWeb, NULL, web_server_thread, NULL);
	if (ret != 0) {
		printf("tWeb Create failed.\n");
		return -1;
	}
	pthread_join(tWeb, NULL);
	return 0;
}

