#ifndef __SDR_BCH31_21_HH__
#define __SDR_BCH31_21_HH__

#include <cinttypes>

namespace sdr {

/** Checks and repairs a POCSAG message with its
 * BCH(31,21) ECC. */
int pocsag_repair(uint32_t &data);

}

#endif // __SDR_BCH31_21_HH__
