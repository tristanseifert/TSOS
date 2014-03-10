#import <types.h>

// Handle type
typedef unsigned int hal_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

	// Request the allocation of a new handle pointing to obj
	hal_handle_t hal_handle_allocate(void *obj);

	// Retrieves the object pointed to by a handle.
	void *hal_handle_get_object(hal_handle_t handle);

	// Updates the object pointed to by the handle, freeing the old if requested.
	void hal_handle_update_object(hal_handle_t handle, void *obj, bool free);

	// Releases an existing handle, freeing its object, if requested.
	void hal_handle_release(hal_handle_t handle, bool free);

#ifdef __cplusplus
}
#endif