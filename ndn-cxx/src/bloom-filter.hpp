#ifndef _NDN_BLOOM_FILTER_H_
#define _NDN_BLOOM_FILTER_H_

#include <vector>
#include <cstdint>

class BloomFilter {
public:
  BloomFilter();
  BloomFilter(uint64_t size, uint8_t numHashes);
  void add(uint64_t data);
  static BloomFilter merge(BloomFilter b1, BloomFilter b2);
  bool possiblyContains(uint64_t data) const;
  bool matchLID(BloomFilter b);

  uint64_t getSize();
  uint8_t getNumHashes();
  void setSize(uint64_t s);
  void setNumHashes(uint8_t n);

private:
  uint8_t m_numHashes;
  std::vector<bool> m_bits;
};

#endif
