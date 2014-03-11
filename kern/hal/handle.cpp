#import <types.h>
#import "handle.h"

// Number of handles to allocate memory for when we run out
#define HANDLE_TABLE_GROWTH		0x400
// Number of handles to initially allocate memory for
#define HANDLE_INITIAL_SIZE		0x800

#define FREE_HANDLE_TABLE_LEN	32

/*
 * This is the private internal handle type: It points to some kind of object.
 */
struct khandle {
	unsigned int type;
	void *object;
} __attribute__((packed));

// Pointer to the handle array
static unsigned int handle_array_size;
static struct khandle *handle_array;

// A shortlist of handles to allocate
static hal_handle_t free_handles[FREE_HANDLE_TABLE_LEN];

// Stats
static unsigned int handles_allocated;

/*
 * Initialise the handle allocator.
 */
static int hal_handle_init(void) {
	// Allocate the memory
	handle_array_size = HANDLE_INITIAL_SIZE;
	handle_array = (struct khandle *) kmalloc(handle_array_size * sizeof(struct khandle));

	// Fill handle 0 to an invalid handle
	handle_array[0].type = 0xDEADBEEF;
	handle_array[0].object = (void *) 0xFFFFFFFF;

	// Initialise the remainder of the handle pool
	for(unsigned int i = 1; i < HANDLE_INITIAL_SIZE; i++) {
		handle_array[i].type = BAD_HANDLE_TYPE;
	}

	// Fill free handle table (speeds up allocation)
	for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
		free_handles[i] = i + 1;
	}

	return 0;
}
module_early_init(hal_handle_init);

extern "C" {
	/*
	 * Request the allocation of a new handle, pointing to obj.
	 */
	hal_handle_t hal_handle_allocate(void *obj, unsigned int type) {
		hal_handle_t handle = 0;

		// Try to get a handle from the handle table
		for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
			if(likely(free_handles[i])) {
				handle = free_handles[i];

				// Resort table
				for(int x = i; x < (FREE_HANDLE_TABLE_LEN - 1); x++) {
					free_handles[x] = free_handles[x+1];
				}

				free_handles[FREE_HANDLE_TABLE_LEN-1] = 0;
 			}
		}

		// If the handle is still zero, traverse the array for an empty slot.
		if(!handle) {
			for(unsigned int i = 0; i < handle_array_size; i++) {
				// Found an empty handle
				if(unlikely(!handle_array[i].object && handle_array[i].type == BAD_HANDLE_TYPE)) {
					handle = i;

					// Do a quick check if handles after this are free also
					unsigned int offset = 0;
					for(unsigned int x = 0; x < (FREE_HANDLE_TABLE_LEN / 4); x++) {
						if(!handle_array[x + i].object && handle_array[x + i].type == BAD_HANDLE_TYPE) {
							free_handles[offset++] = x + i;
						}
					}

					break;
				}
			}
		}

		// Once the entire handle table has been searched, resize it by a page.
		if(unlikely(!handle)) {
			unsigned int firstFreeHandle = handle_array_size;
			handle = firstFreeHandle++;

			// Add HANDLE_TABLE_GROWTH number of handles to the array
			unsigned int newSize = sizeof(struct khandle) * handle_array_size;
			newSize += sizeof(struct khandle) * HANDLE_TABLE_GROWTH;

			// Reallocate memory
			struct khandle *newPtr = (struct khandle*) krealloc(handle_array, newSize);

			if(newPtr) {
				KDEBUG("Resized handle array to %u (0x%08X)", handle_array_size + HANDLE_TABLE_GROWTH,
					(unsigned int) newPtr);

				handle_array = newPtr;
			} else {
				return 0;
			}

			// Fill in free handle table
			for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
				free_handles[i] = firstFreeHandle++;
			}

			// Initialise the new handles
			newSize = handle_array_size + HANDLE_TABLE_GROWTH;
			for(unsigned int i = handle_array_size; i < newSize; i++) {
				handle_array[i].type = BAD_HANDLE_TYPE;
			}

			// Increment the size.
			handle_array_size = newSize;
		}

		// Ensure we only allocate valid handles
		if(handle) {
			if(handle_array[handle].type != BAD_HANDLE_TYPE) {
				PANIC("Re-allocating handle for some reason");
			}

			// Update stats
			handles_allocated++;

			// Actually save object in the handle
			handle_array[handle].object = obj;
			handle_array[handle].type = type;
		}

		return handle;
	}

	// Retrieves the object pointed to by a handle.
	void *hal_handle_get_object(hal_handle_t handle) {
		return handle_array[handle].object;
	}

	// Retrieves the type of the handle.
	unsigned int hal_handle_get_type(hal_handle_t handle) {
		return handle_array[handle].type;
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
		// Make sure this handle isn't already deallocated
		if(handle_array[handle].type == BAD_HANDLE_TYPE) return;

		// Deallocate object
		if(free) {
			kfree(handle_array[handle].object);
		}

		// Mark handle as free (pointer == NULL)
		handle_array[handle].object = NULL;
		handle_array[handle].type = BAD_HANDLE_TYPE;

		// Update handle
		handles_allocated--;

		// Optionally mark as free in the free handles table, if there's space
		for(unsigned int i = 0; i < FREE_HANDLE_TABLE_LEN; i++) {
			if(unlikely(!free_handles[i])) {
				free_handles[i] = handle;
				return;
			}
		}
	}
}