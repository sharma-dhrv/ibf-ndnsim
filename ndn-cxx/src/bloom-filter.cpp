#include <cstdint>

#include "bloom-filter.hpp"
#include "murmur3.hpp"

#include "ns3/log.h"
NS_LOG_COMPONENT_DEFINE("ndn-cxx.Face");

using namespace std;

BloomFilter::BloomFilter()
      :m_numHashes(10),
        m_bits(64)
{
  
}

BloomFilter::BloomFilter(uint64_t size, uint8_t numHashes)
      : m_numHashes(numHashes),
        m_bits(size)
{
}

std::array<uint64_t,2> hashbf(const uint8_t *data, std::size_t len)
{
  std::array<uint64_t, 2> hashValue;
  MurmurHash3_x64_128(data, len, 0, hashValue.data());
  return hashValue;
}

inline uint64_t nthHash(uint8_t n, uint64_t hashA, uint64_t hashB, uint64_t filterSize) 
{
    return (hashA + n * hashB) % filterSize;
}

void BloomFilter::add(const uint8_t *data, std::size_t len) 
{
  auto hashValues = hashbf(data, len);

  for (int n = 0; n < m_numHashes; n++) 
  {
      m_bits[nthHash(n, hashValues[0], hashValues[1], m_bits.size())] = true;
  }
}

bool BloomFilter::possiblyContains(const uint8_t *data, std::size_t len) const 
{
  auto hashValues = hashbf(data, len);

  for (int n = 0; n < m_numHashes; n++) 
  {
      if (!m_bits[nthHash(n, hashValues[0], hashValues[1], m_bits.size())]) 
      {
          return false;
      }
  }

  return true;
}

uint64_t BloomFilter::getSize()
{
  return m_bits.size();
}

uint8_t BloomFilter::getNumHashes()
{
  return m_numHashes;
}

void BloomFilter::setNumHashes(uint8_t n)
{
  m_numHashes = n;
}

bool BloomFilter::matchLID(BloomFilter b)
{
  std::vector<bool> ibf = m_bits;
  std::vector<bool> lid = b.m_bits;

  uint64_t ibfSize = ibf.size();
  uint64_t lidSize = lid.size();

  if (ibfSize != lidSize)
  {
    NS_LOG_INFO("Bloom filters to be compared are of different length");
    return false;
  }

  std::vector<bool>::iterator ibf_it = ibf.begin();
  std::vector<bool>::iterator lid_it = lid.begin();

  for (;ibf_it != ibf.end() && lid_it != lid.end(); ++ibf_it, ++lid_it)
  {
    if (*ibf_it && *lid_it != *lid_it)
    {
      return false;
    }
  }

  return true;
}
