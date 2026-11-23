#define _GNU_SOURCE
#include "./../include/ttypt/stele.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <unistd.h>
#endif

int
stl_sock_set_timeout(stl_socket_t fd, int recv_ms, int send_ms)
{
	if (fd == STL_INVALID_SOCKET)
		return -1;
	int rc = 0;
#ifdef _WIN32
	if (recv_ms >= 0) {
		DWORD ms = (DWORD)recv_ms;
		if (setsockopt((SOCKET)fd, SOL_SOCKET, SO_RCVTIMEO,
		               (const char *)&ms, sizeof(ms)) != 0)
			rc = -1;
	}
	if (send_ms >= 0) {
		DWORD ms = (DWORD)send_ms;
		if (setsockopt((SOCKET)fd, SOL_SOCKET, SO_SNDTIMEO,
		               (const char *)&ms, sizeof(ms)) != 0)
			rc = -1;
	}
#else
	if (recv_ms >= 0) {
		struct timeval tv;
		tv.tv_sec = recv_ms / 1000;
		tv.tv_usec = (recv_ms % 1000) * 1000;
		if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
			rc = -1;
	}
	if (send_ms >= 0) {
		struct timeval tv;
		tv.tv_sec = send_ms / 1000;
		tv.tv_usec = (send_ms % 1000) * 1000;
		if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
			rc = -1;
	}
#endif
	return rc;
}

int
stl_wake_init(stl_wake_t *w)
{
	if (!w)
		return -1;
	w->rfd = STL_INVALID_SOCKET;
	w->wfd = STL_INVALID_SOCKET;

#ifdef _WIN32
	stl_sock_init();
	stl_socket_t lfd = stl_sock_create(AF_INET, SOCK_STREAM, 0);
	if (lfd == STL_INVALID_SOCKET)
		return -1;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;

	if (stl_sock_bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
	    stl_sock_listen(lfd, 1) != 0) {
		stl_sock_close(lfd);
		return -1;
	}

	int alen = (int)sizeof(addr);
	if (getsockname((SOCKET)lfd, (struct sockaddr *)&addr, &alen) != 0) {
		stl_sock_close(lfd);
		return -1;
	}

	stl_socket_t wfd = stl_sock_create(AF_INET, SOCK_STREAM, 0);
	if (wfd == STL_INVALID_SOCKET) {
		stl_sock_close(lfd);
		return -1;
	}
	if (stl_sock_connect(wfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		stl_sock_close(wfd);
		stl_sock_close(lfd);
		return -1;
	}

	stl_socket_t rfd = stl_sock_accept(lfd, NULL, NULL);
	stl_sock_close(lfd);
	if (rfd == STL_INVALID_SOCKET) {
		stl_sock_close(wfd);
		return -1;
	}

	int one = 1;
	setsockopt((SOCKET)wfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
	setsockopt((SOCKET)rfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
	stl_sock_set_nonblock(rfd, 1);
	stl_sock_set_nonblock(wfd, 1);
	w->rfd = rfd;
	w->wfd = wfd;
	return 0;
#else
	int fds[2];
	if (pipe(fds) < 0)
		return -1;
	for (int i = 0; i < 2; i++) {
		int fl = fcntl(fds[i], F_GETFL, 0);
		if (fl >= 0) fcntl(fds[i], F_SETFL, fl | O_NONBLOCK);
		fcntl(fds[i], F_SETFD, FD_CLOEXEC);
	}
	w->rfd = fds[0];
	w->wfd = fds[1];
	return 0;
#endif
}

void
stl_wake_close(stl_wake_t *w)
{
	if (!w)
		return;
#ifdef _WIN32
	if (w->rfd != STL_INVALID_SOCKET) closesocket((SOCKET)w->rfd);
	if (w->wfd != STL_INVALID_SOCKET) closesocket((SOCKET)w->wfd);
#else
	if (w->rfd >= 0) close(w->rfd);
	if (w->wfd >= 0) close(w->wfd);
#endif
	w->rfd = STL_INVALID_SOCKET;
	w->wfd = STL_INVALID_SOCKET;
}

stl_socket_t
stl_wake_fd(const stl_wake_t *w)
{
	return w ? w->rfd : STL_INVALID_SOCKET;
}

void
stl_wake_signal(stl_wake_t *w)
{
	if (!w || w->wfd == STL_INVALID_SOCKET)
		return;
	char b = 1;
#ifdef _WIN32
	(void)send((SOCKET)w->wfd, &b, 1, 0);
#else
	ssize_t wr = write(w->wfd, &b, 1);
	(void)wr;
#endif
}

void
stl_wake_drain(stl_wake_t *w)
{
	if (!w || w->rfd == STL_INVALID_SOCKET)
		return;
	char tmp[64];
#ifdef _WIN32
	while (recv((SOCKET)w->rfd, tmp, (int)sizeof(tmp), 0) > 0) {}
#else
	while (read(w->rfd, tmp, sizeof(tmp)) > 0) {}
#endif
}

typedef enum {
	IPC_KIND_TCP = 1,
	IPC_KIND_UNIX = 2,
	IPC_KIND_WIN_PORTFILE = 3
} ipc_kind_t;

static int
ends_with(const char *s, const char *sfx)
{
	size_t sl = strlen(s);
	size_t xl = strlen(sfx);
	return sl >= xl && strcmp(s + sl - xl, sfx) == 0;
}

static int
parse_endpoint(const char *endpoint, ipc_kind_t *kind,
               char *host, size_t host_cap, uint16_t *port,
               char *path, size_t path_cap)
{
	if (!endpoint || !*endpoint)
		return -1;
	host[0] = '\0';
	path[0] = '\0';
	*port = 0;

	if (strncmp(endpoint, "tcp://", 6) == 0) {
		const char *hp = endpoint + 6;
		const char *colon = strrchr(hp, ':');
		if (!colon)
			return -1;
		size_t hlen = (size_t)(colon - hp);
		if (hlen == 0) {
			stl_strlcpy(host, "127.0.0.1", host_cap);
		} else {
			if (hlen >= host_cap) hlen = host_cap - 1;
			memcpy(host, hp, hlen);
			host[hlen] = '\0';
		}
		long p = strtol(colon + 1, NULL, 10);
		if (p < 0 || p > 65535)
			return -1;
		*port = (uint16_t)p;
		*kind = IPC_KIND_TCP;
		return 0;
	}

	if (strncmp(endpoint, "unix://", 7) == 0) {
		stl_strlcpy(path, endpoint + 7, path_cap);
		*kind = IPC_KIND_UNIX;
		return path[0] ? 0 : -1;
	}

	if (strncmp(endpoint, "unix:", 5) == 0) {
		stl_strlcpy(path, endpoint + 5, path_cap);
		*kind = IPC_KIND_UNIX;
		return path[0] ? 0 : -1;
	}

	if (strchr(endpoint, '/') || strchr(endpoint, '\\') || ends_with(endpoint, ".sock")) {
		stl_strlcpy(path, endpoint, path_cap);
		*kind = IPC_KIND_UNIX;
		return 0;
	}

	/* Logical service name (e.g. "ren-jarvis") */
	char tmp[256], user[128];
	stl_temp_dir(tmp, sizeof(tmp));
	stl_username(user, sizeof(user));

#ifdef _WIN32
	snprintf(path, path_cap, "%s\\%s-%s.port", tmp, endpoint, user);
	stl_strlcpy(host, "127.0.0.1", host_cap);
	*port = 0;
	*kind = IPC_KIND_WIN_PORTFILE;
#else
	snprintf(path, path_cap, "%s/%s-%s.sock", tmp, endpoint, user);
	if (strlen(path) >= 100)
		snprintf(path, path_cap, "/tmp/%s-%s.sock", endpoint, user);
	*kind = IPC_KIND_UNIX;
#endif
	return 0;
}

static int
tcp_listen_bind(const char *host, uint16_t port, int backlog,
                stl_socket_t *out_fd, uint16_t *out_port)
{
	stl_sock_init();
	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo((host && *host) ? host : "127.0.0.1", port_str, &hints, &res) != 0 || !res)
		return -1;

	stl_socket_t fd = stl_sock_create(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (fd == STL_INVALID_SOCKET) {
		freeaddrinfo(res);
		return -1;
	}
#ifndef _WIN32
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
	int yes = 1;
#ifdef _WIN32
	setsockopt((SOCKET)fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
#else
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
	if (stl_sock_bind(fd, res->ai_addr, (socklen_t)res->ai_addrlen) != 0 ||
	    stl_sock_listen(fd, backlog) != 0) {
		stl_sock_close(fd);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);

	struct sockaddr_in bound;
	socklen_t blen = sizeof(bound);
	memset(&bound, 0, sizeof(bound));
#ifdef _WIN32
	int wlen = (int)blen;
	if (getsockname((SOCKET)fd, (struct sockaddr *)&bound, &wlen) == 0)
		*out_port = ntohs(bound.sin_port);
#else
	if (getsockname(fd, (struct sockaddr *)&bound, &blen) == 0)
		*out_port = ntohs(bound.sin_port);
#endif
	*out_fd = fd;
	return 0;
}

static stl_socket_t
tcp_connect_host(const char *host, uint16_t port)
{
	if (port == 0)
		return STL_INVALID_SOCKET;
	stl_sock_init();
	char port_str[16];
	snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo((host && *host) ? host : "127.0.0.1", port_str, &hints, &res) != 0 || !res)
		return STL_INVALID_SOCKET;

	stl_socket_t fd = STL_INVALID_SOCKET;
	for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
		fd = stl_sock_create(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd == STL_INVALID_SOCKET)
			continue;
		if (stl_sock_connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) {
#ifndef _WIN32
			fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
			break;
		}
		stl_sock_close(fd);
		fd = STL_INVALID_SOCKET;
	}
	freeaddrinfo(res);
	return fd;
}

int
stl_ipc_listen(const char *endpoint, int backlog, stl_ipc_listener_t *out)
{
	if (!out)
		return -1;
	memset(out, 0, sizeof(*out));
	out->fd = STL_INVALID_SOCKET;

	ipc_kind_t kind;
	char host[128], path[512];
	uint16_t port = 0;
	if (parse_endpoint(endpoint, &kind, host, sizeof(host), &port, path, sizeof(path)) != 0)
		return -1;

	if (kind == IPC_KIND_TCP) {
		if (tcp_listen_bind(host, port, backlog, &out->fd, &out->port) != 0)
			return -1;
		out->is_unix = 0;
		return 0;
	}

	if (kind == IPC_KIND_WIN_PORTFILE) {
#ifdef _WIN32
		if (tcp_listen_bind("127.0.0.1", 0, backlog, &out->fd, &out->port) != 0)
			return -1;
		char tmpfile[560];
		snprintf(tmpfile, sizeof(tmpfile), "%s.tmp.%lu",
		         path, (unsigned long)GetCurrentProcessId());
		FILE *f = fopen(tmpfile, "w");
		if (!f) {
			stl_sock_close(out->fd);
			out->fd = STL_INVALID_SOCKET;
			return -1;
		}
		fprintf(f, "%u %lu\n", (unsigned)out->port,
		        (unsigned long)GetCurrentProcessId());
		fclose(f);
		if (!MoveFileExA(tmpfile, path, MOVEFILE_REPLACE_EXISTING)) {
			_unlink(tmpfile);
			stl_sock_close(out->fd);
			out->fd = STL_INVALID_SOCKET;
			return -1;
		}
		stl_strlcpy(out->path, path, sizeof(out->path));
		out->is_unix = 0;
		return 0;
#else
		return -1;
#endif
	}

	/* IPC_KIND_UNIX */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	if (strlen(path) >= sizeof(addr.sun_path)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	stl_sock_init();
	stl_socket_t fd = stl_sock_create(AF_UNIX, SOCK_STREAM, 0);
	if (fd == STL_INVALID_SOCKET)
		return -1;
#ifndef _WIN32
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
	addr.sun_family = AF_UNIX;
	stl_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
#ifdef _WIN32
	_unlink(path);
#else
	unlink(path);
#endif
	if (stl_sock_bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		stl_sock_close(fd);
		return -1;
	}
#ifndef _WIN32
	chmod(path, 0600);
#endif
	if (stl_sock_listen(fd, backlog) != 0) {
		stl_sock_close(fd);
#ifdef _WIN32
		_unlink(path);
#else
		unlink(path);
#endif
		return -1;
	}
	out->fd = fd;
	out->is_unix = 1;
	stl_strlcpy(out->path, path, sizeof(out->path));
	return 0;
}

stl_socket_t
stl_ipc_accept(const stl_ipc_listener_t *l)
{
	if (!l || l->fd == STL_INVALID_SOCKET)
		return STL_INVALID_SOCKET;
	stl_socket_t cli = stl_sock_accept(l->fd, NULL, NULL);
#ifndef _WIN32
	if (cli != STL_INVALID_SOCKET)
		fcntl(cli, F_SETFD, FD_CLOEXEC);
#endif
	return cli;
}

void
stl_ipc_close_listener(stl_ipc_listener_t *l)
{
	if (!l)
		return;
	if (l->fd != STL_INVALID_SOCKET) {
		stl_sock_close(l->fd);
		l->fd = STL_INVALID_SOCKET;
	}
	if (l->path[0]) {
#ifdef _WIN32
		_unlink(l->path);
#else
		unlink(l->path);
#endif
		l->path[0] = '\0';
	}
	l->port = 0;
}

stl_socket_t
stl_ipc_connect(const char *endpoint)
{
	ipc_kind_t kind;
	char host[128], path[512];
	uint16_t port = 0;
	if (parse_endpoint(endpoint, &kind, host, sizeof(host), &port, path, sizeof(path)) != 0)
		return STL_INVALID_SOCKET;

	if (kind == IPC_KIND_TCP)
		return tcp_connect_host(host, port);

	if (kind == IPC_KIND_WIN_PORTFILE) {
		FILE *f = fopen(path, "r");
		if (!f)
			return STL_INVALID_SOCKET;
		unsigned p = 0;
		unsigned long pid = 0;
		int n = fscanf(f, "%u %lu", &p, &pid);
		fclose(f);
		if (n < 1 || p == 0 || p > 65535)
			return STL_INVALID_SOCKET;
		if (n == 2 && pid > 0 && stl_pid_alive((pid_t)pid) != 0)
			return STL_INVALID_SOCKET;
		return tcp_connect_host("127.0.0.1", (uint16_t)p);
	}

	/* IPC_KIND_UNIX */
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	if (strlen(path) >= sizeof(addr.sun_path)) {
		errno = ENAMETOOLONG;
		return STL_INVALID_SOCKET;
	}
	stl_sock_init();
	stl_socket_t fd = stl_sock_create(AF_UNIX, SOCK_STREAM, 0);
	if (fd == STL_INVALID_SOCKET)
		return STL_INVALID_SOCKET;
	addr.sun_family = AF_UNIX;
	stl_strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
	if (stl_sock_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		stl_sock_close(fd);
		return STL_INVALID_SOCKET;
	}
#ifndef _WIN32
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
	return fd;
}
