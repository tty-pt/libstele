#include "./../include/ttypt/stele.h"
#include "./../include/ttypt/stele_wasm.h"

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

static int g_event_hits = 0;
static char g_last_payload[64];

static void
on_test_event(const char *topic, const void *payload, size_t len, void *user_data)
{
	(void)topic;
	int *counter = (int *)user_data;
	if (counter) (*counter)++;
	g_event_hits++;
	if (payload && len > 0 && len < sizeof(g_last_payload)) {
		memcpy(g_last_payload, payload, len);
		g_last_payload[len] = '\0';
	}
}

static _Atomic uint32_t g_wq_sum = 0;

typedef struct {
	stl_wq_task_t task;
	uint32_t      val;
} test_wq_job_t;

static void
on_wq_job(void *arg)
{
	test_wq_job_t *job = (test_wq_job_t *)arg;
	atomic_fetch_add_explicit(&g_wq_sum, job->val, memory_order_relaxed);
}

int
main(void)
{
	/* 1. stele_realpath */
	char resolved[1024];
	char *rp = stele_realpath(".", resolved);
	assert(rp != NULL);
	assert(strlen(resolved) > 0);
	printf("ok: stele_realpath -> %s\n", resolved);

	/* 2. stele_mkdir */
	const char *test_dir = "./tmp_stele_test_dir";
	rmdir(test_dir);
	int rc = stele_mkdir(test_dir);
	assert(rc == 0);
	rc = rmdir(test_dir);
	assert(rc == 0);
	printf("ok: stele_mkdir\n");

	/* 3. stele_dprintf, stele_flock, stele_ftruncate */
	const char *test_file = "./tmp_stele_test_file.txt";
	int fd = open(test_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
	assert(fd >= 0);

	rc = stele_flock(fd, STL_LOCK_EX);
	assert(rc == 0);

	int written = stele_dprintf(fd, "hello %s %d\n", "stele", 42);
	assert(written > 0);

	rc = stele_ftruncate(fd, 5);
	assert(rc == 0);

	close(fd);
	unlink(test_file);
	printf("ok: stele_dprintf / stele_flock / stele_ftruncate\n");

	/* 4. stele_now_ms, stele_sleep_ms */
	uint64_t t0 = stele_now_ms();
	assert(t0 > 0);
	stele_sleep_ms(10);
	uint64_t t1 = stele_now_ms();
	assert(t1 >= t0);
	printf("ok: stele_now_ms / stele_sleep_ms\n");

	/* 5. stele_pid_alive */
	assert(stele_pid_alive(getpid()) == 0);
	assert(stele_pid_alive(999999) == -1);
	printf("ok: stele_pid_alive\n");

	/* 6. stele_self_rss_bytes */
	uint64_t rss = stele_self_rss_bytes();
	assert(rss > 0);
	printf("ok: stele_self_rss_bytes -> %llu\n", (unsigned long long)rss);

	/* 7. stl_event_on / stl_event_emit / stl_event_off */
	int user_counter = 0;
	assert(stl_event_on("test:ping", on_test_event, &user_counter) == 0);
	assert(stl_event_emit("test:ping", "pong", 4) == 0);
	assert(g_event_hits == 1 && user_counter == 1);
	assert(strcmp(g_last_payload, "pong") == 0);
	assert(stl_event_off("test:ping", on_test_event) == 1);
	assert(stl_event_emit("test:ping", "after", 5) == 0);
	assert(g_event_hits == 1);
	printf("ok: stl_event_on / stl_event_emit / stl_event_off\n");

	/* 8. stl_wq multi-threaded MPMC ring + drain/wait/step */
	assert(stl_cpu_count() >= 1u);
	stl_wq_t *wq = stl_wq_create("test_wq", 2, 64);
	assert(wq != NULL);
	test_wq_job_t jobs[128];
	uint32_t expected_sum = 0;
	atomic_store_explicit(&g_wq_sum, 0u, memory_order_relaxed);
	for (uint32_t i = 0; i < 128; i++) {
		stl_wq_task_init(&jobs[i].task);
		jobs[i].val = i + 1u;
		expected_sum += jobs[i].val;
		assert(stl_wq_submit(wq, &jobs[i].task, on_wq_job, &jobs[i]) >= 0);
	}
	stl_wq_wait(wq, &jobs[127].task);
	stl_wq_drain(wq);
	for (uint32_t i = 0; i < 128; i++) {
		assert(stl_wq_done(&jobs[i].task));
	}
	assert(atomic_load_explicit(&g_wq_sum, memory_order_acquire) == expected_sum);
	stl_wq_destroy(wq);
	printf("ok: stl_wq multi-threaded MPMC ring (%u jobs -> sum=%u)\n", 128u, expected_sum);

	/* 9. stl_username / stl_home_dir / stl_temp_dir / stl_config_dir / stl_data_dir / stl_exe_dir / stl_so_ext / stl_mkdir_p */
	char ubuf[128], hbuf[512], tbuf[512], cbuf[512], dbuf[512], ebuf[512];
	assert(stl_username(ubuf, sizeof(ubuf)) == ubuf && ubuf[0] != '\0');
	assert(stl_home_dir(hbuf, sizeof(hbuf)) == hbuf && hbuf[0] != '\0');
	assert(stl_temp_dir(tbuf, sizeof(tbuf)) == tbuf && tbuf[0] != '\0');
	assert(stl_config_dir("stele-test", cbuf, sizeof(cbuf)) == cbuf && strstr(cbuf, "stele-test") != NULL);
	assert(stl_data_dir("stele-test", dbuf, sizeof(dbuf)) == dbuf && strstr(dbuf, "stele-test") != NULL);
	assert(stl_exe_dir(ebuf, sizeof(ebuf)) == ebuf && ebuf[0] != '\0');
	assert(stl_so_ext() != NULL && (strcmp(stl_so_ext(), "so") == 0 || strcmp(stl_so_ext(), "dll") == 0));
	char mkdir_root[512], mkdir_mid[560], mkdir_nested[600];
	snprintf(mkdir_root, sizeof(mkdir_root), "%.*s/stele-mkdir-test-%d", 400, tbuf, (int)getpid());
	snprintf(mkdir_mid, sizeof(mkdir_mid), "%s/a", mkdir_root);
	snprintf(mkdir_nested, sizeof(mkdir_nested), "%s/b", mkdir_mid);
	assert(stl_mkdir_p(mkdir_nested) == 0);
	rmdir(mkdir_nested);
	rmdir(mkdir_mid);
	rmdir(mkdir_root);
	printf("ok: path/user helpers (user=%s exe=%s ext=%s)\n", ubuf, ebuf, stl_so_ext());

	/* 10. stl_wake_t poll/signal/drain */
	stl_wake_t wake;
	assert(stl_wake_init(&wake) == 0);
	assert(stl_wake_fd(&wake) != STL_INVALID_SOCKET);
	struct pollfd wpfd = { .fd = stl_wake_fd(&wake), .events = POLLIN, .revents = 0 };
	assert(stl_poll(&wpfd, 1, 0) == 0);
	stl_wake_signal(&wake);
	wpfd.revents = 0;
	assert(stl_poll(&wpfd, 1, 100) == 1 && (wpfd.revents & POLLIN));
	stl_wake_drain(&wake);
	wpfd.revents = 0;
	assert(stl_poll(&wpfd, 1, 0) == 0);
	stl_wake_close(&wake);
	printf("ok: stl_wake_t\n");

	/* 11. stl_ipc_listen / connect / accept / timeout (TCP + logical name) */
	stl_ipc_listener_t tcp_l;
	assert(stl_ipc_listen("tcp://127.0.0.1:0", 4, &tcp_l) == 0);
	assert(tcp_l.port > 0);
	char tcp_ep[64];
	snprintf(tcp_ep, sizeof(tcp_ep), "tcp://127.0.0.1:%u", (unsigned)tcp_l.port);
	stl_socket_t cli = stl_ipc_connect(tcp_ep);
	assert(cli != STL_INVALID_SOCKET);
	stl_socket_t srv = stl_ipc_accept(&tcp_l);
	assert(srv != STL_INVALID_SOCKET);
	assert(stl_sock_set_timeout(cli, 50, 50) == 0);
	assert(stl_sock_send(cli, "ping", 4, 0) == 4);
	char ibuf[16] = {0};
	assert(stl_sock_recv(srv, ibuf, sizeof(ibuf), 0) == 4);
	assert(strcmp(ibuf, "ping") == 0);
	stl_sock_close(srv);
	stl_sock_close(cli);
	stl_ipc_close_listener(&tcp_l);

	char svc_name[64];
	snprintf(svc_name, sizeof(svc_name), "stele-test-%d", (int)getpid());
	stl_ipc_listener_t svc_l;
	assert(stl_ipc_listen(svc_name, 4, &svc_l) == 0);
	cli = stl_ipc_connect(svc_name);
	assert(cli != STL_INVALID_SOCKET);
	srv = stl_ipc_accept(&svc_l);
	assert(srv != STL_INVALID_SOCKET);
	assert(stl_sock_send(srv, "ok", 2, 0) == 2);
	memset(ibuf, 0, sizeof(ibuf));
	assert(stl_sock_recv(cli, ibuf, sizeof(ibuf), 0) == 2);
	assert(strcmp(ibuf, "ok") == 0);
	stl_sock_close(srv);
	stl_sock_close(cli);
	stl_ipc_close_listener(&svc_l);
	printf("ok: stl_ipc_listen / connect / accept\n");

	/* 12. stl_spawn + stl_proc_wait + stl_proc_kill + stl_spawn_detached */
#ifdef _WIN32
	const char *argv_echo[] = { "cmd.exe", "/c", "echo stele-proc-ok", NULL };
	const char *argv_cat[] = { "cmd.exe", "/c", "more", NULL };
	const char *argv_sleep[] = { "cmd.exe", "/c", "ping -n 5 127.0.0.1 >NUL", NULL };
	const char *argv_det[] = { "cmd.exe", "/c", "exit 0", NULL };
#else
	const char *argv_echo[] = { "/bin/sh", "-c", "echo stele-proc-ok; echo err >&2", NULL };
	const char *argv_cat[] = { "/bin/cat", NULL };
	const char *argv_sleep[] = { "/bin/sh", "-c", "sleep 5", NULL };
	const char *argv_det[] = { "/bin/true", NULL };
#endif
	stl_proc_t p;
	assert(stl_spawn(argv_echo, STL_SPAWN_STDOUT | STL_SPAWN_MERGE_STDERR, &p) == 0);
	assert(p.stdout_fd >= 0);
	char obuf[128] = {0};
	ssize_t nread = read(p.stdout_fd, obuf, sizeof(obuf) - 1);
	assert(nread > 0);
	assert(strstr(obuf, "stele-proc-ok") != NULL);
	int ec = -1;
	assert(stl_proc_wait(&p, &ec, -1) == 0 && ec == 0);
	stl_proc_close(&p);

	assert(stl_spawn(argv_cat, STL_SPAWN_STDIN | STL_SPAWN_STDOUT, &p) == 0);
	assert(p.stdin_fd >= 0 && p.stdout_fd >= 0);
	assert(write(p.stdin_fd, "stdin-ok\n", 9) == 9);
	close(p.stdin_fd);
	p.stdin_fd = -1;
	memset(obuf, 0, sizeof(obuf));
	nread = read(p.stdout_fd, obuf, sizeof(obuf) - 1);
	assert(nread > 0 && strstr(obuf, "stdin-ok") != NULL);
	assert(stl_proc_wait(&p, &ec, -1) == 0 && ec == 0);
	stl_proc_close(&p);

	assert(stl_spawn(argv_sleep, STL_SPAWN_NEW_GROUP, &p) == 0);
	assert(stl_proc_wait(&p, &ec, 10) == 1); /* still running */
	assert(stl_proc_kill(&p, 1) == 0);
	assert(stl_proc_wait(&p, &ec, 2000) == 0);
	stl_proc_close(&p);

	assert(stl_spawn_detached(argv_det) == 0);
	const char *argv_bad[] = { "/nonexistent_stele_binary_xyz", NULL };
	assert(stl_spawn(argv_bad, 0, &p) == -1);
	printf("ok: stl_spawn / stl_proc_wait / stl_proc_kill / stl_spawn_detached\n");

	printf("stele_port_test: all assertions passed\n");
	return 0;
}
