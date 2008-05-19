#include <stdio.h>
#include <sys/mman.h>
#include <vfi_api.h>
#include <vfi_frmwrk.h>
#include <vfi_log.h>
#include <assert.h>

#define MY_ERROR VFI_DBG_DEFAULT
#define MY_DEBUG (VFI_DBG_EVERYONE | VFI_DBG_EVERYTHING | VFI_LOG_DEBUG)

static int bind_create_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	int err = 0;
	long rslt;
	void *payload;
	struct {void *f; char *src_evt_name; char *dest_evt_name; char *src_loc; char *dest_loc;} *p = e;

	if (vfi_get_dec_arg(result,"result",&rslt)) {
#warning TODO: Change error number? Fatal error!
		err = -EIO;
		goto done;
	}

	if (rslt) {
		err = rslt;
		goto done;
	}

	if (err = vfi_register_event(dev,p->src_evt_name,p->src_loc))
		goto done;

#warning Change event storing? Same name on different locations gives a conflict
#if 0
	if (err = vfi_register_event(dev,p->dest_evt_name,p->dest_loc))
		vfi_unregister_event(dev,p->src_evt_name, &payload);
	MARK;printf("err = %d\n",err);
#endif
 done:
	if (err) {
		free(p->src_loc); free(p->dest_loc);
	}
	free(p->src_evt_name); free(p->dest_evt_name);
	free(p);
	vfi_set_async_handle(ah,NULL);
	
	assert(err <= 0);
	return VFI_RESULT(err);
}

int bind_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **command)
{
	/* bind_create://x.xl.f/d.dl.f?event_name(dn)=s.sl.f?event_name(sn) */
	
	int err = 0;
	char *cmd; char *xfer; char *dest; char *src;
	struct {void *f; char *src_evt_name; char *dest_evt_name; char *src_loc; char *dest_loc;} *e = calloc(1,sizeof(*e));
	if (e) {
		vfi_parse_ternary_op(*command, &cmd, &xfer, &dest, &src);

		if (vfi_get_str_arg(src,"event_name",&e->src_evt_name) != 1) {
			err = -EINVAL;
			goto error;
		}

		if (vfi_get_str_arg(dest,"event_name",&e->dest_evt_name) != 1) {
			err = -EINVAL;
			goto error;
		}
		
		if (err = vfi_get_location(src,&e->src_loc))
			goto error;
		
		if (err = vfi_get_location(dest,&e->dest_loc))
			goto error;

		e->f = bind_create_closure;		

		free(cmd);free(xfer);free(dest);free(src);
		free(vfi_set_async_handle(ah,e));
		return 0;

	error:
		free(cmd); free(xfer); free(dest); free(src);
		free(e->src_evt_name); free(e->dest_evt_name); free(e->src_loc); free(e->dest_loc);
		free(e);
		assert(err < 0);
		return VFI_RESULT(err);
	}

	return VFI_RESULT(-ENOMEM);
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
	return VFI_RESULT(err);
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
	//void **e;

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
	long rslt;
	int rc;

	rc = vfi_get_dec_arg(result,"result",&rslt);
	if (!rc && !rslt) {
		rc = vfi_get_name_location(result,&name,&location);
		if (!rc) {
			vfi_register_event(dev,name,location);
			free(name);
		}
	}

	free(vfi_set_async_handle(ah,NULL));
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
		return VFI_RESULT(-EIO);
	if (rslt)
		return 1;
	free(vfi_set_async_handle(ah,NULL));
	return 0;
}

int location_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* location_find://name.location?wait */

	int err = 0;

	if (vfi_get_option(*cmd,"wait")) {
		struct {void *f;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f = location_find_closure;
			free(vfi_set_async_handle(ah,e));
		}
		else
			err = -ENOMEM;

	}

	return VFI_RESULT(err);
}

static int sync_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(result,"result",&rslt);
	if (rc)
		return VFI_RESULT(-EIO);
	if (rslt)
		return 1;
	free(vfi_set_async_handle(ah,NULL));
	return 0;
}

int sync_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	int err = 0;
	/* sync_find://name.location?wait */
	if (vfi_get_option(*cmd,"wait")) {
		struct {void *f;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f = sync_find_closure;
			free(vfi_set_async_handle(ah,e));
		}
		else
			err = -ENOMEM;
	}

	return VFI_RESULT(err);
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
	int err = 0;
	long rslt = 0;

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
		err = -EINVAL;
		goto done;
	}

	pipe = calloc(numpipe-numevnts,sizeof(void *));
	if (err = vfi_find_func(dev,elem[func],&pipe[0]))
		goto done;

	for (i = 0; i< numimaps;i++)
		if (err = vfi_find_map(dev,elem[i],(struct vfi_map **)&pipe[i+2]))
			goto done;

	for (i = 0; i< numomaps;i++)
		if (err = vfi_find_map(dev,elem[events+i+1],(struct vfi_map **)&pipe[func+i+2]))
			goto done;
	
	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numomaps & 0xff) << 8));
	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);
	
	if (numevnts > 1) {
		for (i = 0; i< numevnts-1;i++) {
			if (err = vfi_find_event(dev,elem[func+i+1],(void **)&eloc))
				goto done;

			vfi_invoke_cmd(dev,"event_chain://%s.%s?request(%p),event_name(%s)\n",
				       elem[func+i+1],
				       eloc,
				       ah,
				       elem[func+i+2]);
			vfi_wait_async_handle(ah,&result,&e);
			if (vfi_get_dec_arg(result,"result",&rslt)) {
#warning TODO: Fatal error.
				err = -EIO;
				goto done;
			}
			if (rslt) {
#warning TODO: Add code to remove chain		
				err = rslt;
				goto done;
			}
		}
	}
	
	free(*command);
	*command = malloc(128);
	if (*command == NULL) {
		err = -ENOMEM;
		goto done;
	}
	
	if (err = vfi_find_event(dev,elem[func+1],(void **)&eloc))
		goto done;
	
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
	}

	return VFI_RESULT(err);
}

int quit_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* quit or quit:// */
	vfi_set_dev_done(dev);
	return 1;
}

int map_init_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* map_init://name#o:e?value(x) */
	/* map_init://name#o:e?pattern(counting) */
	char *name;
	char *location;
	long long offset;
	long extent;
	long val;
	char *pattern = NULL;
	struct vfi_map *map;
	long *mem;
	int err = 0;

	if (err = vfi_get_name_location(*cmd, &name, &location))
		goto done;
	if (vfi_get_offset(*cmd,&offset)) /* Offset defaults to 0 */
		offset = 0;
	if (vfi_get_hex_arg(*cmd,"value",&val))
		if (vfi_get_str_arg(*cmd,"pattern",&pattern) != 1) {
			err = -EINVAL;
			goto done;
		}
	if (err = vfi_find_map(dev,name,&map))
		goto done;
	if (vfi_get_extent(*cmd,&extent)) /* Extent defaults to map's extent */
		extent = map->extent;
	if (map->extent < offset + extent) {
		err = -EINVAL;
		goto done;
	}

	mem = (long *)map->mem + offset;
	extent = extent >> 2;

	if (pattern)
		if (strstr(pattern,"counting")) {
			val = 1;
			while  (extent--)
				*mem++ = val++;			
		}
		else
			err = -EINVAL;
		
	else
		while  (extent--)
			*mem++ = val;

done:
	free(location);
	free(name);
	free(pattern);
	if (err)
		return VFI_RESULT(err);
	return 1;
}

int map_check_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* map_check://name#o:e?value(x) */
	/* map_check://name#o:e?pattern(counting) */
	char *name;
	char *location;
	long long offset;
	long extent;
	long val;
	char *pattern = NULL;
	struct vfi_map *map;
	long *mem;
	int err = 0;

	if (err = vfi_get_name_location(*cmd, &name, &location))
		goto done;
	if (vfi_get_offset(*cmd,&offset)) /* Offset defaults to 0 */
		offset = 0;
	if (vfi_get_hex_arg(*cmd,"value",&val))
		if (vfi_get_str_arg(*cmd,"pattern",&pattern) != 1) {
			err = -EINVAL;
			goto done;
		}
	if (err = vfi_find_map(dev,name,&map))
		goto done;
	if (vfi_get_extent(*cmd,&extent)) /* Extent defaults to map's extent */
		extent = map->extent;
	if (map->extent < offset + extent) {
		err = -EINVAL;
		goto done;
	}

	mem = (long *)map->mem + offset;
	extent = extent >> 2;

	if (pattern)
		if (strstr(pattern,"counting")) {
			val = 1;
			while  (extent--)
				if (*mem++ != val++) {
					err = -EBADMSG;
					break;
				}
					
		}
		else
			err = -EINVAL;
		
	else
		while  (extent--)
			if (*mem++ != val) {
				err = -EBADMSG;
				break;
			}

done:
	free(location);
	free(name);
	free(pattern);
	if (err) {
		printf("%s: Map has ERRORS \n", __func__);
		return VFI_RESULT(err);
	}
	else
		printf("%s: Map is OK\n", __func__);
	return 1;
}
