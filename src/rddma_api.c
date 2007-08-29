#include "rddma_api.h"
#include <stdio.h>

int rddma_smb_create(char *desc)
{
	char buffer[128];
	int ret;

	FILE *fp = fopen("/dev/rddma", "rw");

	if (NULL == fp)
		return -1;

	fprintf(fp,"smb_create://%s",desc);

	fscanf(fp,"[%d]%s",&ret,buffer);
	
	fclose(fp);

	printf("smb_create://%s returns [%d]%s\n",desc,ret,buffer);

	return ret;
}
