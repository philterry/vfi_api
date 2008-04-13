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
 * This command parses the bind_create command in @cmd for event names
 * and registers them with the API handle @dev's event name list. 
 *
 * Returns: 0 to indicate that the @cmd should be run by the driver.
 */
extern int bind_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * smb_mmap_pre_cmd
 */
extern int smb_mmap_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * smb_create_pre_cmd
 */
extern int smb_create_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * event_find_pre_cmd
 * @dev: the API handle in use
 * @ah: the #vfi_async_handle in use
 * @cmd: the event_find command.
 *
 * This command inserts a closure into @ah to be run by the caller
 * after running the @cmd through the driver.
 *
 * Returns: 0 indicating that @cmd should be run by the driver.
 */
extern int event_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * sync_find_pre_cmd
 */
extern int sync_find_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * pipe_pre_cmd
 */
extern int pipe_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * quit_pre_cmd
 */
extern int quit_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

/**
 * map_init_pre_cmd
 */
extern int map_init_pre_cmd(struct vfi_dev *dev, struct vfi_async_handle *ah, char **cmd);

#endif






