#ifndef VFI_FRMWRK_H
#define VFI_FRMWRK_H


struct vfi_dev;
struct vfi_async_handle;
/**
 * bind_create_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the bind_create command in @cmd, and inserts
 * a closure into @ah to be run by the caller after running the cmd
 * through the driver. If the driver command is successful the 
 * closure will register the event names with the API handle @dev's 
 * event name list. 
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver
 * or a negative value to indicate that an error occurred.
 */
extern int bind_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * mmap_create_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the mmap_create command in @cmd for map names
 * and registers them with the API handle @dev's map name list. 
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int mmap_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * smb_create_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the smb_create command in @cmd for map names
 * and registers them with the API handle @dev's map name list. 
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int smb_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * event_find_pre_cmd
 * @dev: the API handle in use
 * @ah: the #vfi_async_handle in use
 * @cmd: the event_find command.
 *
 * This command inserts a closure into @ah to be run by the caller
 * after running the @cmd through the driver. If the driver command is
 * successful the closure will register the event in the API's @dev handle.
 *
 * Returns: 0 indicating that @cmd should be run by the driver.
 */
extern int event_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * wait_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses any command in @cmd for a wait option 
 * and sets up a closure to loop on errors from the driver command if found.
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int wait_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * pipe_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the pipe command in @cmd for event names, map names and function
 * and sets up a closure in the #vfi_async_handle @ah to execute the
 * named function. The pipe command in @cmd is replaced with an
 * event_start command for the head of the event chain, or lone event,
 * named in the pipe command and the closure will return true to keep
 * the pipeline in source_thread() running. The closure will terminate
 * the pipeline if errors occur or if it detects that quit/abort etc.,
 * have been called.
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int pipe_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * unix_pipe_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the pipe command in @cmd for event names, map names and function
 * and sets up a closure in the #vfi_async_handle @ah to execute the
 * named function. The pipe command in @cmd is replaced with an
 * event_start command for the head of the event chain, or lone event,
 * named in the pipe command and the closure will return true to keep
 * the pipeline in source_thread() running. The closure will terminate
 * the pipeline if errors occur or if it detects that quit/abort etc.,
 * have been called.
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int unix_pipe_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * quit_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the quit command in @cmd for and notifies all
 * users of @dev that a quit is in progress. 
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int quit_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * map_init_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the
 * map_init://map.location#offset:extent?value(x) and the 
 * map_init://map.location#offset:extent?pattern(type) command in @cmd
 * for a map name and an init value or init pattern. The offset and extent of the
 * named map is then initialized with the value x or the pattern type. 
 * The supported pattern type is "counting".
 *
 * Returns: 1 to indicate that the @cmd should not be run by the driver or a negative
 * value if an error occurred.
 */
extern int map_init_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * map_check_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the
 * map_check://map.location#offset:extent?value(x) and the
 * map_check://map.location#offset:extent?pattern(type) command in @cmd
 * for a map name and an init value or init patter. The offset and extent of the
 * named map is checked for the value x or the pattern type. The supported pattern 
 * type is "counting".
 *
 * Returns: 1 to indicate that the @cmd should not be run by the driver or a negative
 * value if an error occurred. If the check fails then -EBADMSG is returned.
 */
extern int map_check_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * map_install_pre_cmd
 * @dev: API handle
 * @ah: async handle in use for this thread
 * @cmd: IO parameter, bind command on input
 *
 * This command parses the
 * map_install://name:extent command in @cmd
 * for a map name and extent and malloc's an area of memory of extent size and then
 * registers it as a map under name.
 *
 * Returns: 1 to indicate that the @cmd should not be run by the driver or a negative
 * value if an error occurred. If the check fails then -EBADMSG is returned.
 */
extern int map_install_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * vfi_initialize_api
 * @dev: API handle
 *
 * This command initializes the API handle with a default set of pre-commands.
 *
 * Returns: 0
 */
extern int vfi_initialize_api(struct vfi_dev *dev);

/**
 * vfi_clear_api
 * @dev: API handle
 *
 * This command clears the API handle of the default set of pre-commands.
 *
 * Returns: 0
 */
extern void vfi_clear_api(struct vfi_dev *dev);

#endif






