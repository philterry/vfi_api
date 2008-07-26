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
		err = -EIO;
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		goto done;
	}

	if (rslt) {
		err = rslt;
		vfi_log(VFI_LOG_ERR, "%s: Command failed with error %d (%s)", __func__, err, result);
		goto done;
	}

	if (err = vfi_register_event(dev,p->src_evt_name,p->src_loc)) {
		vfi_log(VFI_LOG_ERR, "%s: Failed to register event. Error is %d", __func__, err);
		goto done;
	}

	if (err = vfi_register_event(dev,p->dest_evt_name,p->dest_loc))
		vfi_unregister_event(dev,p->src_evt_name, &payload);

 done:
	if (err) {
		free(p->src_loc); free(p->dest_loc);
	}
	free(p->src_evt_name); free(p->dest_evt_name);
	free(vfi_set_async_handle(ah,NULL));
	
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
		char *src_name, *dest_name;
		vfi_parse_ternary_op(*command, &cmd, &xfer, &dest, &src);

		if (vfi_get_str_arg(src,"event_name",&src_name) != 1) {
			err = -EINVAL;
			vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Source event name not specified (%s).", __func__, *cmd);
			goto error;
		}

		if (vfi_get_str_arg(dest,"event_name",&dest_name) != 1) {
			err = -EINVAL;
			vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Destination event name not specified (%s).", __func__, *cmd);
			goto error;
		}
		
		if (err = vfi_get_location(src,&e->src_loc)) {
			vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Source location not specified (%s).", __func__, *cmd);
			goto error;
		}
		
		if (err = vfi_get_location(dest,&e->dest_loc)) {
			vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Destination location not specified (%s).", __func__, *cmd);
			goto error;
		}

		e->src_evt_name = malloc(strlen(src_name) + strlen(e->src_loc) + 2);
		if (e->src_evt_name == NULL) {
			vfi_log(VFI_LOG_ERR, "%s: Error creating src_evt_name (%s).", __func__, *cmd);
			err = -ENOMEM;
			goto error;
		}

		e->dest_evt_name = malloc(strlen(dest_name) + strlen(e->dest_loc) + 2);
		if (e->dest_evt_name == NULL) {
			vfi_log(VFI_LOG_ERR, "%s: Error creating dest_evt_name (%s).", __func__, *cmd);
			err = -ENOMEM;
			goto error;
		}

		sprintf(e->src_evt_name,"%s.%s",src_name,e->src_loc);
		sprintf(e->dest_evt_name,"%s.%s",dest_name,e->dest_loc);

		e->f = bind_create_closure;		

		free(cmd);free(xfer);free(dest);free(src);
		free(src_name);free(dest_name);
		free(vfi_set_async_handle(ah,e));
		return VFI_RESULT(0);

	error:
		free(cmd); free(xfer); free(dest); free(src);
		free(src_name);free(dest_name);
		free(e->src_evt_name); free(e->dest_evt_name); free(e->src_loc); free(e->dest_loc);
		free(e);
		assert(err < 0);
		return VFI_RESULT(err);
	}

	vfi_log(VFI_LOG_ERR, "%s: Failed to allocate memory. Error is %d", __func__, -ENOMEM);
	return VFI_RESULT(-ENOMEM);
}

static int mmap_create_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
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
		free(vfi_set_async_handle(ah,NULL));
	}
	return 0;
}

int mmap_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	char *name;
	int err = 0;
	if (vfi_get_str_arg(*cmd,"map_name",&name) > 0) {
		struct vfi_map *e;
		if (err = vfi_alloc_map(&e,name))
			vfi_log(VFI_LOG_ERR, "%s: Failed to allocate map. Error is %d", __func__, err);
		else {
			e->f = mmap_create_closure;
			if (err = vfi_get_extent(*cmd,&e->extent)) {
				vfi_log(VFI_LOG_ERR, "%s: Parse error. Extent not found. Error is %d", __func__, err);
				free(e);
			}
			else
				free(vfi_set_async_handle(ah,e));
		}
		free(name);
	}
	return VFI_RESULT(err);
}

static int smb_name_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	struct {void *f; char *name; long address; char **cmd;} *p = e;
	struct vfi_map *me;
	vfi_alloc_map(&me,p->name);
 	vfi_get_extent(result,&me->extent);
	me->mem = (char *)p->address;
	vfi_register_map(dev,me->name,me);
	free(p->name);
	free(vfi_set_async_handle(ah,NULL));
	return 0;
}

static int smb_create_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	int i;
	char *smb;
	struct {void *f; char *name; char **cmd;} *p = e;
	struct vfi_map *me;
	vfi_alloc_map(&me,p->name);
	sscanf(result+strlen("smb_create://"),"%a[^?]",&smb);
	sprintf(*p->cmd,"mmap_create://%s?map_name(%s)",smb,p->name);
	free(smb);
	me->f = mmap_create_closure;
 	vfi_get_extent(result,&me->extent);
	free(p->name);
	free(vfi_set_async_handle(ah,me));
	return 0;
}

int smb_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* smb_create://smb.loc.f#off:ext?map_name(name),map_address(address) */
	char *name;
	char *result = NULL;
	unsigned long address;
	struct vfi_map *map = NULL;
	int named, sourced;

	named = vfi_get_str_arg(*cmd,"map_name",&name) > 0;
	sourced = vfi_get_hex_arg(*cmd,"map_address", &address) == 0;
	if (named)
		vfi_find_map(dev,name,&map);

	if (!map && named) 
		if (!sourced) {
			struct {void *f; char *name; char **cmd;} *e = calloc(1,sizeof(*e));
			if (e) {
				e->f =smb_create_closure;
				e->name = name;
				e->cmd = cmd;
				free(vfi_set_async_handle(ah,e));
				return 0;
			}
			free(name);
			return -ENOMEM;
		}
		else {
			struct {void *f; char *name; long address; char **cmd;} *e = calloc(1,sizeof(*e));
			if (e) {
				char *old_cmd = *cmd;
				*cmd = malloc(strlen(old_cmd)+64);
				if (*cmd) {
					sprintf(*cmd,"%s,mytid(%d)",old_cmd,getpid());
					free(old_cmd);
					e->f =smb_name_closure;
					e->name = name;
					e->address = address;
					e->cmd = cmd;
					free(vfi_set_async_handle(ah,e));
					return 0;
				}
				*cmd = old_cmd;
			}
			free(name);
			return -ENOMEM;
		}
	else if (map && !sourced) {
		/* add map_address from map */
		char *old_cmd = *cmd;
		*cmd = malloc(strlen(old_cmd)+96);
		if (*cmd) {
			sprintf(*cmd,"%s,map_address(%lx),map_extent(%lx),mytid(%d)",old_cmd,map->mem,map->extent,getpid());
			free(old_cmd);
			return 0;
		}
		*cmd = old_cmd;
		return -ENOMEM;
	}

	return 0;
}

int map_install_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* map_install://fred:4000 */
	char *name = NULL;
	char *location = NULL;
	long extent;
	int ret;
	struct vfi_map *map;

	ret = vfi_get_name_location(*cmd,&name,&location);
	if (ret)
		goto out;

	free(location);

	ret = vfi_get_extent(*cmd,&extent);
	if (ret)
		goto name;

	ret = vfi_alloc_map(&map,name);
	if (ret)
		goto name;

	map->mem = malloc(extent);
	if (map->mem == NULL)
		goto mem;

	map->extent = extent;

	ret = vfi_register_map(dev,name,map);
	if (ret)
		goto map;

	return 1;

map:
	free(map->mem);
mem:
	free(map);
name:
	free(name);
out:
	return ret;
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

static int wait_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char *result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(result,"result",&rslt);
	if (rc) {
		vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
		return VFI_RESULT(-EIO);
	}
	if (rslt) {
		sleep(1);
		return 1;
	}
	free(vfi_set_async_handle(ah,NULL));
	return 0;
}

int wait_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	int err = 0;
	if (vfi_get_option(*cmd,"wait")) {
		struct {void *f;} *e = calloc(1,sizeof(*e));
		if (e) {
			e->f = wait_closure;
			free(vfi_set_async_handle(ah,e));
		}
		else {
			err = -ENOMEM;
			vfi_log(VFI_LOG_ERR, "%s: Failed to allocate memory. Error is %d", __func__, err);

		}
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
	int numin = 0;
	int numout = 0;
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
		vfi_log(VFI_LOG_ERR, "%s: 0 events specified. Error is %d", __func__, elem[i], err);
		goto done;
	}

	pipe = calloc(numpipe-numevnts,sizeof(void *));
	if (!pipe) {
		err = -ENOMEM;
		vfi_log(VFI_LOG_ERR, "%s: Failed to allocate memory. Error is %d", __func__, err);
	}

	if (err = vfi_find_func(dev,elem[func],&pipe[0], &numin, &numout)) {
		vfi_log(VFI_LOG_ERR, "%s: Failed to lookup function %s. Error is %d", __func__, elem[func], err);
		goto done;
	}

	if (numin >= 0 && numin != numimaps) {
		err = -EINVAL;
		vfi_log(VFI_LOG_ERR, "%s: Number of input maps (%d) differs from expected (%d) for function %s",
			__func__, numimaps, numin, elem[func]);
		goto done;
	}

	if (numout >= 0 && numout != numomaps) {
		err = -EINVAL;
		vfi_log(VFI_LOG_ERR, "%s: Number of output maps (%d) differs from expected (%d) for function %s",
			__func__, numomaps, numout, elem[func]);
		goto done;
	}

	for (i = 0; i< numimaps;i++)
		if (err = vfi_find_map(dev,elem[i],(struct vfi_map **)&pipe[i+2])) {
			vfi_log(VFI_LOG_ERR, "%s: Failed to find map %s. Error is %d", __func__, elem[i], err);
			goto done;
		}

	for (i = 0; i< numomaps;i++)
		if (err = vfi_find_map(dev,elem[events+i+1],(struct vfi_map **)&pipe[func+i+2])) {
			vfi_log(VFI_LOG_ERR, "%s: Failed to find map %s. Error is %d", __func__, elem[events+i+1], err);
			goto done;
		}

	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numomaps & 0xff) << 8));
	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);

	if (numevnts > 1) {
		for (i = 0; i< numevnts-1;i++) {
			if (err = vfi_find_event(dev,elem[func+i+1],(void **)&eloc)) {
				vfi_log(VFI_LOG_ERR, "%s: Failed to lookup event %s. Error is %d", __func__, elem[func+i+1], err);
				goto done;
			}

			vfi_invoke_cmd(dev,"event_chain://%s?request(%p),event_name(%.*s)\n",
				       elem[func+i+1],
				       ah,
				       elem[func+i+2],strcspn(elem[func+i+2],"."));
			vfi_wait_async_handle(ah,&result,&e);
			if (vfi_get_dec_arg(result,"result",&rslt)) {
				err = -EIO;
				vfi_log(VFI_LOG_EMERG, "%s: Fatal error. Result string not returned from driver", __func__);
				goto done;
			}
			if (rslt) {
#warning TODO: Add code to remove chain		
				err = rslt;
				vfi_log(VFI_LOG_ERR, "%s: Command failed with error %ld (%s)", __func__, rslt, result);
				goto done;
			}
		}
	}
	
	free(*command);
	*command = malloc(128);
	if (*command == NULL) {
		err = -ENOMEM;
		vfi_log(VFI_LOG_ERR, "%s: Failed to allocate memory. Error is %d", __func__, err);
		goto done;
	}
	
	i = snprintf(*command,128,"event_start://%s",elem[func+1]);
	if (i >= 128) {
		*command = realloc(*command,i+1);
		snprintf(*command,128,"event_start://%s",elem[func+1]);
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

	if (err = vfi_get_name_location(*cmd, &name, &location)) {
		vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Name and location not found (%s).", __func__, *cmd);
		goto done;
	}
	if (vfi_get_offset(*cmd,&offset)) /* Offset defaults to 0 */
		offset = 0;
	if (vfi_get_hex_arg(*cmd,"value",&val))
		if (vfi_get_str_arg(*cmd,"pattern",&pattern) != 1) {
			err = -EINVAL;
			vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Value or pattern not found (%s)", __func__, *cmd);
			goto done;
		}
	if (err = vfi_find_map(dev,name,&map)) {
		vfi_log(VFI_LOG_ERR, "%s: Failed to lookup map %s. Error is %d", __func__, name, err);		
		goto done;
	}
	if (vfi_get_extent(*cmd,&extent)) /* Extent defaults to map's extent */
		extent = map->extent;
	if (map->extent < offset + extent) {
		err = -EINVAL;
		vfi_log(VFI_LOG_ERR, "%s: Offset and extent combination larger than maps extent. Error is %d", __func__, err);
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
		else {
			err = -EINVAL;
			vfi_log(VFI_LOG_ERR, "%s: Illegal pattern (%s). Error is %d", __func__, pattern, err);
		}
		
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

	if (err = vfi_get_name_location(*cmd, &name, &location)) {
		vfi_log(VFI_LOG_ERR, "%s: Error parsing string. Name and location not found (%s).", __func__, *cmd);
		goto done;
	}
	if (vfi_get_offset(*cmd,&offset)) /* Offset defaults to 0 */
		offset = 0;
	if (vfi_get_hex_arg(*cmd,"value",&val))
		if (vfi_get_str_arg(*cmd,"pattern",&pattern) != 1) {
			err = -EINVAL;
			vfi_log(VFI_LOG_ERR, "%s: Error parsing command string. Value or pattern not found (%s)", __func__, *cmd);
			goto done;
		}
	if (err = vfi_find_map(dev,name,&map)) {
		vfi_log(VFI_LOG_ERR, "%s: Failed to lookup map %s. Error is %d", __func__, name, err);		
		goto done;
	}
	if (vfi_get_extent(*cmd,&extent)) /* Extent defaults to map's extent */
		extent = map->extent;
	if (map->extent < offset + extent) {
		err = -EINVAL;
		vfi_log(VFI_LOG_ERR, "%s: Offset and extent combination larger than maps extent. Error is %d", __func__, err);
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
					vfi_log(VFI_LOG_ERR, "%s: Counting pattern has errors", __func__);
					break;
				}
					
		}
		else {
			err = -EINVAL;
			vfi_log(VFI_LOG_ERR, "%s: Illegal pattern (%s). Error is %d", __func__, pattern, err);
		}
		
	else
		while  (extent--)
			if (*mem++ != val) {
				err = -EBADMSG;
				vfi_log(VFI_LOG_ERR, "%s: Repeating value pattern has errors", __func__);
				break;
			}

done:
	free(location);
	free(name);
	free(pattern);
	if (err) {
		VFI_DEBUG (MY_DEBUG, "%s: Map has ERRORS\n", __func__);
		return VFI_RESULT(err);
	}
	else
		VFI_DEBUG (MY_DEBUG, "%s: Map is OK\n", __func__);
	return 1;
}
