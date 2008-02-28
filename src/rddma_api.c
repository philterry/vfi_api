#include "rddma_api.h"
#include <poll.h>
#include <stdarg.h>
#include <semaphore.h>

struct rddma_dev *rddma_open(char *dev_name, int timeout)
{
	struct rddma_dev *dev = malloc(sizeof(struct rddma_dev));

	dev->to = timeout;
	dev->fd = open(dev_name ? dev_name : "/dev/rddma", (O_NONBLOCK | O_RDWR));

	if ( dev->fd < 0 ) {
		perror("failed");
		free(dev);
		return (0);
	}

	dev->file = fdopen(dev->fd,"r+");

	return dev;
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
	struct rddma_async_handle *c;
	sem_t sem;
};

void *rddma_alloc_async_handle()
{
	struct rddma_async_handle *handle = calloc(1,sizeof(*handle));
	
	handle->c = (void *)handle;
	sem_init(&handle->sem,0,0);

	return (void *)handle;
}

int rddma_get_async_handle(void *h, char **result)
{
	struct rddma_async_handle *handle = (struct rddma_async_handle *)h;
	if (handle->c == handle) {
		sem_wait(&handle->sem);
		*result = handle->result;
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

int rddma_get_result_async(struct rddma_dev *dev)
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

