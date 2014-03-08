#import <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LZFX_VERSION_MAJOR	  0
#define LZFX_VERSION_MINOR	  1
#define LZFX_VERSION_STRING	 "0.1"

#ifndef LZFX_HLOG
#define LZFX_HLOG 16
#endif

// Error codes
#define LZFX_ESIZE		-1	  // Output buffer too small
#define LZFX_ECORRUPT	-2	  // Invalid data for decompression
#define LZFX_EARGS		-3	  // Arguments invalid (NULL)

/*  Buffer-to buffer compression.
	Supply pre-allocated input and output buffers via ibuf and obuf, and
	their size in bytes via ilen and olen.  Buffers may not overlap.

	On success, the function returns a non-negative value and the argument
	olen contains the compressed size in bytes.  On failure, a negative
	value is returned and olen is not modified.
*/
int lzfx_compress(const void* ibuf, unsigned int ilen, void* obuf, unsigned int *olen);

/*  Buffer-to-buffer decompression.
	Supply pre-allocated input and output buffers via ibuf and obuf, and
	their size in bytes via ilen and olen.  Buffers may not overlap.

	On success, the function returns a non-negative value and the argument
	olen contains the uncompressed size in bytes.  On failure, a negative
	value is returned.

	If the failure code is LZFX_ESIZE, olen contains the minimum buffer size
	required to hold the decompressed data.  Otherwise, olen is not modified.

	Supplying a zero *olen is a valid and supported strategy to determine the
	required buffer size.  This does not require decompression of the entire
	stream and is consequently very fast.  Argument obuf may be NULL in
	this case only.
*/
int lzfx_decompress(const void* ibuf, unsigned int ilen, void* obuf, unsigned int *olen);


#ifdef __cplusplus
}
#endif