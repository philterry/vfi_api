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
		vfi_register_map(dev,p->name,e);
		vfi_set_async_handle(ah,NULL);
	}
	return 0;
}

int smb_mmap_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	char *name;
	if (vfi_get_str_arg(*cmd,"map_name",&name) > 0) {
		struct vfi_map *e;
		if (!vfi_alloc_map(&e,name)) {
			e->f = smb_mmap_closure;
			vfi_get_extent(*cmd,&e->extent);
			free(vfi_set_async_handle(ah,e));
		}
		free(name);
	}
	return 0;
}

static int smb_create_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	int i;
	char *smb;
	struct {void *f; char *name; char **cmd;} *p = e;
	struct vfi_map *me;
	vfi_alloc_map(&me,p->name);
	sscanf(*result+strlen("smb_create://"),"%a[^?]",&smb);
	sprintf(*p->cmd,"smb_mmap://%s?map_name(%s)\n",smb,p->name);
	free(smb);
	me->f = smb_mmap_closure;
	vfi_get_extent(*result,&me->extent);
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

static int event_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	/* event_find://name.location */
	char *name;
	char *location;
	long err;
	int rc;

	rc = vfi_get_dec_arg(*result,"result",&err);
	if (!err && !rc) {
		rc = vfi_get_name_location(*result,&name,&location);
		if (!rc) {
			vfi_register_event(dev,name,location);
			free(name);free(location);
		}
	}
	return 0;
}

int event_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* event_find://name.location */
	free(vfi_set_async_handle(ah,event_find_closure));
	return 0;
}

static int location_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(*result,"result",&rslt);
	if (rc)
		return 0;

	return rslt;
}

int location_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* location_find://name.location?wait */
	if (vfi_get_option(*cmd,"wait"))
		free(vfi_set_async_handle(ah,location_find_closure));

	return 0;
}

static int sync_find_closure(void *e, struct vfi_dev *dev, struct vfi_async_handle *ah, char **result)
{
	long rslt;
	int rc;
	rc = vfi_get_dec_arg(*result,"result",&rslt);
	if (rc)
		return 0;

	return rslt;
}

int sync_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* sync_find://name.location?wait */
	if (vfi_get_option(*cmd,"wait"))
		free(vfi_set_async_handle(ah,sync_find_closure));

	return 0;
}
/* The API pre-command to create and deliver a closure which executes
 * the named function on the named maps, invokes the chained events,
 * and is reinvoked on completion of the chained events. */
int pipe_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **command)
{
/* pipe://[<inmap><]*<func>[(<event>[,<event>]*)][><omap>]*  */

	char *sp;
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
	void **pipe;
	char *elem[20];
	void *e;
	char *cmd;
	char *result = NULL;
	char *eloc;

	vfi_parse_unary_op(*command, &cmd, &sp);
	free(cmd);

	while (*sp) {
		if (sscanf(sp," %a[^<>,()]%n",&elem[i],&size) > 0) {
			switch (*(sp+size)) {
			case '<':
				inmaps = i;
				break;
			case '(':
				func = i;
				found_func = 1;
				break;
			case ')':
				events = i;
				break;
			case ',':
				break;
			case '>':
				if (!found_func) {
					func = i;
					found_func = 1;
				}
				outmaps = i;
				break;

			default:
				if (!found_func) {
					func = i;
					found_func = 1;
				}
				outmaps = i;
				break;
			}
			i++;
			sp += size;
		}
		else
			sp += 1;
	}
	numpipe = i + 1;
	numimaps = func;

	if (events)
		numevnts = events - func;
	else
		events = func;
	if (outmaps)
		numomaps = outmaps - events; 

	if (numevnts == 0) {
		while (i--)
			free(elem[i]);
		return 0;
	}

	pipe = calloc(numpipe-numevnts,sizeof(void *));
	vfi_find_func(dev,elem[func],&pipe[0]);
	free(elem[func]);

	for (i = 0; i< numimaps;i++) {
		vfi_find_map(dev,elem[i],(struct vfi_map **)&pipe[i+2]);
		free(elem[i]);
	}

	for (i = 0; i< numomaps;i++) {
		vfi_find_map(dev,elem[events+i+1],(struct vfi_map **)&pipe[func+i+2]);
		free(elem[events+i+1]);
	}

	pipe[1] = (void *)(((numimaps & 0xff) << 0) | ((numomaps & 0xff) << 8));
	pipe[1] =  (void *)((unsigned int)pipe[1] ^ (unsigned int)pipe[0]);

	if (numevnts > 1) {
		for (i = 0; i< numevnts-1;i++) {
			vfi_find_event(dev,elem[func+i+1],(void **)&eloc);
			vfi_invoke_cmd(dev,"event_chain://%s.%s?request(%p),event_name(%s)\n",
				       elem[func+i+1],
				       eloc,
				       ah,
				       elem[func+i+2]);
			vfi_wait_async_handle(ah,&result,&e);

			if (i)
				free(elem[func+i+1]);
		}
		free(elem[func+i+1]);
	}

	free(*command);
	*command = malloc(128);
	vfi_find_event(dev,elem[func+1],(void **)&eloc);
	i = snprintf(*command,128,"event_start://%s.%s?request(%p)\n",elem[func+1],eloc,ah);
	if (i >= 128) {
		*command = realloc(*command,i+1);
		snprintf(*command,128,"event_start://%s.%s\n",elem[func+1],eloc);
	}

	free(elem[func+1]);
	free(vfi_set_async_handle(ah,pipe));

	return 0;
}

int quit_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd)
{
	/* quit or quit:// */
	vfi_set_dev_done(dev);
	return 0;
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

