#ifndef __SDR_SHA1_HH__
#define __SDR_SHA1_HH__

#include <memory.h>
#include <inttypes.h>

namespace sdr {

/* header */

#define SDR_SHA1_HASH_LENGTH 20
#define SDR_SHA1_BLOCK_LENGTH 64

typedef struct {
  uint32_t buffer[SDR_SHA1_BLOCK_LENGTH/4];
  uint32_t state[SDR_SHA1_HASH_LENGTH/4];
  uint32_t byteCount;
  uint8_t bufferOffset;
  uint8_t keyBuffer[SDR_SHA1_BLOCK_LENGTH];
  uint8_t innerHash[SDR_SHA1_HASH_LENGTH];
} sha1;

/**
 */
void sha1_init(sha1 *s);
/**
 */
void sha1_writebyte(sha1 *s, uint8_t data);
/**
 */
void sha1_write(sha1 *s, const char *data, size_t len);
/**
 */
uint8_t* sha1_result(sha1 *s);
/**
 */
void sha1_initHmac(sha1 *s, const uint8_t* key, int keyLength);
/**
 */
uint8_t* sha1_resultHmac(sha1 *s);

}

#endif // __SDR_SHA1_HH__
