#include <vfi_api.h>
#include <poll.h>
#include <stdarg.h>
#include <semaphore.h>

struct vfi_dev {
	int fd;
	FILE *file;
	aio_context_t ctx;
	int to;
	struct vfi_npc *funcs;
	struct vfi_npc *maps;
	struct vfi_npc *events;
	struct vfi_cmd_elem *pre_commands;
	struct vfi_cmd_elem *post_commands;
};

struct vfi_cmd_elem {
	struct vfi_cmd_elem *next;
	void **(*f) (struct vfi_dev *, struct vfi_async_handle *, char *);
	int size;
	char *cmd;		/* this is the name of the command is self->b */
	char b[];		/* Holds the name of the card, pointed
				 * to by cmd above. */
};

struct vfi_npc {
	struct vfi_npc *next;
	char *name;		/* name of closure self->b */
	int size;		/* size of name */
	void *e;		/* closure */
	char b[];		/* buffer for name */
};

/*
 * Generalize input of command strings from multiple sources, command
 * line parameters (inputs), stream read (from files or stdin).
 */
static int get_file(void **s, char **command)
{
	int ret;
	FILE *fp = *s;
	ret = fscanf(fp, "%a[^\n]", command);
	if (ret > 0)
		fgetc(fp);
	return ret > 0;
}

int vfi_setup_file(struct vfi_dev *dev, struct vfi_source **src,
		     FILE * fp)
{
	struct vfi_source *h = malloc(sizeof(*h) + sizeof(void *));
	*src = h;

	if (fp == NULL)
		return -EINVAL;

	if (h == NULL)
		return -ENOMEM;

	h->f = get_file;
	h->d = dev;
	h->h[0] = (void *)fp;

	return 0;
}

int vfi_get_cmd(struct vfi_source *src, char **command)
{
	if (*command)
		free(*command);

	return (src->f(src->h, command) > 0);
}

/*
 * Lists of named polymorphic closures (NPC)
 */

/* Generic lookup name in list */
void *vfi_find_npc(struct vfi_npc *elems, char *name)
{
	struct vfi_npc *l;
	int size = strlen(name);

	for (l = elems; l; l = l->next) {
		if ((size == l->size) && !strcmp(l->name, name)) {
			return l->e;
		}
	}
	return NULL;
}

/* Store three lists in dev, funcs, maps and events */
void *vfi_find_func(struct vfi_dev *dev, char *name)
{
	void *ret;
	ret = vfi_find_npc(dev->funcs, name);
	return ret;
}

void *vfi_find_map(struct vfi_dev *dev, char *name)
{
	void *ret;
	ret = vfi_find_npc(dev->maps, name);
	return ret;
}

void *vfi_find_event(struct vfi_dev *dev, char *name)
{
	void *ret;
	ret = vfi_find_npc(dev->events, name);
	return ret;
}

/* Generic register an NPC in a list with a name. */
void *vfi_register_npc(struct vfi_npc **elems, char *name, void *e)
{
	struct vfi_npc *l;
	if (vfi_find_npc(*elems, name))
		return 0;
	l = calloc(1, sizeof(*l) + strlen(name) + 1);
	l->size = strlen(name);
	strcpy(l->b, name);
	l->name = l->b;
	l->e = e;
	l->next = *elems;
	*elems = l;
	return e;
}

/* Again, three lists in dev, funcs, maps, events. */
void *vfi_register_map(struct vfi_dev *dev, char *name, void *e)
{
	return vfi_register_npc(&dev->maps, name, e);
}

void *vfi_register_func(struct vfi_dev *dev, char *name, void *e)
{
	return vfi_register_npc(&dev->funcs, name, e);
}

void *vfi_register_event(struct vfi_dev *dev, char *name, void *e)
{
	return vfi_register_npc(&dev->events, name, e);
}

/*
 * Generic find a command in a list and execute it.  Commands take dev
 * an async handle and a parameter string and return a void * allowing
 * the construction of closures.
 */
void *vfi_find_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah,
		     struct vfi_cmd_elem *commands, char *buf)
{
	struct vfi_cmd_elem *cmd;
	char *term;
	term = strstr(buf, "://");

	if (term) {
		int size = term - buf;

		for (cmd = commands; cmd && cmd->f; cmd = cmd->next)
			if (size == cmd->size && !strncmp(buf, cmd->cmd, size))
				return cmd->f(dev, ah, buf);			       
	}
	return 0;
}

/* Dev stores two lists of commands, a pre and post list. */
void *vfi_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah,
			 char *buf)
{
	return vfi_find_cmd(dev, ah, dev->pre_commands, buf);
}

void *vfi_find_post_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah,
			  char *buf)
{
	return vfi_find_cmd(dev, ah, dev->post_commands, buf);
}

/* 
 * Register commands to the pre and post lists...
 */
int vfi_register_pre_cmd(struct vfi_dev *dev, char *name,
			   void **(*f) (struct vfi_dev *,
					struct vfi_async_handle *, char *))
{
	struct vfi_cmd_elem *c;
	int len = strlen(name);
	c = calloc(1, sizeof(*c) + len + 1);
	strcpy(c->b, name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = dev->pre_commands;
	dev->pre_commands = c;
}

int vfi_register_post_cmd(struct vfi_dev *dev, char *name,
			    void **(*f) (struct vfi_dev *,
					 struct vfi_async_handle *, char *))
{
	struct vfi_cmd_elem *c;
	int len = strlen(name);
	c = calloc(1, sizeof(*c) + len + 1);
	strcpy(c->b, name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = dev->post_commands;
	dev->post_commands = c;
}

/*
 * If threads are implemented against a non-blocking, asynchronous
 * driver some mechanism is required to allow the thread to pick up
 * the reply to a request. We generalize this concept with an
 * asynchronous handle.
 *
 * Because arbitrary thread structures may be involved we make this
 * structure a reference counted entity. The access_semaphore is to
 * protect the ref count. The wait_semaphore is the synchonization
 * point to allow the dispatch loop to release the thread. The async
 * handle carries the result retrieved by the dispatch loop ready for
 * the thread to pick up at its convenience and carries a closure.
 *
 * The closure is provided as a convenience to the thread to allow the
 * communication of arbitrary data from the pre to post event
 * completion execution and allows the construction of arbitrary event
 * handling mechanisms within the application.
 *
 * A closure is a data structure represented as a void * whose only
 * defined element is the first, which is a function which takes a
 * void * and returns a void *. The closure itself is passed to this
 * function. This function knows the true structure of the closure
 * data object and effectively unpacks it to the original set of
 * parameters and invokes the delayed function. 
 *
 * A closure is manufactured by a constructor functions which takes
 * the original set of parameters, packs them up into a closure data
 * structure along with the function to unpack them and continue, and
 * returns the void * pointer to this closure structure.
 */
struct vfi_async_handle {
	char *result;
	void *e;
	struct vfi_async_handle *c;
	sem_t wait_sem;
	sem_t access_sem;
	int count;
};

/* As a convenience the async handle can be passed the closure on its creation. */
struct vfi_async_handle *vfi_alloc_async_handle(void *e)
{
	struct vfi_async_handle *handle = calloc(1, sizeof(*handle));

	if (handle) {
		handle->c = (void *)handle;
		sem_init(&handle->wait_sem, 0, 0);
		sem_init(&handle->access_sem, 0, 1);
		handle->count = 1;
		handle->e = e;
	}
	return (void *)handle;
}

/* Alternatively the closure can be set in the async handle at any
 * time convenient to the application. */
struct vfi_async_handle *vfi_set_async_handle(struct vfi_async_handle *h,
						  void *e)
{
	struct vfi_async_handle *handle = (struct vfi_async_handle *)h;
	if (handle->c == handle) {
		handle->e = e;
		return h;
	}
	return 0;
}

/* This is the synchronization call for a thread which returns the
 * result retrieved by the dispatcher loop and the closure lodged with
 * the handle. The return value is the handle if it is and remains
 * valid, NULL pointer otherwise. */
struct vfi_async_handle *vfi_wait_async_handle(struct vfi_async_handle *h,
						   char **result, void **e)
{
	struct vfi_async_handle *handle = (struct vfi_async_handle *)h;
	if (handle->c == handle) {
		if (sem_wait(&handle->access_sem) < 0)
			return 0;
		handle->count++;
		sem_post(&handle->access_sem);
		if (sem_wait(&handle->wait_sem) < 0)
			return 0;
		*result = handle->result;
		*e = handle->e;
		if (sem_wait(&handle->access_sem) < 0)
			return 0;
		handle->count--;
		if (handle->count == 0) {
			handle->c = 0;
			sem_destroy(&handle->wait_sem);
			sem_destroy(&handle->access_sem);
			free(handle);
			return 0;
		}
		sem_post(&handle->access_sem);
		return h;
	}
	return 0;
}

/* Get and Put operations are used to increment and decrement the ref
 * count of the handle. */
struct vfi_async_handle *vfi_get_async_handle(struct vfi_async_handle *h)
{
	struct vfi_async_handle *handle = (struct vfi_async_handle *)h;
	if (handle->c == handle) {
		if (sem_wait(&handle->access_sem) < 0)
			return 0;
		handle->count++;
		sem_post(&handle->access_sem);
		return h;
	}
	return 0;
}

struct vfi_async_handle *vfi_put_async_handle(struct vfi_async_handle *h)
{
	struct vfi_async_handle *handle = (struct vfi_async_handle *)h;
	if (handle->c == handle) {
		if (sem_wait(&handle->access_sem) < 0)
			return 0;
		handle->count--;
		if (handle->count == 0) {
			handle->c = 0;
			sem_destroy(&handle->wait_sem);
			sem_destroy(&handle->access_sem);
			free(handle);
			return 0;
		}
		sem_post(&handle->access_sem);
		return 0;
	}
	return 0;
}

/* Free is the counter operation to the allocation. As this is a
 * refcounted object it is equivalent to a put. */
struct vfi_async_handle *vfi_free_async_handle(struct vfi_async_handle *h)
{
	return vfi_put_async_handle(h);
}

/* This is the main function of any dispatch loop. Retrieve a result
 * from the driver, decode the reply handle, stash the result in the
 * handle and then post its semaphore to release the waiting thread. */
int vfi_post_async_handle(struct vfi_dev *dev)
{
	int ret;
	char *result = NULL;
	struct vfi_async_handle *handle = NULL;

	ret = vfi_get_result(dev, &result);
	if (ret <= 0)
		return ret;

	handle =
	    (struct vfi_async_handle *)vfi_get_hex_arg(result, "reply");

	if (handle && (handle->c == handle)) {
		handle->result = result;
		sem_post(&handle->wait_sem);
		return 0;
	}

      out:
	ret = -EINVAL;
	free(result);
	return ret;
}

/*
 * The following is an example of a closure and the constructor used
 * to create it. The purpose of this closure is to apply a post
 * command after a driver command has been executed. This closure
 * would be created and inserted in the async handle by the pre
 * command.
 *
 * Consider the mmap requirement. We can have a pre command which
 * intercepts a driver smb_mmap:// command and causes an smb_create
 * post command to be executed once the driver has returned the result
 * of the driver's smb_mmap.
 */
void *vfi_do_post_cmd(void *e)
{
	struct {
		void *f;
		struct vfi_dev *dev;
		struct vfi_async_handle *ah;
		char *buf;
	} *me = e;
	return vfi_find_post_cmd(me->dev, me->ah, me->buf);
}

void *vfi_make_post_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah,
			  char *buf)
{
	void **e = calloc(4, sizeof(void *));
	if (e == NULL)
		return e;

	e[0] = vfi_do_post_cmd;
	e[1] = (void *)dev;
	e[2] = ah;
	e[3] = (void *)buf;
	return e;
}

/*
 * Open and close the vfi driver device and provide a convenient
 * central object to hang the rest of the API objects on, command
 * lists, etc.
 */
int vfi_open(struct vfi_dev **device, char *dev_name, int timeout)
{
	struct vfi_dev *dev = malloc(sizeof(struct vfi_dev));

	*device = dev;
	if (dev == NULL)
		return -ENOMEM;

	dev->to = timeout;
	dev->fd =
	    open(dev_name ? dev_name : "/dev/vfi", (O_NONBLOCK | O_RDWR));

	if (dev->fd < 0) {
		perror("failed");
		free(dev);
		return (0);
	}

	dev->file = fdopen(dev->fd, "r+");

	return 0;
}

void vfi_close(struct vfi_dev *dev)
{
	close(dev->fd);
	free(dev);
}

int vfi_fileno(struct vfi_dev *dev)
{
	return dev->fd;
}

/*
 * The following are convenience routines for parsing strings to
 * extract common parameters and values.
 */

/* Boolean named options, str present is true absent is false. */
int vfi_get_option(char *str, char *name)
{
	if (str && name)
		return (strstr(str, name) != NULL);
	return 0;
}

/* Get a str valued options opt_name(opt_value). Return -1 is the
 * option is not present, 0 if present but with no value, 1 if a value
 * is present. */
int vfi_get_str_arg(char *str, char *name, char **val)
{
	char *var = NULL;

	*val = NULL;

	if (str)
		if ((var = strstr(str, name))) {
			char *arg;
			arg = var + strlen(name);
			if (*arg == '(') {
				sscanf(arg + 1, "%a[^)]", val);
				return 1;
			}
			return 0;
		}

	return -1;
}

/* Retrieve a numeric valued option in various bases. */
int vfi_get_long_arg(char *str, char *name, long *value, int base)
{
	char *val = NULL;
	int ret;
	ret = vfi_get_str_arg(str, name, &val);
	if (ret > 0) {
		*value = strtoul(val, 0, base);
		free(val);
		return ret;
	}
	return 0;
}

/* 
 * Hex interface to get numeric valued option. Assumed hex digits with
 * no leading 0x */
long vfi_get_hex_arg(char *str, char *name)
{
	long val;
	if (vfi_get_long_arg(str, name, &val, 16))
		return val;
	return -1;
}

/* Decimal interface to get numeric valued option. Assumes decimal
 * digits. */
long vfi_get_dec_arg(char *str, char *name)
{
	long val;
	if (vfi_get_long_arg(str, name, &val, 10))
		return val;
	return -1;
}

/* Generic interface to get numeric valued option. Leading 0 is octal
 * base, 0x is hex, numeric is decimal. */
long vfi_get_numeric_arg(char *str, char *name)
{
	long val;
	if (vfi_get_long_arg(str, name, &val, 0))
		return val;
	return -1;
}

/* Poll vfi driver device for read. */
int vfi_poll_read(struct vfi_dev *dev)
{
	struct pollfd fd = { dev->fd, POLLIN, 0 };
	return poll(&fd, 1, dev->to);
}

/* Read vfi device. Either block or if non-block and no result is
 * obtained, block with poll and re-read for result. Caller is
 * responsible for freeing returned result string. */
int vfi_get_result(struct vfi_dev *dev, char **result)
{
	int ret;

	ret = fscanf(dev->file, "%a[^\n]\n", result);

	while (ret <= 0 && ferror(dev->file)) {
		clearerr(dev->file);

		ret = vfi_poll_read(dev);
		if (ret <= 0)
			return ret;

		ret = fscanf(dev->file, "%a[^\n]\n", result);
	}
	return ret;
}

/*
 * The invoke cmd functions are really only of use on a non-blocking
 * vfi driver interface where the application wishes to continue
 * execution in parallel with the drivers execution of the written
 * command. The result will be picked up subsequently.
 *
 * The three functions are simply a varadic and string interface.
 */
int vfi_invoke_cmd_ap(struct vfi_dev *dev, char *f, va_list ap)
{
	int ret;
	ret = vfprintf(dev->file, f, ap);
	fflush(dev->file);
	return ret;
}

int vfi_invoke_cmd(struct vfi_dev *dev, char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap, f);
	ret = vfi_invoke_cmd_ap(dev, f, ap);
	va_end(ap);

	return ret;
}

int vfi_invoke_cmd_str(struct vfi_dev *dev, char *cmd, int size)
{
	int ret;
	if (size)
		ret = fwrite(cmd, size, 1, dev->file);
	else
		ret = fprintf(dev->file, "%s\n", cmd);

	fflush(dev->file);
	return ret;

}

/* 
 * The do_cmd functions are designed to achieve the same blocking
 * semantics as a blocking interface while using a non-blocking driver
 * interface. They simply invoke the command and immediately block
 * waiting for the response. This is only of use to a single thread
 * with a private file descriptor to the driver. If a single interface
 * shared between two or more threads is used with these commands the
 * response retrived cannot be guaranteed to the response to the
 * issued command.
 *
 * Again the three commands are simply the varadic and string
 * interfaces to the underlying commands.
 */
int vfi_do_cmd_ap(struct vfi_dev *dev, char **result, char *f, va_list ap)
{
	int ret;
	ret = vfi_invoke_cmd_ap(dev, f, ap);
	if (ret < 0)
		return ret;

	ret = vfi_get_result(dev, result);
	return ret;
}

int vfi_do_cmd(struct vfi_dev *dev, char **result, char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap, f);
	ret = vfi_do_cmd_ap(dev, result, f, ap);
	va_end(ap);
	return ret;
}

int vfi_do_cmd_str(struct vfi_dev *dev, char **result, char *cmd, int size)
{
	int ret;
	ret = vfi_invoke_cmd_str(dev, cmd, size);
	if (ret < 0)
		return ret;
	ret = vfi_get_result(dev, result);
	return ret;
}

/*********************************************************************
 * F I X  M E
 * The following are non-api'd functions to do with AIO
 * interface. They need wrapping up in an api abstraction before final
 * use.
 */
int vfi_get_eventfd(int count)
{
	int afd;

	/* Initialize event fd */
	if ((afd = eventfd(count)) == -1) {
		perror("eventfd");
		return -1;
	}
	fcntl(afd, F_SETFL, fcntl(afd, F_GETFL, 0) | O_NONBLOCK);

	return afd;
}

void asyio_prep_preadv(struct iocb *iocb, int fd, struct iovec *iov,
		       int nr_segs, int64_t offset, int afd)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PREADV;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) iov;
	iocb->aio_nbytes = nr_segs;
	iocb->aio_offset = offset;
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = afd;
}

void asyio_prep_pwritev(struct iocb *iocb, int fd, struct iovec *iov,
			int nr_segs, int64_t offset, int afd)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PWRITEV;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) iov;
	iocb->aio_nbytes = nr_segs;
	iocb->aio_offset = offset;
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = afd;
}

void asyio_prep_pread(struct iocb *iocb, int fd, void *buf, int nr_segs,
		      int64_t offset, int afd)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PREAD;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) buf;
	iocb->aio_nbytes = nr_segs;
	iocb->aio_offset = offset;
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = afd;
}

void asyio_prep_pwrite(struct iocb *iocb, int fd, void const *buf, int nr_segs,
		       int64_t offset, int afd)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) (unsigned long)buf;
	iocb->aio_nbytes = nr_segs;
	/* Address of string for vfi reply */
	iocb->aio_offset = (u_int64_t) (unsigned long)malloc(256);
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = afd;
}

int waitasync(int afd, int timeout)
{
	struct pollfd fd = { afd, POLLIN, 0 };
	return poll(&fd, 1, timeout);
}
