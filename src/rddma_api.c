#include "rddma_api.h"
#include <poll.h>
#include <stdarg.h>
#include <semaphore.h>

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

struct rddma_dev *rddma_open(char *dev_name, int flags)
{
	struct rddma_dev *dev = malloc(sizeof(struct rddma_dev));

	dev->fd = open(dev_name ? dev_name : "/dev/rddma",flags ? flags : O_RDWR);

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

long rddma_get_hex_option(char *str, char *name)
{
	char *opt;
	char *val;
	if ((opt = strstr(str,name)))
		if ((val = strstr(opt,"(")))
			return strtoul(val,0,16);
	return 0;
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

int rddma_poll_read(struct rddma_dev *dev, int timeout)
{
	struct pollfd fd = {dev->fd,POLLIN,0};
	return poll(&fd,1,timeout);
}

int rddma_do_cmd(struct rddma_dev *dev, char **result,  char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap,f);

	ret = vfprintf(dev->file,f,ap);
	if (ret < 0)
		goto out;

	ret = fflush(dev->file);
	if (ret < 0)
		goto out;

	ret = fscanf(dev->file,"%a[^\n]",result);
out:
	va_end(ap);
	return ret;
}

int rddma_do_cmd_blk(struct rddma_dev *dev, int timeout, char **result,  char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap,f);

	ret = vfprintf(dev->file,f,ap);
	if (ret < 0)
		goto out;

	ret = fflush(dev->file);
	if (ret < 0)
		goto out;

	ret = rddma_poll_read(dev,timeout);
	if (ret <= 0)
		goto out;

	ret = fscanf(dev->file,"%a[^\n]",result);
out:
	va_end(ap);
	return ret;
}

int rddma_invoke_cmd(struct rddma_dev *dev, char *f, ...)
{
	va_list ap;
	int ret;

	va_start(ap,f);

	ret = vfprintf(dev->file,f,ap);
	if (ret < 0)
		goto out;

	fflush(dev->file);
out:
	va_end(ap);
	return ret;
}

int rddma_get_result(struct rddma_dev *dev, int timeout, char **result)
{
	int ret;

	ret = fscanf(dev->file,"%a[^\n]\n",result);

	while (ret <= 0 && ferror(dev->file)){
		clearerr(dev->file);

		ret = rddma_poll_read(dev,timeout);
		if (ret <= 0)
			goto out;

		ret = fscanf(dev->file,"%a[^\n]\n",result);
	}
out:
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

int rddma_get_result_async(struct rddma_dev *dev, int timeout)
{
	int ret;
	char *reply = NULL;
	char *result = NULL;
	struct rddma_async_handle *handle = NULL;

	ret = fscanf(dev->file,"%a[^\n]\n",&result);

	while (ret <= 0 && ferror(dev->file)) {
		clearerr(dev->file);

		ret = rddma_poll_read(dev,timeout);
		if (ret <= 0)
			goto out;

		ret = fscanf(dev->file,"%a[^\n]\n",&result);
	}

	reply = strstr(result,"reply");
	if (reply == NULL) {
		ret = 0;
		goto out;
	}

	ret = sscanf(reply,"reply(%p)",(void *) &handle);
	if (ret < 1)
		goto out;

	if (handle && (handle->c == handle)) {
		handle->result = result;
		sem_post(&handle->sem);
	}
	else
		ret = -EINVAL;
out:
	return ret;
}
