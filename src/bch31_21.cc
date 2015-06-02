#include "bch31_21.hh"
#include "stdlib.h"
#include "string.h"

using namespace sdr;

/*
 * the code used by POCSAG is a (n=31,k=21) BCH Code with dmin=5,
 * thus it could correct two bit errors in a 31-Bit codeword.
 * It is a systematic code.
 * The generator polynomial is:
 *   g(x) = x^10+x^9+x^8+x^6+x^5+x^3+1
 * The parity check polynomial is:
 *   h(x) = x^21+x^20+x^18+x^16+x^14+x^13+x^12+x^11+x^8+x^5+x^3+1
 * g(x) * h(x) = x^n+1
 */
#define BCH_POLY 03551 /* octal */
#define BCH_N    31
#define BCH_K    21

static inline unsigned char even_parity(uint32_t data)
{
    unsigned int temp = data ^ (data >> 16);

    temp = temp ^ (temp >> 8);
    temp = temp ^ (temp >> 4);
    temp = temp ^ (temp >> 2);
    temp = temp ^ (temp >> 1);
    return temp & 1;
}

static unsigned int
pocsag_syndrome(uint32_t data)
{
    uint32_t shreg = data >> 1; /* throw away parity bit */
    uint32_t mask = 1L << (BCH_N-1), coeff = BCH_POLY << (BCH_K-1);
    int n = BCH_K;

    for(; n > 0; mask >>= 1, coeff >>= 1, n--) {
      if (shreg & mask) { shreg ^= coeff; }
    }
    if (even_parity(data)) {
      shreg |= (1 << (BCH_N - BCH_K));
    }
    return shreg;
}

static void
bitslice_syndrome(uint32_t *slices)
{
    const int firstBit = BCH_N - 1;
    int i, n;
    uint32_t paritymask = slices[0];

    // do the parity and shift together
    for (i = 1; i < 32; ++i) {
        paritymask ^= slices[i];
        slices[i-1] = slices[i];
    }
    slices[31] = 0;

    // BCH_POLY << (BCH_K - 1) is
    //                                                              20   21 22 23
    //  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ONE, 0, 0, ONE,
    //  24 25   26  27  28   29   30   31
    //  0, ONE, ONE, 0, ONE, ONE, ONE, 0

    for (n = 0; n < BCH_K; ++n) {
        // one line here for every '1' bit in coeff (above)
        const int bit = firstBit - n;
        slices[20 - n] ^= slices[bit];
        slices[23 - n] ^= slices[bit];
        slices[25 - n] ^= slices[bit];
        slices[26 - n] ^= slices[bit];
        slices[28 - n] ^= slices[bit];
        slices[29 - n] ^= slices[bit];
        slices[30 - n] ^= slices[bit];
        slices[31 - n] ^= slices[bit];
    }

    // apply the parity mask we built up
    slices[BCH_N - BCH_K] |= paritymask;
}

static uint32_t
transpose_n(int n, uint32_t *matrix)
{
  uint32_t out = 0;
  int j;

  for (j = 0; j < 32; ++j) {
    if (matrix[j] & (1<<n)) {
      out |= (1<<j);
    }
  }

  return out;
}

static uint32_t *
transpose_clone(uint32_t src, uint32_t *out)
{
  int i;
  if (!out) { out = (uint32_t *)malloc(sizeof(uint32_t)*32); }
  for (i = 0; i < 32; ++i) {
    if (src & (1<<i)) {
      out[i] = 0xffffffff;
    } else {
      out[i] = 0;
    }
  }

  return out;
}


// This might not be elegant, yet effective!
// Error correction via bruteforce ;)
//
// It's a pragmatic solution since this was much faster to implement
// than understanding the math to solve it while being as effective.
// Besides that the overhead is neglectable.
int
sdr::pocsag_repair(uint32_t &data)
{
  // Check if data is correct
  if (0 == pocsag_syndrome(data)) { return 0; }

  int i, n, b1, b2;
  uint32_t res;
  uint32_t *xpose = 0, *in = 0;

  // check for single bit errors
  xpose = (uint32_t *) malloc(sizeof(uint32_t)*32);
  in = (uint32_t *) malloc(sizeof(uint32_t)*32);

  transpose_clone(data, xpose);
  for (i = 0; i < 32; ++i) { xpose[i] ^= (1<<i); }
  bitslice_syndrome(xpose);

  res = 0;
  for (i = 0; i < 32; ++i) { res |= xpose[i]; }
  res = ~res;

  if (res) {
    int n = 0;
    while (res) { ++n; res >>= 1; }
    --n;
    data ^= (1<<n);
    goto returnfree;
  }

  //check for two bit errors
  n = 0;
  transpose_clone(data, xpose);

  for (b1 = 0; b1 < 32; ++b1) {
    for (b2 = b1; b2 < 32; ++b2) {
      xpose[b1] ^= (1<<n);
      xpose[b2] ^= (1<<n);

      if (++n == 32) {
        memcpy(in, xpose, sizeof(uint32_t)*32);
        bitslice_syndrome(xpose);
        res = 0;
        for (i = 0; i < 32; ++i) { res |= xpose[i]; }
        res = ~res;
        if (res) {
          int n = 0;
          while (res) { ++n; res >>= 1; }
          --n;

          data = transpose_n(n, in);
          goto returnfree;
        }

        transpose_clone(data, xpose);
        n = 0;
      }
    }
  }

  if (n > 0) {
    memcpy(in, xpose, sizeof(uint32_t)*32);

    bitslice_syndrome(xpose);

    res = 0;
    for (i = 0; i < 32; ++i) { res |= xpose[i]; }
    res = ~res;

    if (res) {
      int n = 0;
      while (res) { ++n; res >>= 1; }
      --n;

      data = transpose_n(n, in);
      goto returnfree;
    }
  }

  if (xpose) { free(xpose); }
  if (in) { free(in); }
  return 1;

returnfree:
  if (xpose)
    free(xpose);
  if (in)
    free(in);
  return 0;
}

