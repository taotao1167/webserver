#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef __linux__
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include <execinfo.h>
#endif
#ifndef __TT_WEB_H__
#include "tt_web.h"
#endif

#define WORKDIR ""
#define PIDFILE WORKDIR"webserver.pid"

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

pthread_t tWeb = 0;

int init_main() {
#ifdef WATCH_RAM
	if (0 != init_malloc_debug()) {
		return -1;
	}
#endif
	if (0 != init_webserver()) { /* web server init */
		return -1;
	}
	return 0;
}

#ifndef _WIN32
int start_daemon() {
	int ret = 0;
	int fd = -1;
	char pid[20] = {0};
	struct flock fl;

	fd = open(PIDFILE, O_RDWR | O_CREAT, 0600);
	if (fd < 0) {
		printf("open PIDFILE: %s.\n", strerror(errno));
		return -1;
	}
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	ret = fcntl(fd, F_GETLK, &fl);
	if (ret < 0) {
		printf("get lock info: %s.\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (fl.l_type != F_UNLCK) {
		printf("already runnning, pid is %d.\n", fl.l_pid);
		return -1;
	}
	printf("start process success.\n");
	if (daemon(1, 0) == -1) {
		printf("execute daemon failed.\n");
		return -1;
	}
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	ret = fcntl(fd, F_SETLK, &fl);
	if (ret < 0) {
		printf("sef lock failed.\n");
		close(fd);
		return -1;
	}
	printf("execute daemon success.\n");
	sprintf(pid, "%d", getpid());
	ret = write(fd, pid, strlen(pid));
	return 0;
}

int stop_daemon() {
	int ret = 0;
	int fd = -1;
	struct flock fl;

	fd = open(PIDFILE, O_RDONLY, 0600);
	if (fd < 0) {
		printf("open PIDFILE: %s.\n", strerror(errno));
		return -1;
	}
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	ret = fcntl(fd, F_GETLK, &fl);
	if (ret < 0) {
		printf("get lock info: %s.\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (fl.l_type == F_UNLCK) {
		printf("process not running.\n");
		return -1;
	}
	close(fd);
	ret = kill(fl.l_pid, SIGINT);
	if (ret == -1) {
		printf("kill failed: %s.\n", strerror(errno));
		return -1;
	} else {
		remove(PIDFILE);
		printf("stop process success.\n");
	}
	return 0;
}
#endif /* _WIN32 */

#define TRACE_SIZE 64
static void stackinfo(int signo) {
    int nptrs = 0;
    void *buffer[TRACE_SIZE] = {NULL};
    char fname[128] = {0};
    int fd = 0;
    const char *sig = NULL;

    switch (signo) {
        case SIGSEGV: sig = "SIGSEGV"; break;
        case SIGABRT: sig = "SIGABRT"; break;
        case SIGPIPE: sig = "SIGPIPE"; break;
        default: sig = "UNKNOWN";
    }
    sprintf(fname, "%ld_%s.crash", time(NULL), sig);
    fd = open(fname, O_WRONLY | O_CREAT, 0664);
    nptrs = backtrace(buffer, TRACE_SIZE);
    if (fd != -1) {
        backtrace_symbols_fd(buffer, nptrs, fd);
        close(fd);
    } else {
        backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);
    }
}
static void handler(int signo) {
    stackinfo(signo);
    signal(signo, SIG_DFL);
    raise(signo);
}
int main(int argc, char **argv) {
	int ret = 0;
#ifndef _WIN32
	if (argc < 2) {
		printf("usage: <program> start|stop\n");
		return -1;
	}
#endif
#if 1
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);
    signal(SIGFPE, handler);
#endif

	if (0 != init_main()) {
		return -1;
	}
#ifndef _WIN32
	if (0 == strcmp("start", argv[1])) {
		if (0 != start_daemon()) {
			return -1;
		}
	} else if (0 == strcmp("debug", argv[1])) {
		/* do nothing */
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
	// while (1) {
	// 	sleep(1);
	// 	ret = sync_call("test", "hello, notify_web test.", strlen("hello, notify_web test."));
	// 	printf("sync_call ret %d.\n", ret);
	// }
	pthread_join(tWeb, NULL);
	return 0;
}

