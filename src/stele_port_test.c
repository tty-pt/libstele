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

	printf("stele_port_test: all assertions passed\n");
	return 0;
}
