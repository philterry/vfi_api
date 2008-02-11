#include "rddma_api.h"

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


char *rddma_call(struct rddma_dev *dev, char *cmd)
{
	char *output = NULL;
	fprintf(dev->file,"%s\n",cmd);
	fflush(dev->file);
 	fscanf(dev->file,"%a[^\n]", &output); 
	return output;
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


long waitasync(int afd, int timeo) {
	struct pollfd pfd;

	pfd.fd = afd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeo) < 0) {
		perror("poll");
		return -1;
	}
	if ((pfd.revents & POLLIN) == 0) {
		fprintf(stderr, "no results completed\n");
		return 0;
	}

	return 1;
}

