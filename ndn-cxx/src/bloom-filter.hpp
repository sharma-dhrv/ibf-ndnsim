#ifndef _NDN_BLOOM_FILTER_H_
#define _NDN_BLOOM_FILTER_H_

#include <vector>
#include <cstdint>

class BloomFilter {
public:
  BloomFilter();
  BloomFilter(uint64_t size, uint8_t numHashes);
  void add(const uint8_t *data, std::size_t len);
  bool possiblyContains(const uint8_t *data, std::size_t len) const;
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
