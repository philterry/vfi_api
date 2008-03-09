#include "rddma_api.h"
#include <poll.h>
#include <stdarg.h>
#include <semaphore.h>

/*
 * Generalize input of command strings from multiple sources, command
 * line parameters (inputs), stream read (from files or stdin).
 */
static int get_file(void *s, char **command)
{
	int ret;
	FILE *fp = (FILE *)s;
	ret = fscanf(fp,"%a[^\n]",command);
	if (ret > 0)
		fgetc(fp);
	return ret > 0;
}

int rddma_setup_file(struct rddma_source **src, FILE *fp)
{
	struct rddma_source *h = malloc(sizeof(*h));
	*src = h;

	if (fp == NULL)
		return -EINVAL;

	if (h == NULL)
		return -ENOMEM;

	h->f = get_file;
	h->h = (void *)fp;

	return 0;
}

void *rddma_find_npc(struct rddma_npc *elems, char *name)
{
	struct rddma_npc *l;
	int size = strlen(name);

	for (l = elems; l; l = l->next) {
		if ((size == l->size) && !strcmp(l->name,name)) {
			return l->e;
		}
	}
	return NULL;
}

void *rddma_find_func(struct rddma_dev *dev, char *name)
{
	void *ret;
	ret = rddma_find_npc(dev->funcs,name);
	return ret;
}

void *rddma_find_map(struct rddma_dev *dev, char *name)
{
	void * ret;
	ret = rddma_find_npc(dev->maps,name);
	return ret;
}

void *rddma_find_event(struct rddma_dev *dev, char *name)
{
	void *ret;
	ret = rddma_find_npc(dev->events,name);
	return ret;
}

/* Register closures in global lists, eventually dev? */
void *rddma_register_npc(struct rddma_npc **elems, char *name, void *e)
{
	struct rddma_npc *l;
	if (rddma_find_npc(*elems,name))
		return 0;
	l = calloc(1,sizeof(*l)+strlen(name)+1);
	l->size = strlen(name);
	strcpy(l->b,name);
	l->name = l->b;
	l->e = e;
	l->next = *elems;
	*elems = l;
	return e;
}

void *rddma_register_map(struct rddma_dev *dev,char *name, void *e)
{
	return rddma_register_npc(&dev->maps,name,e);
}

void *rddma_register_func(struct rddma_dev *dev,char *name, void *e)
{
	return rddma_register_npc(&dev->funcs,name,e);
}

void *rddma_register_event(struct rddma_dev *dev, char *name, void *e)
{
	return rddma_register_npc(&dev->events,name,e);
}

int rddma_get_cmd(struct rddma_source *src, char **command)
{
	if (*command)
		free(*command);

	return (src->f(src->h,command) > 0);
}

/* Find by name and execute an internal command. If we make internal
 * commands closures then we can do more than just pass them the
 * command string... */
void *rddma_find_cmd(struct rddma_dev *dev, struct rddma_cmd_elem *commands, char *buf, int sz)
{
	struct rddma_cmd_elem *cmd;
	char *term;
	term = strstr(buf,"://");
	
	if (term) {
		int size = term - buf;
		
		for (cmd = commands;cmd && cmd->f; cmd = cmd->next)
			if (size == cmd->size && !strncmp(buf,cmd->cmd,size))
				return cmd->f(dev,term+3);
	}
	return 0;
}

void *rddma_find_pre_cmd(struct rddma_dev *dev, char *buf, int sz)
{
	return rddma_find_cmd(dev,dev->pre_commands,buf,sz);
}

void *rddma_find_post_cmd(struct rddma_dev *dev, char *buf, int sz)
{
	return rddma_find_cmd(dev,dev->post_commands,buf,sz);
}

/* 
 * Register commands to the global lists... should eventually pass dev
 * or something... */
int register_pre_cmd(struct rddma_dev *dev, char *name, void **(*f)(struct rddma_dev *,char *))
{
	struct rddma_cmd_elem *c;
	int len = strlen(name);
	c = calloc(1,sizeof(*c)+len+1);
	strcpy(c->b,name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = dev->pre_commands;
	dev->pre_commands = c;
}

int register_post_cmd(struct rddma_dev *dev, char *name, void **(*f)(struct rddma_dev *,char *))
{
	struct rddma_cmd_elem *c;
	int len = strlen(name);
	c = calloc(1,sizeof(*c)+len+1);
	strcpy(c->b,name);
	c->cmd = c->b;
	c->size = len;
	c->f = f;
	c->next = dev->post_commands;
	dev->post_commands = c;
}

int rddma_open(struct rddma_dev **device, char *dev_name, int timeout)
{
	struct rddma_dev *dev = malloc(sizeof(struct rddma_dev));

	*device = dev;
	if (dev == NULL)
		return -ENOMEM;

	dev->to = timeout;
	dev->fd = open(dev_name ? dev_name : "/dev/rddma", (O_NONBLOCK | O_RDWR));

	if ( dev->fd < 0 ) {
		perror("failed");
		free(dev);
		return (0);
	}

	dev->file = fdopen(dev->fd,"r+");

	return 0;
}

void rddma_close(struct rddma_dev *dev)
{
	close(dev->fd);
	free(dev);
}

int rddma_get_option(char *str, char *name)
{
	if (str && name)
		return (strstr(str,name) != NULL);
	return 0;
}

int rddma_get_str_arg(char *str, char *name, char **val)
{
	char *var = NULL;
	
	*val = NULL;

	if (str)
		if ((var = strstr(str,name))) {
			char *arg;
			arg = var + strlen(name);
			if (*arg == '(') {
				sscanf(arg+1,"%a[^)]",val);
				return 1;
			}
			return 0;
		}

	return -1 ;
}

int rddma_get_long_arg(char *str, char *name, long *value, int base)
{
	char *val = NULL;
	int ret;
	ret = rddma_get_str_arg(str,name,&val);
	if(ret > 0) {
		*value = strtoul(val,0,base);
		free(val);
		return ret;
	}
	return 0;
}

long rddma_get_hex_arg(char *str, char *name)
{
	long val;
	if (rddma_get_long_arg(str,name,&val,16))
		return val;
	return -1;
}

long rddma_get_dec_arg(char *str, char *name)
{
	long val;
	if (rddma_get_long_arg(str,name,&val,10))
		return val;
	return -1;
}

int rddma_poll_read(struct rddma_dev *dev)
{
	struct pollfd fd = {dev->fd,POLLIN,0};
	return poll(&fd,1,dev->to);
}

int rddma_get_result(struct rddma_dev *dev, char **result)
{
	int ret;

	ret = fscanf(dev->file,"%a[^\n]\n",result);

	while (ret <= 0 && ferror(dev->file)){
		clearerr(dev->file);

		ret = rddma_poll_read(dev);
		if (ret <= 0)
			return ret;

		ret = fscanf(dev->file,"%a[^\n]\n",result);
	}
	return ret;
}

int rddma_invoke_cmd_ap(struct rddma_dev *dev, char *f, va_list ap)
{
	int ret;
	ret = vfprintf(dev->file,f,ap);
	fflush(dev->file);
	return ret;
}

int rddma_invoke_cmd(struct rddma_dev *dev, char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap,f);
	ret = rddma_invoke_cmd_ap(dev,f,ap);
	va_end(ap);

	return ret;
}

int rddma_invoke_cmd_str(struct rddma_dev *dev, char *cmd, int size)
{
	int ret;
	if (size) 
		ret = fwrite(cmd,size,1,dev->file);
	else 
		ret = fprintf(dev->file,"%s\n",cmd);

	fflush(dev->file);
	return ret;

}

int rddma_do_cmd_ap(struct rddma_dev *dev, char **result, char *f, va_list ap)
{
	int ret;
	ret = rddma_invoke_cmd_ap(dev,f,ap);
	if (ret < 0)
		return ret;

	ret = rddma_get_result(dev,result);
	return ret;
}

int rddma_do_cmd(struct rddma_dev *dev, char **result,  char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap,f);
	ret = rddma_do_cmd_ap(dev,result,f,ap);
	va_end(ap);
	return ret;
}

int rddma_do_cmd_str(struct rddma_dev *dev, char **result, char *cmd, int size)
{
	int ret;
	ret = rddma_invoke_cmd_str(dev,cmd,size);
	if (ret < 0)
		return ret;
	ret = rddma_get_result(dev,result);
	return ret;
}

struct rddma_async_handle {
	char *result;
	void *e;
	struct rddma_async_handle *c;
	sem_t sem;
};

void *rddma_alloc_async_handle(void *e)
{
	struct rddma_async_handle *handle = calloc(1,sizeof(*handle));
	
	if (handle) {
		handle->c = (void *)handle;
		handle->e = e;
		sem_init(&handle->sem,0,0);
	}
	return (void *)handle;
}

int rddma_get_async_handle(void *h, char **result, void **e)
{
	struct rddma_async_handle *handle = (struct rddma_async_handle *)h;
	if (handle->c == handle) {
		sem_wait(&handle->sem);
		*result = handle->result;
		*e = handle->e;
		return 0;
	}
	return -EINVAL;
}

int rddma_set_async_handle(void *h, void *e)
{
	struct rddma_async_handle *handle = (struct rddma_async_handle *)h;
	if (handle->c == handle) {
		handle->e = e;
		return 0;
	}
	return -EINVAL;
}

int rddma_free_async_handle(void *h)
{
	struct rddma_async_handle *handle = (struct rddma_async_handle *)h;
	if (handle->c == handle) {
		sem_destroy(&handle->sem);
		free(handle);
		return 0;
	}
	return -EINVAL;
}

int rddma_put_async_handle(struct rddma_dev *dev)
{
	int ret;
	char *result = NULL;
	struct rddma_async_handle *handle = NULL;

	ret = rddma_get_result(dev,&result);
	if (ret < 0)
		return ret;

	handle = (struct rddma_async_handle *)rddma_get_hex_arg(result,"reply");

	if (handle && (handle->c == handle)) {
		handle->result = result;
		sem_post(&handle->sem);
		return 0;
	}
	
out:
	ret = -EINVAL;
	free(result);
	return ret;
}

int rddma_get_eventfd(int count)
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

void asyio_prep_preadv(struct iocb *iocb, int fd, struct iovec *iov, int nr_segs,
		       int64_t offset, int afd)
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

void asyio_prep_pwritev(struct iocb *iocb, int fd, struct iovec *iov, int nr_segs,
			int64_t offset, int afd)
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
	iocb->aio_buf = (u_int64_t) (unsigned long) buf;
	iocb->aio_nbytes = nr_segs;
	/* Address of string for rddma reply */
	iocb->aio_offset = (u_int64_t) (unsigned long) malloc(256);
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = afd;
}


int waitasync(int afd, int timeout)
{
	struct pollfd fd = {afd,POLLIN,0};
	return poll(&fd,1,timeout);
}

