/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* We need to have separate xfuncs.c and xfuncs_printf.c because
 * with current linkers, even with section garbage collection,
 * if *.o module references any of XXXprintf functions, you pull in
 * entire printf machinery. Even if you do not use the function
 * which uses XXXprintf.
 *
 * xfuncs.c contains functions (not necessarily xfuncs)
 * which do not pull in printf, directly or indirectly.
 * xfunc_printf.c contains those which do.
 */

#include "libbb.h"


/* All the functions starting with "x" call bb_error_msg_and_die() if they
 * fail, so callers never need to check for errors.  If it returned, it
 * succeeded. */

#ifndef DMALLOC
/* dmalloc provides variants of these that do abort() on failure.
 * Since dmalloc's prototypes overwrite the impls here as they are
 * included after these prototypes in libbb.h, all is well.
 */
// Warn if we can't allocate size bytes of memory.
void* FAST_FUNC malloc_or_warn(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		bb_error_msg(bb_msg_memory_exhausted);
	return ptr;
}

// Die if we can't allocate size bytes of memory.
void* FAST_FUNC xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}

// Die if we can't resize previously allocated memory.  (This returns a pointer
// to the new memory, which may or may not be the same as the old memory.
// It'll copy the contents to a new chunk and free the old one if necessary.)
void* FAST_FUNC xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif /* DMALLOC */

// Die if we can't allocate and zero size bytes of memory.
void* FAST_FUNC xzalloc(size_t size)
{
	void *ptr = xmalloc(size);
	memset(ptr, 0, size);
	return ptr;
}

// Die if we can't copy a string to freshly allocated memory.
char* FAST_FUNC xstrdup(const char *s)
{
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup(s);

	if (t == NULL)
		bb_error_msg_and_die(bb_msg_memory_exhausted);

	return t;
}

// Die if we can't allocate n+1 bytes (space for the null terminator) and copy
// the (possibly truncated to length n) string into it.
char* FAST_FUNC xstrndup(const char *s, int n)
{
	int m;
	char *t;

	if (ENABLE_DEBUG && s == NULL)
		bb_error_msg_and_die("xstrndup bug");

	/* We can just xmalloc(n+1) and strncpy into it, */
	/* but think about xstrndup("abc", 10000) wastage! */
	m = n;
	t = (char*) s;
	while (m) {
		if (!*t) break;
		m--;
		t++;
	}
	n -= m;
	t = xmalloc(n + 1);
	t[n] = '\0';

	return memcpy(t, s, n);
}

// Die if we can't open a file and return a FILE* to it.
// Notice we haven't got xfread(), This is for use with fscanf() and friends.
FILE* FAST_FUNC xfopen(const char *path, const char *mode)
{
	FILE *fp = fopen(path, mode);
	if (fp == NULL)
		bb_perror_msg_and_die("can't open '%s'", path);
	return fp;
}

// Die if we can't open a file and return a fd.
int FAST_FUNC xopen3(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		bb_perror_msg_and_die("can't open '%s'", pathname);
	}
	return ret;
}

// Die if we can't open a file and return a fd.
int FAST_FUNC xopen(const char *pathname, int flags)
{
	return xopen3(pathname, flags, 0666);
}

/* Die if we can't open an existing file readonly with O_NONBLOCK
 * and return the fd.
 * Note that for ioctl O_RDONLY is sufficient.
 */
int FAST_FUNC xopen_nonblocking(const char *pathname)
{
	return xopen(pathname, O_RDONLY | O_NONBLOCK);
}

// Warn if we can't open a file and return a fd.
int FAST_FUNC open3_or_warn(const char *pathname, int flags, int mode)
{
	int ret;

	ret = open(pathname, flags, mode);
	if (ret < 0) {
		bb_perror_msg("can't open '%s'", pathname);
	}
	return ret;
}

// Warn if we can't open a file and return a fd.
int FAST_FUNC open_or_warn(const char *pathname, int flags)
{
	return open3_or_warn(pathname, flags, 0666);
}

void FAST_FUNC xunlink(const char *pathname)
{
	if (unlink(pathname))
		bb_perror_msg_and_die("can't remove file '%s'", pathname);
}

void FAST_FUNC xrename(const char *oldpath, const char *newpath)
{
	if (rename(oldpath, newpath))
		bb_perror_msg_and_die("can't move '%s' to '%s'", oldpath, newpath);
}

int FAST_FUNC rename_or_warn(const char *oldpath, const char *newpath)
{
	int n = rename(oldpath, newpath);
	if (n)
		bb_perror_msg("can't move '%s' to '%s'", oldpath, newpath);
	return n;
}

void FAST_FUNC xpipe(int filedes[2])
{
	if (pipe(filedes))
		bb_perror_msg_and_die("can't create pipe");
}

void FAST_FUNC xdup2(int from, int to)
{
	if (dup2(from, to) != to)
		bb_perror_msg_and_die("can't duplicate file descriptor");
}

// "Renumber" opened fd
void FAST_FUNC xmove_fd(int from, int to)
{
	if (from == to)
		return;
	xdup2(from, to);
	close(from);
}

// Die with an error message if we can't write the entire buffer.
void FAST_FUNC xwrite(int fd, const void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_write(fd, buf, count);
		if ((size_t)size != count)
			bb_error_msg_and_die("short write");
	}
}
void FAST_FUNC xwrite_str(int fd, const char *str)
{
	xwrite(fd, str, strlen(str));
}

void FAST_FUNC xclose(int fd)
{
	if (close(fd))
		bb_perror_msg_and_die("close failed");
}

// Die with an error message if we can't lseek to the right spot.
off_t FAST_FUNC xlseek(int fd, off_t offset, int whence)
{
	off_t off = lseek(fd, offset, whence);
	if (off == (off_t)-1) {
		if (whence == SEEK_SET)
			bb_perror_msg_and_die("lseek(%"OFF_FMT"u)", offset);
		bb_perror_msg_and_die("lseek");
	}
	return off;
}

int FAST_FUNC xmkstemp(char *template)
{
	int fd = mkstemp(template);
	if (fd < 0)
		bb_perror_msg_and_die("can't create temp file '%s'", template);
	return fd;
}

// Die with supplied filename if this FILE* has ferror set.
void FAST_FUNC die_if_ferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		/* ferror doesn't set useful errno */
		bb_error_msg_and_die("%s: I/O error", fn);
	}
}

// Die with an error message if stdout has ferror set.
void FAST_FUNC die_if_ferror_stdout(void)
{
	die_if_ferror(stdout, bb_msg_standard_output);
}

int FAST_FUNC fflush_all(void)
{
	return fflush(NULL);
}


int FAST_FUNC bb_putchar(int ch)
{
	return putchar(ch);
}

// Die with an error message if we can't malloc() enough space and do an
// sprintf() into that space.
char* FAST_FUNC xasprintf(const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);

	if (r < 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return string_ptr;
}

void FAST_FUNC xsetenv(const char *key, const char *value)
{
	if (setenv(key, value, 1))
		bb_error_msg_and_die(bb_msg_memory_exhausted);
}

/* Handles "VAR=VAL" strings, even those which are part of environ
 * _right now_
 */
void FAST_FUNC bb_unsetenv(const char *var)
{
	char *tp = strchr(var, '=');

	if (!tp) {
		unsetenv(var);
		return;
	}

	/* In case var was putenv'ed, we can't replace '='
	 * with NUL and unsetenv(var) - it won't work,
	 * env is modified by the replacement, unsetenv
	 * sees "VAR" instead of "VAR=VAL" and does not remove it!
	 * horror :( */
	tp = xstrndup(var, tp - var);
	unsetenv(tp);
	free(tp);
}

void FAST_FUNC bb_unsetenv_and_free(char *var)
{
	bb_unsetenv(var);
	free(var);
}

// Die with an error message if we can't set gid.  (Because resource limits may
// limit this user to a given number of processes, and if that fills up the
// setgid() will fail and we'll _still_be_root_, which is bad.)
void FAST_FUNC xsetgid(gid_t gid)
{
	if (setgid(gid)) bb_perror_msg_and_die("setgid");
}

// Die with an error message if we can't set uid.  (See xsetgid() for why.)
void FAST_FUNC xsetuid(uid_t uid)
{
	if (setuid(uid)) bb_perror_msg_and_die("setuid");
}

// Die if we can't chdir to a new path.
void FAST_FUNC xchdir(const char *path)
{
	if (chdir(path))
		bb_perror_msg_and_die("chdir(%s)", path);
}

void FAST_FUNC xchroot(const char *path)
{
	if (chroot(path))
		bb_perror_msg_and_die("can't change root directory to %s", path);
	xchdir("/");
}

// Print a warning message if opendir() fails, but don't die.
/*
DIR* FAST_FUNC warn_opendir(const char *path)
{
	DIR *dp;

	dp = opendir(path);
	if (!dp)
		bb_perror_msg("can't open '%s'", path);
	return dp;
}

// Die with an error message if opendir() fails.
DIR* FAST_FUNC xopendir(const char *path)
{
	DIR *dp;

	dp = opendir(path);
	if (!dp)
		bb_perror_msg_and_die("can't open '%s'", path);
	return dp;
}
*/
// Die with an error message if we can't open a new socket.
int FAST_FUNC xsocket(int domain, int type, int protocol)
{
	int r = socket(domain, type, protocol);

	if (r < 0) {
		/* Hijack vaguely related config option */
#if ENABLE_VERBOSE_RESOLUTION_ERRORS
		const char *s = "INET";
# ifdef AF_PACKET
		if (domain == AF_PACKET) s = "PACKET";
# endif
# ifdef AF_NETLINK
		if (domain == AF_NETLINK) s = "NETLINK";
# endif
IF_FEATURE_IPV6(if (domain == AF_INET6) s = "INET6";)
		bb_perror_msg_and_die("socket(AF_%s,%d,%d)", s, type, protocol);
#else
		bb_perror_msg_and_die("socket");
#endif
	}

	return r;
}

// Die with an error message if we can't bind a socket to an address.
void FAST_FUNC xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
	if (bind(sockfd, my_addr, addrlen)) bb_perror_msg_and_die("bind");
}

// Die with an error message if we can't listen for connections on a socket.
void FAST_FUNC xlisten(int s, int backlog)
{
	if (listen(s, backlog)) bb_perror_msg_and_die("listen");
}

/* Die with an error message if sendto failed.
 * Return bytes sent otherwise  */
ssize_t FAST_FUNC xsendto(int s, const void *buf, size_t len, const struct sockaddr *to,
				socklen_t tolen)
{
	ssize_t ret = sendto(s, buf, len, 0, to, tolen);
	if (ret < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(s);
		bb_perror_msg_and_die("sendto");
	}
	return ret;
}

// xstat() - a stat() which dies on failure with meaningful error message
void FAST_FUNC xstat(const char *name, struct stat *stat_buf)
{
	if (stat(name, stat_buf))
		bb_perror_msg_and_die("can't stat '%s'", name);
}

void FAST_FUNC xfstat(int fd, struct stat *stat_buf, const char *errmsg)
{
	/* errmsg is usually a file name, but not always:
	 * xfstat may be called in a spot where file name is no longer
	 * available, and caller may give e.g. "can't stat input file" string.
	 */
	if (fstat(fd, stat_buf))
		bb_simple_perror_msg_and_die(errmsg);
}

// selinux_or_die() - die if SELinux is disabled.
void FAST_FUNC selinux_or_die(void)
{
#if ENABLE_SELINUX
	int rc = is_selinux_enabled();
	if (rc == 0) {
		bb_error_msg_and_die("SELinux is disabled");
	} else if (rc < 0) {
		bb_error_msg_and_die("is_selinux_enabled() failed");
	}
#else
	bb_error_msg_and_die("SELinux support is disabled");
#endif
}

int FAST_FUNC ioctl_or_perror_and_die(int fd, unsigned request, void *argp, const char *fmt,...)
{
	int ret;
	va_list p;

	ret = ioctl(fd, request, argp);
	if (ret < 0) {
		va_start(p, fmt);
		bb_verror_msg(fmt, p, strerror(errno));
		/* xfunc_die can actually longjmp, so be nice */
		va_end(p);
		xfunc_die();
	}
	return ret;
}

int FAST_FUNC ioctl_or_perror(int fd, unsigned request, void *argp, const char *fmt,...)
{
	va_list p;
	int ret = ioctl(fd, request, argp);

	if (ret < 0) {
		va_start(p, fmt);
		bb_verror_msg(fmt, p, strerror(errno));
		va_end(p);
	}
	return ret;
}

#if ENABLE_IOCTL_HEX2STR_ERROR
int FAST_FUNC bb_ioctl_or_warn(int fd, unsigned request, void *argp, const char *ioctl_name)
{
	int ret;

	ret = ioctl(fd, request, argp);
	if (ret < 0)
		bb_simple_perror_msg(ioctl_name);
	return ret;
}
int FAST_FUNC bb_xioctl(int fd, unsigned request, void *argp, const char *ioctl_name)
{
	int ret;

	ret = ioctl(fd, request, argp);
	if (ret < 0)
		bb_simple_perror_msg_and_die(ioctl_name);
	return ret;
}
#else
int FAST_FUNC bb_ioctl_or_warn(int fd, unsigned request, void *argp)
{
	int ret;

	ret = ioctl(fd, request, argp);
	if (ret < 0)
		bb_perror_msg("ioctl %#x failed", request);
	return ret;
}
int FAST_FUNC bb_xioctl(int fd, unsigned request, void *argp)
{
	int ret;

	ret = ioctl(fd, request, argp);
	if (ret < 0)
		bb_perror_msg_and_die("ioctl %#x failed", request);
	return ret;
}
#endif

char* FAST_FUNC xmalloc_ttyname(int fd)
{
	char *buf = xzalloc(128);
	int r = ttyname_r(fd, buf, 127);
	if (r) {
		free(buf);
		buf = NULL;
	}
	return buf;
}

#if BB_MMU
pid_t FAST_FUNC xfork(void)
{
	pid_t pid;
	pid = fork();
	if (pid < 0) /* wtf? */
		bb_perror_msg_and_die("vfork"+1);
	return pid;
}
#endif
