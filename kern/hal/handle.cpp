#import <types.h>
#import "handle.h"

// Number of handles to allocate memory for when we run out
#define HANDLE_TABLE_GROWTH		512
// Number of handles to initially allocate memory for
#define HANDLE_INITIAL_SIZE		2048

#define FREE_HANDLE_TABLE_LEN	32

/*
 * This is the private internal handle type: It points to some kind of object.
 */
struct khandle {
	void *object;
	unsigned int reserved;
};

// Pointer to the handle array
unsigned int handle_array_size;
static struct khandle *handle_array;

// A shortlist of handles to allocate
static hal_handle_t free_handles[FREE_HANDLE_TABLE_LEN];

/*
 * Initialise the handle allocator.
 */
static int hal_handle_init(void) {
	// Allocate the memory
	handle_array_size = HANDLE_INITIAL_SIZE;
	handle_array = (struct khandle *) kmalloc(handle_array_size * sizeof(struct khandle));

	// Fill free handle table (speeds up allocation)
	for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
		free_handles[i] = i;
	}

	return 0;
}
module_early_init(hal_handle_init);

extern "C" {
	/*
	 * Request the allocation of a new handle, pointing to obj.
	 */
	hal_handle_t hal_handle_allocate(void *obj) {
		hal_handle_t handle = 0;

		// Try to get a handle from the handle table
		for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
			if(likely(free_handles[i])) {
				handle = free_handles[i];
				free_handles[i] = 0;
			}
		}

		// If the handle is still zero, traverse the array for an empty slot.
		for(unsigned int i = 0; i < handle_array_size; i++) {
			// Found an empty handle
			if(unlikely(!handle_array[i].object)) {
				handle = i;
				break;
			}
		}

		// Once the entire handle table has been searched, resize it by a page.
		if(unlikely(!handle)) {
			unsigned int firstFreeHandle = handle_array_size;
			handle = firstFreeHandle;

			// Add HANDLE_TABLE_GROWTH number of handles to the array
			unsigned int newSize = sizeof(struct khandle) * handle_array_size;
			newSize += sizeof(struct khandle) * HANDLE_TABLE_GROWTH;

			// Reallocate memory
			handle_array = (struct khandle*) krealloc(handle_array, newSize);

			// Fill in free handle table
			for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
				free_handles[i] = firstFreeHandle++;
			}
		}

		// Actually save object in the handle
		handle_array[handle].object = obj;
		return handle;
	}

	// Retrieves the object pointed to by a handle.
	void *hal_handle_get_object(hal_handle_t handle) {
		return handle_array[handle].object;
	}

	// Updates the object pointed to by the handle, freeing the old if requested.
	void hal_handle_update_object(hal_handle_t handle, void *obj, bool free) {
		// Deallocate object
		if(free) {
			kfree(handle_array[handle].object);
		}

		// Change the object pointed to by the handle
		handle_array[handle].object = obj;
	}

	// Releases an existing handle, freeing its object, if requested.
	void hal_handle_release(hal_handle_t handle, bool free) {
		// Deallocate object
		if(free) {
			kfree(handle_array[handle].object);
		}

		// Mark handle as free (pointer == NULL)
		handle_array[handle].object = NULL;

		// Optionally mark as free in the free handles table, if there's space
		for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
			if(unlikely(!free_handles[i])) {
				free_handles[i] = handle;
				return;
			}
		}
	}
}