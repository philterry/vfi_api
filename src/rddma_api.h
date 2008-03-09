#ifndef RDDMA_API_H
#define RDDMA_API_H

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdio.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
 /*
 * This were good at the time of 2.6.21-rc5.mm4 ...
 */
#ifndef __NR_eventfd
#if defined(__x86_64__)
#define __NR_eventfd 283
#elif defined(__i386__)
#define __NR_eventfd 323
#elif defined(__powerpc__)
#define __NR_eventfd 308
#else
#error Cannot detect your architecture!
#endif
#endif

typedef unsigned long aio_context_t;

enum {
	IOCB_CMD_PREAD = 0,
		IOCB_CMD_PWRITE = 1,
		IOCB_CMD_FSYNC = 2,
		IOCB_CMD_FDSYNC = 3,
		/* These two are experimental.
		 * IOCB_CMD_PREADX = 4,
		 * IOCB_CMD_POLL = 5,
		 */
		IOCB_CMD_NOOP = 6,
		IOCB_CMD_PREADV = 7,
		IOCB_CMD_PWRITEV = 8,
};

#if defined(__LITTLE_ENDIAN)
#define PADDED(x,y)	x, y
#elif defined(__BIG_ENDIAN)
#define PADDED(x,y)	y, x
#else
#error edit for your odd byteorder.
#endif

#define IOCB_FLAG_RESFD		(1 << 0)

/*
 * we always use a 64bit off_t when communicating
 * with userland.  its up to libraries to do the
 * proper padding and aio_error abstraction
 */
struct iocb {
	/* these are internal to the kernel/libc. */
	u_int64_t	aio_data;	/* data to be returned in event's data */
	u_int32_t	PADDED(aio_key, aio_reserved1);
	/* the kernel sets aio_key to the req # */

	/* common fields */
	u_int16_t	aio_lio_opcode;	/* see IOCB_CMD_ above */
	int16_t	aio_reqprio;
	u_int32_t	aio_fildes;

	u_int64_t	aio_buf;
	u_int64_t	aio_nbytes;
	int64_t	aio_offset;

	/* extra parameters */
	u_int64_t	aio_reserved2;	/* TODO: use this for a (struct sigevent *) */

	u_int32_t	aio_flags;
	/*
	 * If different from 0, this is an eventfd to deliver AIO results to
	 */
	u_int32_t	aio_resfd;
}; /* 64 bytes */


struct io_event {
	u_int64_t           data;           /* the data field from the iocb */
	u_int64_t           obj;            /* what iocb this event came from */
	int64_t           res;            /* result code for this event */
	int64_t           res2;           /* secondary result */
};

static inline long io_setup(unsigned nr_reqs, aio_context_t *ctx) {

	return syscall(__NR_io_setup, nr_reqs, ctx);
}

static inline long io_destroy(aio_context_t ctx) {

	return syscall(__NR_io_destroy, ctx);
}

static inline long io_submit(aio_context_t ctx, long n, struct iocb **paiocb) {

	return syscall(__NR_io_submit, ctx, n, paiocb);
}

static inline long io_cancel(aio_context_t ctx, struct iocb *aiocb, struct io_event *res) {

	return syscall(__NR_io_cancel, ctx, aiocb, res);
}

static inline long io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events,
		  struct timespec *tmo) {

	return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo);
}

static inline int eventfd(int count) {

	return syscall(__NR_eventfd, count);
}

struct rddma_dev {
	int fd;
	FILE *file;
	aio_context_t ctx;
	int to;
	struct rddma_npc *funcs;
	struct rddma_npc *maps;
	struct rddma_npc *events;
	struct rddma_cmd_elem *pre_commands;
	struct rddma_cmd_elem *post_commands;
};

struct rddma_source {
	int (*f)(void *, char **);
	void *h;
};

extern int rddma_setup_file(struct rddma_source **,FILE *);
extern int rddma_get_cmd(struct rddma_source *,char **);
extern void *rddma_find_cmd(struct rddma_dev *, struct rddma_cmd_elem *, char *, int);
extern void *rddma_find_pre_cmd(struct rddma_dev *, char *, int);
extern void *rddma_find_post_cmd(struct rddma_dev *, char *, int);

extern int register_pre_cmd(struct rddma_dev *, char *, void **(*)(struct rddma_dev *,char *));
extern int register_post_cmd(struct rddma_dev *, char *, void **(*)(struct rddma_dev *,char *));

struct rddma_npc {
	struct rddma_npc *next;
	char *name;		/* name of closure self->b */
	int size;		/* size of name */
	void *e;		/* closure */
	char b[];		/* buffer for name */
};

extern void *rddma_register_npc(struct rddma_npc **, char *, void *);
extern void *rddma_register_func(struct rddma_dev *, char *, void *);
extern void *rddma_register_map(struct rddma_dev *, char *, void *);
extern void *rddma_register_event(struct rddma_dev *, char *, void *);
extern void *rddma_find_npc(struct rddma_npc *, char *);
extern void *rddma_find_func(struct rddma_dev *, char *);
extern void *rddma_find_map(struct rddma_dev *, char *);
extern void *rddma_find_event(struct rddma_dev *, char *);

struct rddma_cmd_elem {
	struct rddma_cmd_elem *next;
	void **(*f)(struct rddma_dev *, char *);
	int size;
	char *cmd;		/* this is the name of the command is self->b */
	char b[];		/* Holds the name of the card, pointed
				 * to by cmd above. */
};

extern int rddma_open(struct rddma_dev **, char *, int);
extern void rddma_close(struct rddma_dev *);

extern int rddma_get_option(char *str, char *name);
extern int rddma_get_str_arg(char *str, char *name, char **val);
extern int rddma_get_long_arg(char *str, char *name, long *value, int base);
extern long rddma_get_dec_arg(char *str, char *name);
extern long rddma_get_hex_arg(char *str, char *name);

extern int rddma_poll_read(struct rddma_dev *);

extern int rddma_do_cmd(struct rddma_dev *, char **, char *, ...) __attribute__((format(printf,3,4)));
extern int rddma_do_cmd_ap(struct rddma_dev *, char **, char *, va_list);
extern int rddma_do_cmd_str(struct rddma_dev *, char **, char *, int);

extern int rddma_invoke_cmd(struct rddma_dev *, char *, ...) __attribute__((format(printf,2,3)));
extern int rddma_invoke_cmd_ap(struct rddma_dev *, char *, va_list);
extern int rddma_invoke_cmd_str(struct rddma_dev *, char *, int);

extern int rddma_get_result(struct rddma_dev *, char **);

extern void *rddma_alloc_async_handle(void *);
extern int rddma_free_async_handle(void *);

extern int rddma_get_async_handle(void *, char **, void **);
extern int rddma_set_async_handle(void *, void *);
extern int rddma_put_async_handle(struct rddma_dev *);

extern int rddma_get_eventfd(int);
extern void asyio_prep_pread(struct iocb *iocb, int fd, void *buf, int nr_segs,
			     int64_t offset, int afd);
extern void asyio_prep_pwrite(struct iocb *, int, void const *, int, int64_t, int);
extern void asyio_prep_preadv(struct iocb *iocb, int fd, struct iovec *iov, int nr_segs,
			      int64_t offset, int afd);
extern void asyio_prep_pwritev(struct iocb *iocb, int fd, struct iovec *iov, int nr_segs,
			       int64_t offset, int afd);
extern int waitasync(int, int);

#endif	/* RDDMA_API_H */
