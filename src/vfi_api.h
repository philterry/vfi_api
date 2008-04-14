#ifndef VFI_API_H
#define VFI_API_H

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdio.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

/** 
 * vfi_dev:
 *
 * This is an opaque type a pointer to which is used as the handle
 * which anchors the API around a particular vfi device driver
 * interface. As well as opening and holding the file descriptor for
 * /dev/vfi it also contains the heads of the various lists of data
 * structures used to implement the API. A handle is instantiated with
 * the constructor function vfi_open() and freed with the destructor
 * function vfi_close().
 */
struct vfi_dev;

/**
 * vfi_open:
 * @dev: a handle to be instantiated.
 * @devname: a string name for the char device, %NULL defaults to
 * /dev/vfi.
 * @timeout: the timeout on underlying blocking polls of the vfi
 * device, 0 defaults to -1 no timeout.
 *
 * This function allocates and initializes an vfi_dev and returns
 * the pointer to it in the output parameter @dev.
 *
 * Returns: 0 on success, negative on errors.
 */
extern int vfi_open(struct vfi_dev **dev, char *devname, int timeout);

/**
 * vfi_close:
 * @dev: handle of device to be closed and freed.
 *
 * This function closes down the API and frees all unused structures.
 */
extern void vfi_close(struct vfi_dev *dev);

/**
 * vfi_fileno:
 * @dev: the API device handle
 *
 * This function returns the file descriptor of the VFI char device
 * underlying the API. This may be required for various OS specific
 * calls.
 *
 * Returns: the file descriptor
 */
extern int vfi_fileno(struct vfi_dev *dev);

/**
 * vfi_source:
 * @f: function which is passed a pointer to @h[] and an input/output parameter @cmd
 * @d: the API root object
 * @h: an array of void * pointers forming the opaque input parameters to @f
 *
 * A source constructor, such as vfi_setup_file(), allocates an
 * #vfi_source with sufficent size to hold the necessary parameters
 * in @h[]. What these parameters are, how many, their types, etc., are
 * all a private matter between the constructor and the executor of
 * the source retrieval function @f which is set by the
 * constructor. Technically, vfi_source is a closure.
 *
 * Normally @f is not called directly but is invoked by the wrapper
 * function vfi_get_cmd().
 */
struct vfi_source {
	int (*f) (void **h, char **cmd);
	struct vfi_dev *d;
	void *h[];
};

/**
 * vfi_setup_file:
 * @dev: api root object
 * @src: the vfi_source handle to be initialized
 * @fp: the file stream pointer to be used for the input stream.
 *
 * @vfi_setup_file is an example of an #vfi_source constructor. It
 * sets up @src as a closure which reads the file stream @fp for RIL commands.
 * 
 * Returns: 0 if successful or EINVAL if @fp is NULL or ENOMEM if it
 * can't allocate the memory for @src.
 */
extern int vfi_setup_file(struct vfi_dev *dev, struct vfi_source **src,
			    FILE * fp);

/**
 * vfi_get_cmd:
 * @src: a source closure prepared with a source
 * generator, such as vfi_setup_file()
 * @cmd: an input/output parameter returning a string which the caller
 * is responsible for freeing. By passing a previously returned
 * command string as an input parameter this function will
 * automatically free it.
 *
 * vfi_get_cmd() is a function which executes the closure in @src in
 * order to retrieve a RIL command in an allocated string returned in
 * @cmd.  The caller must at some point free the returned @cmd
 * string. As a convenience if vfi_get_cmd() is called with the
 * previously returned value as input it will free the string as a
 * convenience to the caller before retrieving a newly allocated
 * string from the source. Initially or if this convenience is not
 * required *@cmd should be set to %NULL before calling vfi_get_cmd().
 *
 * Returns: true if there is more data to be fetched, false if this is
 * the last item of data from this source. Calling vfi_get_cmd() on a
 * @src after it has returned false is undefined.
 */
extern int vfi_get_cmd(struct vfi_source *src, char **cmd);

/**
 * vfi_async_handle:
 *
 * An opaque type used to provide synchronization points for
 * applications implementing some form of asynchronous interface to
 * the driver.
 * 
 * At its simplest its simply a void * providing a unique address for
 * use as an identifier in request(xxx)/reply(xxx) interactions with
 * the driver. For this purpose it can be allocated with
 * vfi_alloc_async_handle() and freed with vfi_free_async_handle().
 *
 * More usefully, it provides a synchronization semaphore which can
 * wait for the reply to the request with which it was issued. The
 * applications waits using vfi_wait_async_handle() and some
 * background thread/polling mechanism must call
 * vfi_post_async_handle() periodically to retrieve results from the
 * VFI driver and resume their waiting callers.
 *
 * If used with threads there may be issues with allocating, using and
 * freeing such a structure if shared. Therefore this structure is
 * also reference counted. Usage counts are incremented with
 * vfi_get_async_handle() and decremented with
 * vfi_put_async_handle(). A put which decrements the count to zero
 * will free the structuree. In fact vfi_free_async_handle() simply
 * descrements the usage count and only actually frees the structure
 * if the count reaches zero.
 *
 * Finally, as a convenience to implementers the async handle can also
 * pass a closure. This can be initialized in the
 * vfi_alloc_async_handle() constructor or modified/instantiated
 * with vfi_set_async_handle(). The closure is returned along with
 * the driver result in vfi_wait_async_handle call.
 */
struct vfi_async_handle;

/**
 * vfi_invoke_closure:
 * @e: handle for the closure
 *
 * A closure is an anonymouse chuck of memory the first element of
 * which is a function which takes a void * and returns a void *. The
 * rest of the structure is assumed to be data parameters which the
 * function may be interested in. The closure function is passed the
 * closure itself as only it knows what the size of memory is and what
 * the signature of the various types and structures contained therein
 * may be. Typically the closure function casts the void ** to a
 * structure pointer defining the contents of the closure. We use void
 * ** as the signature of a closure to indicate that it is more than a
 * single pointer.
 *
 * This function is simply a wrapper to avoid everyone trying to work
 * out the casts...
 *
 * Returns: void * which may be #NULL or itself a closure or simply
 * some value cast as a void *. Only the closure knows...
 */
static inline void *vfi_invoke_closure(void **e, struct vfi_dev *d, struct vfi_async_handle *ah, char *s)
{
	if (e && !vfi_dev_done(d))
		return ((void *(*)(void *,struct vfi_dev *,struct vfi_async_handle *, char *))e[0]) (e,d,ah,s);
	return 0;
}

/**
 * vfi_alloc_async_handle:
 * @e: A closure handle or NULL
 *
 * Allocates and returns an #vfi_async_handle optionally
 * instantiated with a closure.
 *
 * Returns: NULL on failure or problem.
 */
extern struct vfi_async_handle *vfi_alloc_async_handle(void *e);

/**
 * vfi_get_async_handle:
 * @h: handle to #vfi_async_handle
 *
 * Increments the usage count of @h.
 *
 * Returns: @h passed in on success #NULL otherwise.
 */
extern struct vfi_async_handle *vfi_get_async_handle(struct
							 vfi_async_handle *h);
/**
 * vfi_put_async_handle:
 * @h: handle to #vfi_async_handle
 *
 * Decrements the usage count of @h and deallocates the structure if
 * count reaches zero.
 *
 * Returns: @h passed in or #NULL if deallocated or some other problem.
 */
extern struct vfi_async_handle *vfi_put_async_handle(struct
							 vfi_async_handle *h);
/**
 * vfi_free_async_handle:
 * @h: handle to #vfi_async_handle
 *
 * Decrements usage count of @h and frees it if zero.
 *
 * Returns: @h passed in or #NULL if deallocated.
 */
extern struct vfi_async_handle *vfi_free_async_handle(struct
							  vfi_async_handle *h);
/**
 * vfi_set_async_handle:
 * @h: handle to #vfi_async_handle
 * @e: handle to closure
 *
 * Inserts the @e closure into the @h handle.
 *
 * Returns: the old closure originally in @h which may be NULL. NULL
 * is also returned if the handle is invalid.
 */
extern void *vfi_set_async_handle(struct vfi_async_handle * h,
				  void *e);
/**
 * vfi_wait_async_handle:
 * @h: handle of #vfi_async_handle to wait on
 * @r: result returned by driver
 * @e: closure set in handle.
 *
 * This function will sleep on the synchronizing event if the response
 * from the driver has not yet been received. When woken or if already
 * present, the reply from the driver is returned in @r and the
 * closure present in the handle is returned in @e
 *
 * Returns: @h passed in.
 */
extern struct vfi_async_handle *vfi_wait_async_handle(struct
							  vfi_async_handle *h,
							  char **r, void **e);
/**
 * vfi_post_async_handle:
 * @dev: the device to retrieve responses from
 *
 * This command should be repeatedly called in a background thread or
 * a polling loop to retrieve responses from the VFI driver and
 * return them to their requesting execution paths. A single response
 * is retrieved and returned per call. Replies retrieved which do not
 * have a reply field mapping to an #vfi_async_handle will be
 * discarded.
 *
 * Returns: 0 on success, negative if a response was discarded
 */
extern int vfi_post_async_handle(struct vfi_dev *dev);

/**
 * vfi_cmd_elem
 *
 * This opaque type is used to mechanise lists of commands. A command
 * is a function which takes a #vfi_dev handle, an
 * #vfi_async_handle and a cmd string and returns a closure. The
 * command string is of the format
 * &lt;cmd_name&gt;://&lt;paramter_string&gt;. Essentially, an #vfi_cmd_elem contains a
 * size of the &lt;cmd_name&gt; string, the &lt;cmd_name&gt; and a function
 * pointer together with the necessary next pointers and semaphores to
 * allow lists of such elements to be managed. The heads of the list
 * are stored in the @dev.  
 */
struct vfi_cmd_elem;
/**
 * vfi_find_cmd:
 * @dev: the api handle containing the list heads
 * @ah: an #vfi_async_handle
 * @list: a pointer to the list head
 * @cmd: the command string to be executed
 *
 * The command name extracted from the @cmd paramter is looked up in
 * the list headed by @list. If found the command function is executed
 * being passed the @dev, @ah and @cmd parameters.
 *
 * Returns: the closure returned by the execution of the command if
 * found otherwise #NULL.
 */
extern int vfi_find_cmd(struct vfi_dev *dev,
			    struct vfi_async_handle *ah,
			    struct vfi_cmd_elem *list, char **cmd);
/**
 * vfi_find_pre_cmd
 * @dev: an api #vfi_device handle
 * @ah: an #vfi_async_handle
 * @cmd: the command string to be executed if found.
 *
 * This command searches for @cmd in the pre_command list of @dev and
 * is basically a wrapper for vfi_find_cmd() with @list being the
 * pre_command element of @dev.
 * 
 * Returns: the closure from the executed command or #NULL if not found.
 */
extern int vfi_find_pre_cmd(struct vfi_dev *dev,
				struct vfi_async_handle *ah, char **cmd);
/**
 * vfi_find_post_cmd
 * @dev: an api #vfi_device handle
 * @ah: an #vfi_async_handle
 * @cmd: the command string to be executed if found.
 *
 * This command searches for @cmd in the post_command list of @dev and
 * is basically a wrapper for vfi_find_cmd() with @list being the
 * post_command element of @dev.
 *
 * Returns: the closure from the executed command or #NULL if the
 * command is not found in the list.
 */
extern int vfi_find_post_cmd(struct vfi_dev *dev,
				 struct vfi_async_handle *ah, char **cmd);

/**
 * vfi_register_cmd
 * @list: an #vfi_cmd_elem handle
 * @name: the name string of the command
 * @f: the command function
 * 
 * This command registers the function @f under @name in the provided list.
 *
 * Returns: 0 if successful otherwise a negative error code.
 */
extern int vfi_register_cmd(struct vfi_cmd_elem **list, char *name,
				  int (*f) (struct vfi_dev * dev,
					       struct vfi_async_handle * ah,
					       char **cmd));
/**
 * vfi_register_pre_cmd
 * @dev: an #vfi_dev handle
 * @name: the name string of the command
 * @f: the command function
 * 
 * This command registers the function @f under @name in the @dev's
 * pre_command list.
 *
 * Returns: 0 if successful otherwise a negative error code.
 */
extern int vfi_register_pre_cmd(struct vfi_dev *dev, char *name,
				  int(*f) (struct vfi_dev * dev,
					       struct vfi_async_handle * ah,
					       char **cmd));
/**
 * vfi_register_post_cmd
 * @dev: an #vfi_dev handle
 * @name: the name string of the command
 * @f: the command function
 * 
 * This command registers the function @f under @name in the @dev's
 * post_command list.
 *
 * Returns: 0 if successful otherwise a negative error code.
 */
extern int vfi_register_post_cmd(struct vfi_dev *dev, char *name,
				   int (*f) (struct vfi_dev * dev,
						struct vfi_async_handle * ah,
						char **cmd));
/**
 * vfi_npc
 *
 * This structure is an opaque type representing a named polymorphic
 * closure, or npc, for short. An closure is an area of memory whose
 * first element is a function taking a void * and returning a void
 * *. The returned void * may be a closure, or simply #NULL, or simply
 * a value coerced to void *. Only the function knows the size and
 * signature, i.e., type structure, of the memory areas representd by
 * the closure which we type as void ** to indicate a pointer to an
 * array of void *, i.e., some unknown size chunk of memory.
 *
 * The closure is named by being associated in a list structure with a
 * name. This is the purpose of #vfi_npc.
 */
struct vfi_npc;

/**
 * vfi_register_npc
 * @list: the list header of npc's
 * @name: the name of the closure to be added.
 * @e: the closure to be added.
 *
 * This is the generic function underlying vfi_register_map(),
 * vfi_register_event(), vfi_register_func().
 *
 * Returns: 0 on success otherwise error.
 */
extern int vfi_register_npc(struct vfi_npc **list, char *name, void *e);

/**
 * vfi_unregister_npc
 * @list: the list header of npc's
 * @name: the name of the closure to be added.
 * @e: returns the closure of unregistered entry.
 *
 * This function is the generic function underlying vfi_register_map(),
 * vfi_register_event(), vfi_register_func().
 *
 * Returns: 0 on success otherwise error.
 */
extern int vfi_unregister_npc(struct vfi_npc **list, char *name, void **e);

/**
 * vfi_register_func
 * @dev: the #vfi_dev handle with the list head of functions
 * @name: the name of the command function to be added
 * @e: the closure representing the command function
 *
 * This function adds a closure representing an API command to the
 * @dev's list of functions.
 *
 * Returns: 0 on success otherwise error.
 */
extern int vfi_register_func(struct vfi_dev *dev, char *name, void *e);

/**
 * vfi_unregister_func
 * @dev: the #vfi_dev handle with the list head of functions
 * @name: the name of the command function to be added
 * @e: returns the closure of the deleted function
 *
 * This function deletes a closure representing an API command from the
 * @dev's list of functions.
 *
 * Returns: 0 on success otherwise error.
 */
extern int vfi_unregister_func(struct vfi_dev *dev, char *name, void **e);

/**
 * vfi_map
 * @f: the closure function use in smb_mmap and smb_create pre commands
 * @name: a pointer to a heap allocated name string
 * @mem: a void * to the virtual address of the mmap
 * @extent: the size of the mmap in bytes.
 * @name_buf: the name buffer pointed to by @name
 *
 * This structure is used to provide a closure structure for use with vfi_register_map() and vfi_find_map().
 */
struct vfi_map {
	void *f;
	char *name;
	void *mem;
	long extent;
	char name_buf[];
};

/**
 * vfi_alloc_map
 * @map: the returned structure
 * @name: the name of the structure
 *
 * Allocates a #vfi_map structure and initializes its name.
 *
 * Returns: 0 on success error otherwise
 */
extern int vfi_alloc_map(struct vfi_map **map, char *name);

/**
 * vfi_register_map
 * @dev: the #vfi_dev handle with the list head of maps
 * @name: the name of the map
 * @map: the closure representing the map
 *
 * This function adds a closure representing an API map to the @dev's
 * list of maps
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_register_map(struct vfi_dev *dev, char *name, struct vfi_map *map);

/**
 * vfi_unregister_map
 * @dev: the #vfi_dev handle with the list head of maps
 * @name: the name of the map
 * @map: returns the closure of the unregistered map
 *
 * This function removes a closure representing an API map from the @dev's
 * list of maps
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_unregister_map(struct vfi_dev *dev, char *name, struct vfi_map **map);

/**
 * vfi_register_event
 * @dev: the #vfi_dev handle with the list head of maps
 * @name: the name of the map
 * @e: the closure representing the event
 *
 * This function adds a closure representing an API event to the @dev's
 * list of events
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_register_event(struct vfi_dev *dev, char *name, void *e);

/**
 * vfi_unregister_event
 * @dev: the #vfi_dev handle with the list head of maps
 * @name: the name of the map
 * @e: the closure from the unregistered event
 *
 * This function removes a closure representing an API event from the @dev's
 * list of events
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_unregister_event(struct vfi_dev *dev, char *name, void **e);

/**
 * vfi_find_npc
 * @list: the list to be searched
 * @name: the name of the closure being searched for.
 * @npc: the returned npc if found
 *
 * This function searches a list of named closures.
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_find_npc(struct vfi_npc *list, char *name, struct vfi_npc **npc);

/**
 * vfi_find_func
 * @dev: the #vfi_dev handle whose function list is to be searched
 * @name: the name of the function closure being searched for.
 * @e: returns the closure if found
 *
 * This function searches the function list of named closures whose head is held in @dev
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_find_func(struct vfi_dev *dev, char *name, void **e);

/**
 * vfi_find_map
 * @dev: the #vfi_dev handle whose map list is to be searched
 * @name: the name of the map closure being searched for.
 * @map: returns the map if found
 *
 * This function searches the map list of named closures whose head is held in @dev
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_find_map(struct vfi_dev *dev, char *name, struct vfi_map **map);

/**
 * vfi_find_event
 * @dev: the #vfi_dev handle whose event list is to be searched
 * @name: the name of the event closure being searched for.
 * @e: returns the event if found
 *
 * This function searches the event list of named closures whose head is held in @dev
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_find_event(struct vfi_dev *dev, char *name, void **e);

/**
 * vfi_get_option
 * @str: the string to be searched for the option
 * @name: the name of the option to be search for.
 *
 * The @str is searched for the string @name. 
 *
 * Returns: %TRUE if @name is found in @str %FALSE otherwise
 */
extern int vfi_get_option(char *str, char *name);

/**
 * vfi_get_str_arg
 * @str: to be searched for named option
 * @name: option to be sought
 * @val: string value of option if found
 *
 * If @name is not found a negative error value is returned. If @name is
 * found but doesn't have a value, ie @name(value) format, then zero is
 * returned. Otherwise %TRUE is returned.
 *
 * Returns: < 0 if @name not found, 0 if found but no value, > 0 if
 * @val is returned.
 */
extern int vfi_get_str_arg(char *str, char *name, char **val);

/**
 * vfi_get_long_arg:
 * @str: string to be searched
 * @name: of option being sought
 * @value: output parameter for value of option to be stored in.
 * @base: interpretation of value string to long conversion.
 *
 * @base selects conversion for strtol, 8, 10 or 16. 0 assumes octal
 * for leading 0, hex if leading 0x, decimal otherwise.
 *
 * Returns: value of options value string or -1 if not found or has no
 * value string format.
 */
extern int vfi_get_long_arg(char *str, char *name, long *value, int base);

/**
 * vfi_get_dec_arg
 * @str: string to be searched for option
 * @name: option being sought
 * @val: value of option
 *
 * This is a decimal, ie @base=10, wrapper for vfi_get_long_arg().
 *
 * Returns: 0 on success else error.
 */
extern int vfi_get_dec_arg(char *str, char *name, long *val);

/**
 * vfi_get_location
 * @str: the string to be searched for a location substring
 * @loc: a copy of the location in an allocated string buffer.
 *
 * Finds the first occurrence of "name.location" in string @str and
 * returns a string @loc containting "location". Caller should
 * deallocate @loc when finished with it.
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_get_location(char *str, char **loc);

/**
 * vfi_get_name_location
 * @str: the string to be searched for a location substring
 * @name: copy of the name in an allocated string buffer.
 * @loc: a copy of the location in an allocated string buffer.
 *
 * Finds the first occurrence of "name.location" in string @str and
 * returns a string @name containting "name" and a string @loc
 * containting "location". Caller should deallocate @name and @loc
 * when finished with them.
 *
 * Returns: 0 on success otherwise error
 */
extern int vfi_get_name_location(char *str, char **name, char **loc);

/**
 * vfi_get_extent
 * @str: string to be searched "name[.location][#offset][:extent]
 * @extent: output parameter to hold value of extent if found
 *
 * Searches for an extent string, :extent, and if found returns it in @extent.
 *
 * Returns: 0 on success, otherwise error.
 */
extern int vfi_get_extent(char *str, long *extent);

/**
 * vfi_get_offset
 * @str: string to be searched "name[.location][#offset][:extent]
 * @offset: output parameter to hold value of offset if found
 *
 * Searches for an offset string, #offset, and if found returns it in @offset.
 *
 * Returns: 0 on success, otherwise error.
 */
extern int vfi_get_offset(char *str, long long *offset);

/**
 * vfi_parse_bind
 * @str: string to be parsed of form cmd://xfer/dest=src
 * @cmd: cmd string if found else #NULL
 * @xfer: xfer string if found else #NULL
 * @dest: dest string if found else #NULL
 * @src: src string if found else #NULL
 *
 * Parses string into constituent parts. Parts not found return #NULL. Caller 
 * is responsible for freeing returned strings.
 *
 * Returns: number of elements parsed, 0 or negative on error.
 */
extern int vfi_parse_bind(char *str, char **cmd, char **xfer, char **dest, char **src);

/**
 * vfi_parse_desc
 * @str: string to be parsed of form name[.location][#offset][:extent][[?option][,option]*]
 * @name: string containing name if found or #NULL
 * @location: string containing location if found or NULL
 * @offset: long long to contain value of #offset if found else undefined
 * @extent: long to contain value of :extent if found else undefined
 * @opts: string containing options if found else NULL.
 *
 * Parses desc string. Returned strings must be freed by caller.
 *
 * Returns: 0 if no extent or offset, 1 if extent, 2 if offset, 3 if both found.
 */
extern int vfi_parse_desc(char *str, char **name, char **location, int *offset, int *extent, char **opts);

/**
 * vfi_get_hex_arg
 * @str: string to be searched for option
 * @name: option being sought
 * @val: value of option if found
 *
 * This is a hexadecimal, ie @base=16, wrapper for vfi_get_long_arg().
 *
 * Returns: 0 on success else error.
 */
extern int vfi_get_hex_arg(char *str, char *name, long *val);

/**
 * vfi_poll_read
 * @dev: the #vfi_dev handle to be polled for results.
 *
 * This function polls the underlying file descriptor for a
 * non-blocking read. The poll blocks for the timeout configued for
 * the @dev.
 *
 * Returns: positive on success, 0 on timeout, negative on error. See poll().
 */
extern int vfi_poll_read(struct vfi_dev *dev);

/**
 * vfi_do_cmd
 * @dev: the device to which the command is sent and from which the
 * reply is obtained.
 * @reply: the output char * parameter to receive the reply string.
 * @format: the format string to "print" the command to the @dev.
 * @...: parameters for the format printf.
 *
 * This command dispatches the command to the @dev and then blocks for
 * the response. These do_cmds do not use an #vfi_async_handle but
 * rely on the application not issuing multiple writes, i.e.,
 * interleaved vfi_invoke_cmd() like functions are not used.
 *
 * Returns: 0 on success else error
 */
extern int vfi_do_cmd(struct vfi_dev *dev, char **reply, char *format, ...)
    __attribute__ ((format(printf, 3, 4)));

/**
 * vfi_do_cmd_ap
 * @dev: the device to be used to send command and receive reply.
 * @reply: the reply output parameter
 * @format: the format string used to print the command to the @dev.
 * @list: va_list to satisfy @format
 *
 * This command is a va_list interface to vfi_do_cmd().
 *
 * Returns: 0 on success else error.
 */
extern int vfi_do_cmd_ap(struct vfi_dev *dev, char **reply, char *format, va_list list);

/**
 * vfi_do_cmd_str
 * @dev: the device to be used
 * @reply: the output parameter for the reply string
 * @cmd: the command string to be sent
 * @size: optional size of the @cmd string. 0 if unknown.
 *
 * This is a string interface version of vfi_do_cmd().
 *
 * Returns: 0 on success else error
 */
extern int vfi_do_cmd_str(struct vfi_dev *dev, char **reply, char *cmd, int size);

/**
 * vfi_invoke_cmd
 * @dev: #vfi_dev handle currently in use
 * @format: format string to "print" command to @dev
 * @...: paramters to satisfy @format
 *
 * Use this varadic function to print a command to @dev. To obtain the
 * result from the driver you must print a "?request(%p)" in the
 * format passing a #vfi_async_handle to satisfy the %p. This allows
 * vif_post_async_handle() to pass the result back via the async
 * handle when retrieve with a call to vfi_wait_async_handle().
 *
 * Returns: length of string written to @dev else 0 or negative error.
 */
extern int vfi_invoke_cmd(struct vfi_dev *dev, char *format, ...)
    __attribute__ ((format(printf, 2, 3)));

/**
 * vfi_invoke_cmd_ap
 * @dev: #vfi_dev handle currently in use
 * @format: format string to "print" command to @dev
 * @list: parameters to satisfy @format
 *
 * This function is the same as vfi_invoke_cmd() where the caller is
 * already varadic.
 *
 * Returns: length of string written to @dev else 0 or negative error.
 */
extern int vfi_invoke_cmd_ap(struct vfi_dev *dev, char *format,va_list list);

/**
 * vfi_invoke_cmd_str
 * @dev: #vfi_dev handle currently in use
 * @str: the command string to be passed to the driver
 * @size: 0 or size of string @str
 *
 * As with vfi_invoke_cmd() the string @str should contain a
 * request(xxx) option where xxx is the address of an
 * #vfi_async_handle.
 *
 * Returns: length of string written to @dev else 0 or negative error.
 */
extern int vfi_invoke_cmd_str(struct vfi_dev *dev, char *str, int size);

/**
 * vfi_get_result
 * @dev: @vfi_dev handle in use
 * @result: string returned from driver
 *
 * This command is used to read results from the driver. The returned
 * string must be freed by the caller at some point.
 *
 * Returns: 1 on success or negative on error.
 */
extern int vfi_get_result(struct vfi_dev *dev, char **result);


 /*
  * This were good at the time of 2.6.21-rc5.mm4 ...
  */
#ifndef __NR_eventfd
#if defined(__x86_64__)
#define __NR_eventfd 283
#elif defined(__i386__)
#define __NR_eventfd 323
#elif defined(__powerpc__)
#define __NR_eventfd 308
#else
#error Cannot detect your architecture!
#endif
#endif

typedef unsigned long aio_context_t;

enum {
	IOCB_CMD_PREAD = 0,
	IOCB_CMD_PWRITE = 1,
	IOCB_CMD_FSYNC = 2,
	IOCB_CMD_FDSYNC = 3,
	/* These two are experimental.
	 * IOCB_CMD_PREADX = 4,
	 * IOCB_CMD_POLL = 5,
	 */
	IOCB_CMD_NOOP = 6,
	IOCB_CMD_PREADV = 7,
	IOCB_CMD_PWRITEV = 8,
};

#if defined(__LITTLE_ENDIAN)
#define PADDED(x,y)	x, y
#elif defined(__BIG_ENDIAN)
#define PADDED(x,y)	y, x
#else
#error edit for your odd byteorder.
#endif

#define IOCB_FLAG_RESFD		(1 << 0)

/*
 * we always use a 64bit off_t when communicating
 * with userland.  its up to libraries to do the
 * proper padding and aio_error abstraction
 */
struct iocb {
	/* these are internal to the kernel/libc. */
	u_int64_t aio_data;	/* data to be returned in event's data */
	u_int32_t PADDED(aio_key, aio_reserved1);
	/* the kernel sets aio_key to the req # */

	/* common fields */
	u_int16_t aio_lio_opcode;	/* see IOCB_CMD_ above */
	int16_t aio_reqprio;
	u_int32_t aio_fildes;

	u_int64_t aio_buf;
	u_int64_t aio_nbytes;
	int64_t aio_offset;

	/* extra parameters */
	u_int64_t aio_reserved2;	/* TODO: use this for a (struct sigevent *) */

	u_int32_t aio_flags;
	/*
	 * If different from 0, this is an eventfd to deliver AIO results to
	 */
	u_int32_t aio_resfd;
};				/* 64 bytes */

struct io_event {
	u_int64_t data;		/* the data field from the iocb */
	u_int64_t obj;		/* what iocb this event came from */
	int64_t res;		/* result code for this event */
	int64_t res2;		/* secondary result */
};

static inline long io_setup(unsigned nr_reqs, aio_context_t * ctx)
{

	return syscall(__NR_io_setup, nr_reqs, ctx);
}

static inline long io_destroy(aio_context_t ctx)
{

	return syscall(__NR_io_destroy, ctx);
}

static inline long io_submit(aio_context_t ctx, long n, struct iocb **paiocb)
{

	return syscall(__NR_io_submit, ctx, n, paiocb);
}

static inline long io_cancel(aio_context_t ctx, struct iocb *aiocb,
			     struct io_event *res)
{

	return syscall(__NR_io_cancel, ctx, aiocb, res);
}

static inline long io_getevents(aio_context_t ctx, long min_nr, long nr,
				struct io_event *events, struct timespec *tmo)
{

	return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo);
}

static inline int eventfd(int count)
{

	return syscall(__NR_eventfd, count);
}

extern int vfi_get_eventfd(int);
extern void asyio_prep_pread(struct iocb *iocb, int fd, void *buf, int nr_segs,
			     int64_t offset, int afd);
extern void asyio_prep_pwrite(struct iocb *, int, void const *, int, int64_t,
			      int);
extern void asyio_prep_preadv(struct iocb *iocb, int fd, struct iovec *iov,
			      int nr_segs, int64_t offset, int afd);
extern void asyio_prep_pwritev(struct iocb *iocb, int fd, struct iovec *iov,
			       int nr_segs, int64_t offset, int afd);
extern int waitasync(int, int);

#endif /* VFI_API_H */
