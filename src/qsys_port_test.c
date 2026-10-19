#include "./../include/ttypt/qsys.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <direct.h>
#define getpid _getpid
#define unlink _unlink
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

int
main(void)
{
	/* 1. qsys_realpath */
	char resolved[1024];
	char *rp = qsys_realpath(".", resolved);
	assert(rp != NULL);
	assert(strlen(resolved) > 0);
	printf("ok: qsys_realpath -> %s\n", resolved);

	/* 2. qsys_mkdir */
	const char *test_dir = "./tmp_qsys_test_dir";
	rmdir(test_dir);
	int rc = qsys_mkdir(test_dir);
	assert(rc == 0);
	rc = rmdir(test_dir);
	assert(rc == 0);
	printf("ok: qsys_mkdir\n");

	/* 3. qsys_dprintf, qsys_flock, qsys_ftruncate */
	const char *test_file = "./tmp_qsys_test_file.txt";
	int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
	assert(fd >= 0);

	rc = qsys_flock(fd, QSYS_LOCK_EX);
	assert(rc == 0);

	int written = qsys_dprintf(fd, "hello %s %d\n", "qsys", 42);
	assert(written > 0);

	rc = qsys_ftruncate(fd, 5);
	assert(rc == 0);

	rc = qsys_flock(fd, QSYS_LOCK_UN);
	assert(rc == 0);

	close(fd);
	unlink(test_file);
	printf("ok: qsys_dprintf, qsys_flock, qsys_ftruncate\n");

	/* 4. qsys_now_ms, qsys_sleep_ms */
	uint64_t t0 = qsys_now_ms();
	qsys_sleep_ms(20);
	uint64_t t1 = qsys_now_ms();
	assert(t1 >= t0);
	printf("ok: qsys_now_ms, qsys_sleep_ms (elapsed %llu ms)\n", (unsigned long long)(t1 - t0));

	/* 5. qsys_pid_alive, qsys_self_rss_bytes */
	assert(qsys_pid_alive(getpid()) == 0);
	assert(qsys_pid_alive(999999) == -1);
	uint64_t rss = qsys_self_rss_bytes();
	assert(rss > 0);
	printf("ok: qsys_pid_alive, qsys_self_rss_bytes (%llu bytes)\n", (unsigned long long)rss);

	/* 6. qsys_sock_* */
	assert(qsys_sock_init() == 0);
	qsys_socket_t s = qsys_sock_create(AF_INET, SOCK_STREAM, 0);
	assert(s >= 0);
	assert(qsys_sock_set_nonblock(s, 1) == 0);

	/* 7. qsys_poll */
	struct pollfd pfd;
	pfd.fd = s;
	pfd.events = POLLOUT;
	pfd.revents = 0;
	int poll_rc = qsys_poll(&pfd, 1, 0);
	assert(poll_rc >= 0);

	assert(qsys_sock_close(s) == 0);
	printf("ok: qsys_sock_*, qsys_poll\n");

	/* 8. qsys_ignore_pipe */
	qsys_ignore_pipe();
	printf("ok: qsys_ignore_pipe\n");

	printf("qsys_port_test: all tests passed\n");
	return 0;
}
