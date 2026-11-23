#define _GNU_SOURCE
#include "./../include/ttypt/stele.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#endif
#endif

static void
strip_trailing_sep(char *path)
{
	size_t n = strlen(path);
	while (n > 1 && (path[n - 1] == '/' || path[n - 1] == '\\')) {
#ifdef _WIN32
		if (n == 3 && path[1] == ':')
			break;
#endif
		path[--n] = '\0';
	}
}

const char *
stl_so_ext(void)
{
#ifdef _WIN32
	return "dll";
#else
	return "so";
#endif
}

const char *
stl_username(char *buf, size_t cap)
{
	if (!buf || cap == 0)
		return NULL;
#ifdef _WIN32
	const char *u = getenv("USERNAME");
	if (u && *u) {
		stl_strlcpy(buf, u, cap);
		return buf;
	}
	DWORD sz = (DWORD)cap;
	if (GetUserNameA(buf, &sz) && buf[0])
		return buf;
#else
	const char *u = getenv("USER");
	if (!u || !*u)
		u = getenv("LOGNAME");
	if (u && *u) {
		stl_strlcpy(buf, u, cap);
		return buf;
	}
	struct passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_name && *pw->pw_name) {
		stl_strlcpy(buf, pw->pw_name, cap);
		return buf;
	}
#endif
	stl_strlcpy(buf, "user", cap);
	return buf;
}

const char *
stl_home_dir(char *buf, size_t cap)
{
	if (!buf || cap == 0)
		return NULL;
#ifdef _WIN32
	const char *h = getenv("USERPROFILE");
	if (h && *h) {
		stl_strlcpy(buf, h, cap);
		strip_trailing_sep(buf);
		return buf;
	}
	const char *hd = getenv("HOMEDRIVE");
	const char *hp = getenv("HOMEPATH");
	if (hd && *hd && hp && *hp) {
		snprintf(buf, cap, "%s%s", hd, hp);
		strip_trailing_sep(buf);
		return buf;
	}
	h = getenv("HOME");
	if (h && *h) {
		stl_strlcpy(buf, h, cap);
		strip_trailing_sep(buf);
		return buf;
	}
#else
	const char *h = getenv("HOME");
	if (h && *h) {
		stl_strlcpy(buf, h, cap);
		strip_trailing_sep(buf);
		return buf;
	}
	struct passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_dir && *pw->pw_dir) {
		stl_strlcpy(buf, pw->pw_dir, cap);
		strip_trailing_sep(buf);
		return buf;
	}
#endif
	stl_strlcpy(buf, ".", cap);
	return buf;
}

const char *
stl_temp_dir(char *buf, size_t cap)
{
	if (!buf || cap == 0)
		return NULL;
#ifdef _WIN32
	DWORD n = GetTempPathA((DWORD)cap, buf);
	if (n > 0 && n < cap) {
		strip_trailing_sep(buf);
		return buf;
	}
	const char *t = getenv("TEMP");
	if (!t || !*t) t = getenv("TMP");
	if (t && *t) {
		stl_strlcpy(buf, t, cap);
		strip_trailing_sep(buf);
		return buf;
	}
	stl_strlcpy(buf, ".", cap);
	return buf;
#else
	const char *t = getenv("TMPDIR");
	if (t && *t) {
		stl_strlcpy(buf, t, cap);
		strip_trailing_sep(buf);
		return buf;
	}
	stl_strlcpy(buf, "/tmp", cap);
	return buf;
#endif
}

const char *
stl_config_dir(const char *app, char *buf, size_t cap)
{
	if (!buf || cap == 0)
		return NULL;
	char base[512];
#ifdef _WIN32
	const char *ad = getenv("APPDATA");
	if (ad && *ad) {
		stl_strlcpy(base, ad, sizeof(base));
		strip_trailing_sep(base);
	} else {
		stl_home_dir(base, sizeof(base));
		stl_strlcat(base, "\\.config", sizeof(base));
	}
	if (app && *app)
		snprintf(buf, cap, "%s\\%s", base, app);
	else
		stl_strlcpy(buf, base, cap);
#else
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg) {
		stl_strlcpy(base, xdg, sizeof(base));
		strip_trailing_sep(base);
	} else {
		stl_home_dir(base, sizeof(base));
		stl_strlcat(base, "/.config", sizeof(base));
	}
	if (app && *app)
		snprintf(buf, cap, "%s/%s", base, app);
	else
		stl_strlcpy(buf, base, cap);
#endif
	return buf;
}

const char *
stl_data_dir(const char *app, char *buf, size_t cap)
{
	if (!buf || cap == 0)
		return NULL;
	char base[512];
#ifdef _WIN32
	const char *lad = getenv("LOCALAPPDATA");
	if (lad && *lad) {
		stl_strlcpy(base, lad, sizeof(base));
		strip_trailing_sep(base);
	} else {
		stl_home_dir(base, sizeof(base));
		stl_strlcat(base, "\\.local\\share", sizeof(base));
	}
	if (app && *app)
		snprintf(buf, cap, "%s\\%s", base, app);
	else
		stl_strlcpy(buf, base, cap);
#else
	const char *xdg = getenv("XDG_DATA_HOME");
	if (xdg && *xdg) {
		stl_strlcpy(base, xdg, sizeof(base));
		strip_trailing_sep(base);
	} else {
		stl_home_dir(base, sizeof(base));
		stl_strlcat(base, "/.local/share", sizeof(base));
	}
	if (app && *app)
		snprintf(buf, cap, "%s/%s", base, app);
	else
		stl_strlcpy(buf, base, cap);
#endif
	return buf;
}

const char *
stl_exe_dir(char *buf, size_t cap)
{
	if (!buf || cap == 0)
		return NULL;
	char exe[1024];
	exe[0] = '\0';
#ifdef _WIN32
	DWORD n = GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
	if (n == 0 || n >= sizeof(exe)) {
		stl_strlcpy(buf, ".", cap);
		return buf;
	}
#elif defined(__APPLE__)
	char raw[1024];
	uint32_t sz = (uint32_t)sizeof(raw);
	if (_NSGetExecutablePath(raw, &sz) == 0) {
		if (!realpath(raw, exe))
			stl_strlcpy(exe, raw, sizeof(exe));
	}
#elif defined(__linux__)
	ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (n > 0)
		exe[n] = '\0';
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	size_t len = sizeof(exe);
	if (sysctl(mib, 4, exe, &len, NULL, 0) != 0)
		exe[0] = '\0';
#endif
	if (!exe[0]) {
		stl_strlcpy(buf, ".", cap);
		return buf;
	}
	char *slash = strrchr(exe, '/');
	char *bslash = strrchr(exe, '\\');
	char *sep = slash > bslash ? slash : bslash;
	if (sep) {
#ifdef _WIN32
		if (sep == exe + 2 && exe[1] == ':')
			sep[1] = '\0';
		else if (sep == exe)
			sep[1] = '\0';
		else
			*sep = '\0';
#else
		if (sep == exe)
			sep[1] = '\0';
		else
			*sep = '\0';
#endif
		stl_strlcpy(buf, exe, cap);
	} else {
		stl_strlcpy(buf, ".", cap);
	}
	return buf;
}

int
stl_mkdir_p(const char *path)
{
	if (!path || !*path)
		return -1;
	char tmp[1024];
	if (stl_strlcpy(tmp, path, sizeof(tmp)) >= sizeof(tmp))
		return -1;
	strip_trailing_sep(tmp);
	for (size_t i = 1; tmp[i]; i++) {
		if (tmp[i] == '/' || tmp[i] == '\\') {
#ifdef _WIN32
			if (i == 2 && tmp[1] == ':')
				continue;
			if (i == 1 && tmp[0] == '\\')
				continue;
#endif
			char saved = tmp[i];
			tmp[i] = '\0';
			if (stl_mkdir(tmp) != 0 && errno != EEXIST) {
				struct stat st;
				if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
					tmp[i] = saved;
					return -1;
				}
			}
			tmp[i] = saved;
		}
	}
	if (stl_mkdir(tmp) != 0 && errno != EEXIST) {
		struct stat st;
		if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode))
			return -1;
	}
	return 0;
}

static void
proc_init_zero(stl_proc_t *p)
{
	if (!p) return;
	memset(p, 0, sizeof(*p));
#ifndef _WIN32
	p->pid = -1;
#endif
	p->stdin_fd = -1;
	p->stdout_fd = -1;
	p->stderr_fd = -1;
}

#ifdef _WIN32
static void
append_win32_arg(char **buf, size_t *len, size_t *cap, const char *arg)
{
	size_t alen = strlen(arg);
	int need_quote = (alen == 0);
	for (size_t i = 0; i < alen && !need_quote; i++) {
		if (arg[i] == ' ' || arg[i] == '\t' || arg[i] == '"' ||
		    arg[i] == '\n' || arg[i] == '\v')
			need_quote = 1;
	}
	size_t worst = *len + alen * 2 + 8;
	if (worst > *cap) {
		while (*cap < worst)
			*cap = (*cap == 0) ? 256 : (*cap * 2);
		char *nb = realloc(*buf, *cap);
		if (!nb) return;
		*buf = nb;
	}
	char *dst = *buf + *len;
	if (*len > 0) *dst++ = ' ';
	if (!need_quote) {
		memcpy(dst, arg, alen);
		dst += alen;
		*dst = '\0';
		*len = (size_t)(dst - *buf);
		return;
	}
	*dst++ = '"';
	size_t bs = 0;
	for (size_t i = 0; i < alen; i++) {
		if (arg[i] == '\\') {
			bs++;
		} else if (arg[i] == '"') {
			for (size_t k = 0; k < bs * 2 + 1; k++)
				*dst++ = '\\';
			*dst++ = '"';
			bs = 0;
		} else {
			for (size_t k = 0; k < bs; k++)
				*dst++ = '\\';
			*dst++ = arg[i];
			bs = 0;
		}
	}
	for (size_t k = 0; k < bs * 2; k++)
		*dst++ = '\\';
	*dst++ = '"';
	*dst = '\0';
	*len = (size_t)(dst - *buf);
}

int
stl_spawn(const char *const argv[], int flags, stl_proc_t *out)
{
	if (!argv || !argv[0] || !argv[0][0])
		return -1;
	if (!out && !(flags & STL_SPAWN_DETACHED))
		return -1;
	if (out)
		proc_init_zero(out);

	char *cmdline = NULL;
	size_t clen = 0, ccap = 0;
	for (size_t i = 0; argv[i]; i++)
		append_win32_arg(&cmdline, &clen, &ccap, argv[i]);
	if (!cmdline)
		return -1;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	HANDLE in_r = INVALID_HANDLE_VALUE, in_w = INVALID_HANDLE_VALUE;
	HANDLE out_r = INVALID_HANDLE_VALUE, out_w = INVALID_HANDLE_VALUE;
	HANDLE err_r = INVALID_HANDLE_VALUE, err_w = INVALID_HANDLE_VALUE;
	HANDLE nul_in = INVALID_HANDLE_VALUE, nul_out = INVALID_HANDLE_VALUE;

	int merge_err = (flags & STL_SPAWN_MERGE_STDERR) && (flags & STL_SPAWN_STDOUT);

	if (flags & STL_SPAWN_STDIN) {
		if (!CreatePipe(&in_r, &in_w, &sa, 0))
			goto fail;
		SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
	}
	if (flags & STL_SPAWN_STDOUT) {
		if (!CreatePipe(&out_r, &out_w, &sa, 0))
			goto fail;
		SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
	}
	if ((flags & STL_SPAWN_STDERR) && !merge_err) {
		if (!CreatePipe(&err_r, &err_w, &sa, 0))
			goto fail;
		SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
	}

	STARTUPINFOA si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	int need_handles = (flags & (STL_SPAWN_STDIN | STL_SPAWN_STDOUT | STL_SPAWN_STDERR | STL_SPAWN_DETACHED));
	if (need_handles) {
		si.dwFlags |= STARTF_USESTDHANDLES;
		if (flags & STL_SPAWN_DETACHED) {
			nul_in = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
			nul_out = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
		}
		si.hStdInput = (in_r != INVALID_HANDLE_VALUE) ? in_r :
			((flags & STL_SPAWN_DETACHED) ? nul_in : GetStdHandle(STD_INPUT_HANDLE));
		si.hStdOutput = (out_w != INVALID_HANDLE_VALUE) ? out_w :
			((flags & STL_SPAWN_DETACHED) ? nul_out : GetStdHandle(STD_OUTPUT_HANDLE));
		if (merge_err)
			si.hStdError = out_w;
		else if (err_w != INVALID_HANDLE_VALUE)
			si.hStdError = err_w;
		else if (flags & STL_SPAWN_DETACHED)
			si.hStdError = nul_out;
		else
			si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	}

	DWORD cflags = 0;
	if (flags & STL_SPAWN_DETACHED)
		cflags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW;
	else if (flags & STL_SPAWN_NEW_GROUP)
		cflags |= CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW;

	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));
	BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL,
	                         need_handles ? TRUE : FALSE,
	                         cflags, NULL, NULL, &si, &pi);
	free(cmdline);
	cmdline = NULL;

	if (in_r != INVALID_HANDLE_VALUE) CloseHandle(in_r);
	if (out_w != INVALID_HANDLE_VALUE) CloseHandle(out_w);
	if (err_w != INVALID_HANDLE_VALUE) CloseHandle(err_w);
	if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
	if (nul_out != INVALID_HANDLE_VALUE) CloseHandle(nul_out);

	if (!ok) {
		if (in_w != INVALID_HANDLE_VALUE) CloseHandle(in_w);
		if (out_r != INVALID_HANDLE_VALUE) CloseHandle(out_r);
		if (err_r != INVALID_HANDLE_VALUE) CloseHandle(err_r);
		return -1;
	}

	CloseHandle(pi.hThread);
	if (!out) {
		CloseHandle(pi.hProcess);
		if (in_w != INVALID_HANDLE_VALUE) CloseHandle(in_w);
		if (out_r != INVALID_HANDLE_VALUE) CloseHandle(out_r);
		if (err_r != INVALID_HANDLE_VALUE) CloseHandle(err_r);
		return 0;
	}

	out->handle = (void *)pi.hProcess;
	out->pid = (unsigned long)pi.dwProcessId;
	if (flags & STL_SPAWN_NEW_GROUP) {
		HANDLE job = CreateJobObjectA(NULL, NULL);
		if (job) {
			if (AssignProcessToJobObject(job, pi.hProcess))
				out->job = (void *)job;
			else
				CloseHandle(job);
		}
	}
	if (in_w != INVALID_HANDLE_VALUE)
		out->stdin_fd = _open_osfhandle((intptr_t)in_w, _O_WRONLY | _O_BINARY);
	if (out_r != INVALID_HANDLE_VALUE)
		out->stdout_fd = _open_osfhandle((intptr_t)out_r, _O_RDONLY | _O_BINARY);
	if (err_r != INVALID_HANDLE_VALUE)
		out->stderr_fd = _open_osfhandle((intptr_t)err_r, _O_RDONLY | _O_BINARY);
	return 0;

fail:
	free(cmdline);
	if (in_r != INVALID_HANDLE_VALUE) CloseHandle(in_r);
	if (in_w != INVALID_HANDLE_VALUE) CloseHandle(in_w);
	if (out_r != INVALID_HANDLE_VALUE) CloseHandle(out_r);
	if (out_w != INVALID_HANDLE_VALUE) CloseHandle(out_w);
	if (err_r != INVALID_HANDLE_VALUE) CloseHandle(err_r);
	if (err_w != INVALID_HANDLE_VALUE) CloseHandle(err_w);
	return -1;
}

int
stl_proc_wait(stl_proc_t *p, int *exit_code, int timeout_ms)
{
	if (!p || !p->handle)
		return -1;
	DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
	DWORD wr = WaitForSingleObject((HANDLE)p->handle, ms);
	if (wr == WAIT_TIMEOUT)
		return 1;
	if (wr != WAIT_OBJECT_0)
		return -1;
	if (exit_code) {
		DWORD ec = 0;
		GetExitCodeProcess((HANDLE)p->handle, &ec);
		*exit_code = (int)ec;
	}
	return 0;
}

int
stl_proc_kill(stl_proc_t *p, int force)
{
	(void)force;
	if (!p || !p->handle)
		return -1;
	if (p->job) {
		if (TerminateJobObject((HANDLE)p->job, 1))
			return 0;
	}
	return TerminateProcess((HANDLE)p->handle, 1) ? 0 : -1;
}

void
stl_proc_close(stl_proc_t *p)
{
	if (!p) return;
	if (p->stdin_fd >= 0) { _close(p->stdin_fd); p->stdin_fd = -1; }
	if (p->stdout_fd >= 0) { _close(p->stdout_fd); p->stdout_fd = -1; }
	if (p->stderr_fd >= 0) { _close(p->stderr_fd); p->stderr_fd = -1; }
	if (p->job) { CloseHandle((HANDLE)p->job); p->job = NULL; }
	if (p->handle) { CloseHandle((HANDLE)p->handle); p->handle = NULL; }
	p->pid = 0;
}

#else /* POSIX */

static void
set_cloexec(int fd)
{
	if (fd >= 0)
		fcntl(fd, F_SETFD, FD_CLOEXEC);
}

static void
close_pair(int p[2])
{
	if (p[0] >= 0) { close(p[0]); p[0] = -1; }
	if (p[1] >= 0) { close(p[1]); p[1] = -1; }
}

static void
child_exec_setup(const char *const argv[], int flags,
                 int in_p[2], int out_p[2], int err_p[2], int exec_p[2])
{
	if (exec_p[0] >= 0) close(exec_p[0]);
	if (flags & STL_SPAWN_DETACHED)
		setsid();
	else if (flags & STL_SPAWN_NEW_GROUP)
		setpgid(0, 0);

	if (in_p[0] >= 0) {
		dup2(in_p[0], STDIN_FILENO);
	} else if (flags & STL_SPAWN_DETACHED) {
		int dn = open("/dev/null", O_RDONLY);
		if (dn >= 0) { dup2(dn, STDIN_FILENO); close(dn); }
	}
	if (out_p[1] >= 0) {
		dup2(out_p[1], STDOUT_FILENO);
	} else if (flags & STL_SPAWN_DETACHED) {
		int dn = open("/dev/null", O_WRONLY);
		if (dn >= 0) { dup2(dn, STDOUT_FILENO); close(dn); }
	}
	if ((flags & STL_SPAWN_MERGE_STDERR) && out_p[1] >= 0) {
		dup2(out_p[1], STDERR_FILENO);
	} else if (err_p[1] >= 0) {
		dup2(err_p[1], STDERR_FILENO);
	} else if (flags & STL_SPAWN_DETACHED) {
		int dn = open("/dev/null", O_WRONLY);
		if (dn >= 0) { dup2(dn, STDERR_FILENO); close(dn); }
	}

	close_pair(in_p);
	close_pair(out_p);
	close_pair(err_p);

	execvp(argv[0], (char *const *)argv);
	int err = errno ? errno : ENOENT;
	if (exec_p[1] >= 0) {
		ssize_t wr = write(exec_p[1], &err, sizeof(err));
		(void)wr;
		close(exec_p[1]);
	}
	_exit(127);
}

int
stl_spawn(const char *const argv[], int flags, stl_proc_t *out)
{
	if (!argv || !argv[0] || !argv[0][0])
		return -1;
	if (!out && !(flags & STL_SPAWN_DETACHED))
		return -1;
	if (out)
		proc_init_zero(out);

	int in_p[2] = {-1, -1};
	int out_p[2] = {-1, -1};
	int err_p[2] = {-1, -1};
	int exec_p[2] = {-1, -1};
	int merge_err = (flags & STL_SPAWN_MERGE_STDERR) && (flags & STL_SPAWN_STDOUT);

	if (pipe(exec_p) < 0)
		return -1;
	set_cloexec(exec_p[0]);
	set_cloexec(exec_p[1]);

	if ((flags & STL_SPAWN_STDIN) && pipe(in_p) < 0) goto fail;
	if ((flags & STL_SPAWN_STDOUT) && pipe(out_p) < 0) goto fail;
	if ((flags & STL_SPAWN_STDERR) && !merge_err && pipe(err_p) < 0) goto fail;

	set_cloexec(in_p[1]);
	set_cloexec(out_p[0]);
	set_cloexec(err_p[0]);

	if (!out && (flags & STL_SPAWN_DETACHED)) {
		pid_t mid = fork();
		if (mid < 0)
			goto fail;
		if (mid == 0) {
			close(exec_p[0]);
			setsid();
			pid_t gpid = fork();
			if (gpid < 0) {
				int e = errno;
				ssize_t wr = write(exec_p[1], &e, sizeof(e));
				(void)wr;
				_exit(127);
			}
			if (gpid > 0) {
				close(exec_p[1]);
				_exit(0);
			}
			child_exec_setup(argv, flags, in_p, out_p, err_p, exec_p);
		}
		close(exec_p[1]);
		close_pair(in_p);
		close_pair(out_p);
		close_pair(err_p);
		while (waitpid(mid, NULL, 0) < 0 && errno == EINTR) {}
		int child_err = 0;
		ssize_t r;
		while ((r = read(exec_p[0], &child_err, sizeof(child_err))) < 0 && errno == EINTR) {}
		close(exec_p[0]);
		if (r > 0) {
			errno = child_err ? child_err : ENOENT;
			return -1;
		}
		return 0;
	}

	pid_t pid = fork();
	if (pid < 0)
		goto fail;
	if (pid == 0)
		child_exec_setup(argv, flags, in_p, out_p, err_p, exec_p);

	close(exec_p[1]);
	if (in_p[0] >= 0) close(in_p[0]);
	if (out_p[1] >= 0) close(out_p[1]);
	if (err_p[1] >= 0) close(err_p[1]);

	int child_err = 0;
	ssize_t r;
	while ((r = read(exec_p[0], &child_err, sizeof(child_err))) < 0 && errno == EINTR) {}
	close(exec_p[0]);
	if (r > 0) {
		while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
		if (in_p[1] >= 0) close(in_p[1]);
		if (out_p[0] >= 0) close(out_p[0]);
		if (err_p[0] >= 0) close(err_p[0]);
		errno = child_err ? child_err : ENOENT;
		return -1;
	}

	out->pid = pid;
	out->stdin_fd = in_p[1];
	out->stdout_fd = out_p[0];
	out->stderr_fd = err_p[0];
	out->is_group = ((flags & (STL_SPAWN_NEW_GROUP | STL_SPAWN_DETACHED)) != 0);
	return 0;

fail:
	close_pair(exec_p);
	close_pair(in_p);
	close_pair(out_p);
	close_pair(err_p);
	return -1;
}

int
stl_proc_wait(stl_proc_t *p, int *exit_code, int timeout_ms)
{
	if (!p || p->pid <= 0)
		return -1;
	int st = 0;
	if (timeout_ms < 0) {
		pid_t r;
		while ((r = waitpid(p->pid, &st, 0)) < 0 && errno == EINTR) {}
		if (r <= 0) return -1;
	} else {
		uint64_t deadline = stl_now_ms() + (uint64_t)timeout_ms;
		for (;;) {
			pid_t r = waitpid(p->pid, &st, WNOHANG);
			if (r < 0) {
				if (errno == EINTR) continue;
				return -1;
			}
			if (r > 0)
				break;
			if (timeout_ms == 0 || stl_now_ms() >= deadline)
				return 1;
			stl_sleep_ms(2);
		}
	}
	if (exit_code) {
		if (WIFEXITED(st))
			*exit_code = WEXITSTATUS(st);
		else if (WIFSIGNALED(st))
			*exit_code = 128 + WTERMSIG(st);
		else
			*exit_code = -1;
	}
	p->pid = -1;
	return 0;
}

int
stl_proc_kill(stl_proc_t *p, int force)
{
	if (!p || p->pid <= 0)
		return -1;
	int sig = force ? SIGKILL : SIGTERM;
	if (p->is_group && kill(-p->pid, sig) == 0)
		return 0;
	return kill(p->pid, sig);
}

void
stl_proc_close(stl_proc_t *p)
{
	if (!p) return;
	if (p->stdin_fd >= 0) { close(p->stdin_fd); p->stdin_fd = -1; }
	if (p->stdout_fd >= 0) { close(p->stdout_fd); p->stdout_fd = -1; }
	if (p->stderr_fd >= 0) { close(p->stderr_fd); p->stderr_fd = -1; }
	p->pid = -1;
}

#endif /* _WIN32 */

int
stl_spawn_detached(const char *const argv[])
{
	return stl_spawn(argv, STL_SPAWN_DETACHED, NULL);
}
