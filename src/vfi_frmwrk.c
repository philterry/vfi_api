#include <stdio.h>
#include <sys/mman.h>
#include <vfi_api.h>
#include <vfi_frmwrk.h>

int bind_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **command)
{
	/* bind_create://x.xl.f/d.dl.f?event_name(dn)=s.sl.f?event_name(sn) */

	char *cmd, *xfer, *dest, *src, *sl, *dl, *sen, *den;

	vfi_parse_ternary_op(*command, &cmd, &xfer, &dest, &src);

	free(cmd);free(xfer);

	vfi_get_str_arg(src,"event_name",&sen);
	vfi_get_str_arg(dest,"event_name",&den);

	vfi_get_location(src,&sl);
	vfi_get_location(dest,&dl);

	free(dest);free(src);

	vfi_register_event(dev,sen,sl);
	vfi_register_event(dev,den,dl);

	free(sen);free(den);

	return 0;
}

static int smb_mmap_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long offset;
	void *mem;
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_SHARED;

	struct vfi_map *p = e;
	if (!vfi_get_hex_arg(result,"mmap_offset",&offset)) {
		p->mem = mmap(0,p->extent,prot,flags,vfi_fileno(dev), offset);
		if (vfi_register_map(dev,p->name,e))
			printf("%s: register map failed\n", __func__);
		vfi_set_async_handle(ah,NULL);
	}
	return 0;
}

int smb_mmap_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	char *name;
	int err = 0;
	if (vfi_get_str_arg(*cmd,"map_name",&name) > 0) {
		struct vfi_map *e;
		if (err = vfi_alloc_map(&e,name))
			printf("Failed to allocate map\n");
		else {
			e->f = smb_mmap_closure;
			if (err = vfi_get_extent(*cmd,&e->extent))
				printf("Extent not found\n");
			else
				free(vfi_set_async_handle(ah,e));
		}
		free(name);
	}
	return err;
}

static int smb_create_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	int i;
	char *smb;
	struct {void *f; char *name; char **cmd;} *p = e;
	struct vfi_map *me;
	vfi_alloc_map(&me,p->name);
	sscanf(result+strlen("smb_create://"),"%a[^?]",&smb);
	sprintf(*p->cmd,"smb_mmap://%s?map_name(%s)\n",smb,p->name);
	free(smb);
	me->f = smb_mmap_closure;
 	vfi_get_extent(result,&me->extent);
	free(p->name);
	free(vfi_set_async_handle(ah,me));
	return 0;
}

int smb_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* smb_create://smb.loc.f#off:ext?map_name(name) */
	char *name;
	char *result = NULL;
	void **e;

	if (vfi_get_str_arg(*cmd,"map_name",&name) > 0) {
		struct {void *f; char *name; char **cmd;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f =smb_create_closure;
			e->name = name;
			e->cmd = cmd;
			free(vfi_set_async_handle(ah,e));
		}
	}
	return 0;
}

static int event_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	/* event_find://name.location */
	char *name;
	char *location;
	long err;
	int rc;

	rc = vfi_get_dec_arg(result,"result",&err);
	if (!err && !rc) {
		rc = vfi_get_name_location(result,&name,&location);
		if (!rc) {
			vfi_register_event(dev,name,location);
			free(name);
		}
	}
	return 0;
}

int event_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* event_find://name.location */
	struct {void *f;} *e = calloc(1,sizeof(*e));
	if (e) {
		e->f = event_find_closure;
		free(vfi_set_async_handle(ah,e));
	}

	return 0;
}

static int location_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(result,"result",&rslt);
	if (rc)
		return 0;

	return rslt;
}

int location_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* location_find://name.location?wait */
	if (vfi_get_option(*cmd,"wait")) {
		struct {void *f;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f = location_find_closure;
			free(vfi_set_async_handle(ah,e));
		}
	}

	return 0;
}

static int sync_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(result,"result",&rslt);
	if (rc)
		return 0;

	return rslt;
}

int sync_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* sync_find://name.location?wait */
	if (vfi_get_option(*cmd,"wait")) {
		struct {void *f;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f = sync_find_closure;
			free(vfi_set_async_handle(ah,e));
		}
	}

	return 0;
}

int pipe_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **command)
{
	/* pipe://[<inmap><]*<func>[(<event>[,<event>]*)][><omap>]*  */

	char *sp;
	char *iter;
	int size = 0;
	int i = 0;
	int numpipe = 0;
	int outmaps = 0;
	int events = 0;
	int func = 0;
	int found_func = 0;
	int inmaps = 0;
	int numimaps = 0;
	int numomaps = 0;
	int numevnts = 0;
	void **pipe = NULL;
	char *elem[20];
	int elem_cnt = 0;
	void *e;
	char *cmd;
	char *result = NULL;
	char *eloc;
	long err = 0;

	vfi_parse_unary_op(*command, &cmd, &sp);
	free(cmd);
	iter = sp;

	while (*iter) {
		if (sscanf(iter,"%a[^<>,()]%n",&elem[elem_cnt],&size) > 0) {
			switch (*(iter+size)) {
			case '<':
				inmaps = elem_cnt;
				break;
			case '(':
				func = elem_cnt;
				found_func = 1;
				break;
			case ')':
				events = elem_cnt;
				break;
			case ',':
				break;
			case '>':
				if (!found_func) {
					func = elem_cnt;
					found_func = 1;
				}
				outmaps = elem_cnt;
				break;

			default:
				if (!found_func) {
					func = elem_cnt;
					found_func = 1;
				}
				outmaps = elem_cnt;
				break;
			}
			elem_cnt++;
			iter += size;
		}
		else
			iter += 1;
	}

	free(sp);

	numpipe = elem_cnt + 1;
	numimaps = func;

	if (events)
		numevnts = events - func;
	else
		events = func;
	if (outmaps)
		numomaps = outmaps - events; 

	if (numevnts == 0) {
		errno = err = EINVAL;
		perror("Pipe has 0 events\n");
		goto done;
	}

	pipe = calloc(numpipe-numevnts,sizeof(void *));
	if (vfi_find_func(dev,elem[func],&pipe[0])) {
		errno = err = EINVAL;
		perror("Function not found");
		goto done;
	}

	for (i = 0; i< numimaps;i++) {
		if (vfi_find_map(dev,elem[i],(struct vfi_map **)&pipe[i+2])) {
			errno = err = EINVAL;
			perror("Map not found");
			goto done;
		}
	}

	for (i = 0; i< numomaps;i++) {
		if (vfi_find_map(dev,elem[events+i+1],(struct vfi_map **)&pipe[func+i+2])) {
			errno = err = EINVAL;
			perror("Map not found");
			goto done;
		}
	}
	
	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numomaps & 0xff) << 8));
	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);
	
	if (numevnts > 1) {
		for (i = 0; i< numevnts-1;i++) {
			if (vfi_find_event(dev,elem[func+i+1],(void **)&eloc)) {
				errno = err = EINVAL;
				perror("Event not found");
				goto done;
			}
			vfi_invoke_cmd(dev,"event_chain://%s.%s?request(%p),event_name(%s)\n",
				       elem[func+i+1],
				       eloc,
				       ah,
				       elem[func+i+2]);
			vfi_wait_async_handle(ah,&result,&e);
			err |= vfi_get_hex_arg(result, "result", &err);
			if (err) {
				errno = err;
				perror("Chain command failed");
#warning TODO: Add code to remove chain		       			       
				goto done;
			}
		}
	}
	
	free(*command);
	*command = malloc(128);
	if (*command == NULL) {
		errno = err = ENOMEM;
		perror("Memory allocation failed");
		goto done;
	}
	
	if (vfi_find_event(dev,elem[func+1],(void **)&eloc)) {
		errno = err = EINVAL;
		perror("Event not found");
		goto done;
	}
	
	i = snprintf(*command,128,"event_start://%s.%s",elem[func+1],eloc);
	if (i >= 128) {
		*command = realloc(*command,i+1);
		snprintf(*command,128,"event_start://%s.%s",elem[func+1],eloc);
	}
	
	free(vfi_set_async_handle(ah,pipe));
	
done:
	while (elem_cnt--)
		free(elem[elem_cnt]);

	if (err) {
		if (pipe)
			free(pipe);
		if (*command)
			free(*command);
		return 1;
	}

	return 0;
}

int quit_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* quit or quit:// */
	vfi_set_dev_done(dev);
	return 1;
}

int map_init_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* map_init://name#o:e?init_val(x) */
	char *name;
	char *location;
	long long offset;
	long extent;
	long val;
	struct vfi_map *map;
	long *mem;
	int rc;

	rc = vfi_get_name_location(*cmd, &name, &location);
	free(location);
	if (rc)
		goto out;
	if (vfi_get_offset(*cmd,&offset))
		goto out;
	if (vfi_get_extent(*cmd,&extent))
		goto out;
	if (vfi_get_hex_arg(*cmd,"init_val",&val))
		goto out;
	if (vfi_find_map(dev,name,&map))
		goto out;
	if (map->extent < offset + extent)
		goto out;

	free(name);

	mem = (long *)map->mem + offset;
	extent = extent >> 2;

	while  (extent--)
		*mem++ = val;

	return 1;
out:
	free(name);
	return 0;
}

