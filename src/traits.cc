#include "traits.hh"

using namespace sdr;


const float Traits<uint8_t>::scale = 127;
const size_t Traits<uint8_t>::shift = 8;
const float Traits< std::complex<uint8_t> >::scale = 127;
const size_t Traits< std::complex<uint8_t> >::shift = 8;

const float Traits<int8_t>::scale = 127;
const size_t Traits<int8_t>::shift = 8;
const float Traits< std::complex<int8_t> >::scale = 127;
const size_t Traits< std::complex<int8_t> >::shift = 8;

const float Traits<uint16_t>::scale = 32767;
const size_t Traits<uint16_t>::shift = 16;
const float Traits< std::complex<uint16_t> >::scale = 32767;
const size_t Traits< std::complex<uint16_t> >::shift = 16;

const float Traits<int16_t>::scale = 32767;
const size_t Traits<int16_t>::shift = 16;
const float Traits< std::complex<int16_t> >::scale = 32767;
const size_t Traits< std::complex<int16_t> >::shift = 16;

const float Traits<float>::scale = 1;
const size_t Traits<float>::shift = 0;
const float Traits< std::complex<float> >::scale = 1;
const size_t Traits< std::complex<float> >::shift = 0;

const float Traits<double>::scale = 1;
const size_t Traits<double>::shift = 0;
const float Traits< std::complex<double> >::scale = 1;
const size_t Traits< std::complex<double> >::shift = 0;

