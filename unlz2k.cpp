#include "unlz2k.hpp"
#include <cstring>
#include <iostream>

// Globals that are used by other functions

#define MAX_CHUNK_SIZE 0x40000

uint8_t *compressedFile;
size_t tmpSrcOffs;
size_t tmpSrcSize;
size_t tmpDestSize;
uint32_t bitstream;
uint8_t lastByteRead;
uint8_t previousBitAlign;
uint16_t chunksWithCurrentSetupLeft;
uint32_t readOffset;
uint32_t literalsToCopy;
uint8_t tmpChunk[8192];
uint8_t byteDict0[510];
uint8_t byteDict1[20];
uint16_t wordDict0[256];
uint16_t wordDict1[1024];
uint16_t wordDict2[1024];
uint16_t wordDict3[4096];
const char *lz2k = "LZ2K";
uint32_t tmpCounter = 0;

size_t unlz2k_chunk(std::ifstream &, std::ofstream &, size_t, size_t);
void loadIntoBitstream(uint8_t);
uint32_t decodeBitstream();
uint32_t decodeBitstreamForLiterals();
void setupByteAndWordDicts(uint8_t, uint8_t, uint8_t);
void setupByteDict0();
void processDicts(uint16_t, uint8_t *, uint8_t, uint16_t *);
void readAndDecrypt(size_t, uint8_t *);
void writeToFile(std::ofstream &, uint8_t *, size_t);

uint32_t readUint32(std::ifstream &src) {
  uint8_t data[4];
  src.read((char *)data, sizeof(data));
  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

uint16_t readUint16(std::ifstream &src) {
  char data[2];
  src.read(data, sizeof(data));
  return data[0] | (data[1] << 8);
}

size_t unlz2k(std::ifstream &src, std::ofstream &dest, size_t srcSize,
              size_t destSize) {
  // Allocate chunk for compressed file
  compressedFile = new uint8_t[MAX_CHUNK_SIZE];
  size_t bytesWritten = 0;
  while (bytesWritten < destSize) {
    char lz2k[5];
    src.read(lz2k, 4);
    lz2k[4] = '\0';
    if (strcmp(lz2k, "LZ2K") != 0) {
      std::cerr << "Not valid LZ2K file or chunk at pos " << src.tellg()
                << "\n";
      return 1;
    }
    uint32_t unpacked = readUint32(src);
    uint32_t packed = readUint32(src);
    bytesWritten += unlz2k_chunk(src, dest, packed, unpacked);
  }
  if (src.tellg() != srcSize) {
    printf("Expected to read 0x%08x bytes, but wrote 0x%08x", srcSize,
           src.tellg());
    return 1;
  }
  if (bytesWritten != destSize) {
    printf("Expected to write 0x%08x bytes, but wrote 0x%08x", destSize,
           bytesWritten);
    return 1;
  }
  return 0;
}

size_t unlz2k_chunk(std::ifstream &src, std::ofstream &dest, size_t srcSize,
                    size_t destSize) {
  if (!destSize) {
    return 0;
  }
  // Read file into memory location
  src.read((char *)compressedFile, srcSize);
  tmpSrcOffs = 0;
  tmpSrcSize = srcSize;
  bitstream = 0;
  lastByteRead = 0;
  previousBitAlign = 0;
  chunksWithCurrentSetupLeft = 0;
  readOffset = 0;
  literalsToCopy = 0;
  size_t bytesLeft = destSize;
  size_t bytesWritten = 0;
  loadIntoBitstream(32);
  while (bytesLeft > 0) {
    size_t chunkSize = (bytesLeft < 8192 ? bytesLeft : 8192);
    readAndDecrypt(chunkSize, tmpChunk);
    writeToFile(dest, tmpChunk, chunkSize);
    bytesWritten += chunkSize;
    bytesLeft -= chunkSize;
  }
  return bytesWritten;
}

void loadIntoBitstream(uint8_t bits) {
  bitstream <<= bits;
  uint8_t prev = previousBitAlign;
  uint32_t data = lastByteRead;
  if (bits > prev) {
    do {
      bits -= prev;
      bitstream |= data << bits;
      if (tmpSrcOffs == tmpSrcSize) {
        data = 0;
      } else {
        data = compressedFile[tmpSrcOffs++];
      }
      prev = 8;
    } while (bits > prev);
    lastByteRead = data;
  }
  prev -= bits;
  bits = prev;
  data >>= bits;
  previousBitAlign = prev;
  bitstream |= data;
}

void readAndDecrypt(size_t chunkSize, uint8_t *out) {
  uint32_t outputOffs = 0;
  uint32_t tmpReadOffs = readOffset;
  int32_t toCopy = --literalsToCopy;
  if (toCopy >= 0) {
    do {
      out[outputOffs++] = out[tmpReadOffs++];
      tmpReadOffs &= 0x1FFF;
      if (outputOffs == chunkSize) {
        literalsToCopy = toCopy;
        readOffset = tmpReadOffs;
        return;
      }
      toCopy--;
    } while (toCopy >= 0);
    literalsToCopy = toCopy;
    readOffset = tmpReadOffs;
  }
  while (outputOffs < chunkSize) {
    uint32_t tmpVal = decodeBitstream();
    if (tmpVal <= 255) {
      out[outputOffs++] = tmpVal;
      if (outputOffs == chunkSize)
        return;
    } else {
      toCopy = decodeBitstreamForLiterals();
      tmpReadOffs = (outputOffs - toCopy - 1) & 0x1FFF;
      toCopy = tmpVal - 254;
      readOffset = tmpReadOffs;
      literalsToCopy = toCopy;
      while (toCopy >= 0) {
        out[outputOffs++] = out[tmpReadOffs++];
        tmpReadOffs &= 0x1FFF;
        readOffset = tmpReadOffs;
        if (outputOffs == chunkSize)
          return;
        toCopy--;
        literalsToCopy = toCopy;
      }
    }
  }
  if (outputOffs > chunkSize) {
    std::cerr << "Error: went farther than given length\n";
    return;
  }
}

void writeToFile(std::ofstream &outFile, uint8_t *data, size_t length) {
  outFile.write((const char *)data, length);
}

uint32_t decodeBitstream() {
  if (!chunksWithCurrentSetupLeft) {
    chunksWithCurrentSetupLeft = bitstream >> 16;
    loadIntoBitstream(16);
    setupByteAndWordDicts(19, 5, 3);
    setupByteDict0();
    setupByteAndWordDicts(14, 4, -1);
  }
  chunksWithCurrentSetupLeft--;
  uint16_t tmpVal = wordDict3[bitstream >> 20];
  if (tmpVal >= 510) {
    uint32_t mask = 0x80000;
    do {
      if (!(bitstream & mask)) {
        tmpVal = wordDict1[tmpVal];
      } else {
        tmpVal = wordDict2[tmpVal];
      }
      mask >>= 1;
    } while (tmpVal >= 510);
  }
  uint8_t bits = byteDict0[tmpVal];
  loadIntoBitstream(bits);
  return tmpVal;
}

uint32_t decodeBitstreamForLiterals() {
  uint8_t tmpOffs = bitstream >> 24;
  uint16_t tmpVal = wordDict0[tmpOffs];
  if (tmpVal >= 14) {
    uint32_t mask = 0x800000;
    do {
      if (!(bitstream & mask)) {
        tmpVal = wordDict1[tmpVal];
      } else {
        tmpVal = wordDict2[tmpVal];
      }
      mask >>= 1;
    } while (tmpVal >= 14);
  }
  uint8_t bits = byteDict1[tmpVal];
  loadIntoBitstream(bits);
  if (!tmpVal)
    return 0;
  if (tmpVal == 1)
    return 2;
  tmpVal--;
  uint32_t tmpBitstream = bitstream >> (32 - tmpVal);
  loadIntoBitstream(tmpVal);
  return tmpBitstream + (1 << tmpVal);
}

void setupByteAndWordDicts(uint8_t length, uint8_t bits, uint8_t specialInd) {
  uint32_t tmpVal = bitstream >> (32 - bits);
  loadIntoBitstream(bits);
  if (!tmpVal) {
    tmpVal = bitstream >> (32 - bits);
    loadIntoBitstream(bits);
    if (length > 0) {
      memset(byteDict1, 0, length);
    }
    for (int i = 0; i < 256; ++i) {
      wordDict0[i] = tmpVal;
    }
    return;
  }
  uint32_t tmpVal2 = 0;
  while (tmpVal2 < tmpVal) {
    uint8_t tmpByte = bitstream >> 29;
    uint8_t bits = 3;
    if (tmpByte == 7) {
      uint32_t mask = 0x10000000;
      if (!(bitstream & mask)) {
        bits = 4;
      } else {
        uint8_t counter = 0;
        do {
          mask >>= 1;
          counter++;
        } while (bitstream & mask);
        bits = counter + 4;
        tmpByte += counter;
      }
    }
    loadIntoBitstream(bits);
    byteDict1[tmpVal2++] = tmpByte;
    if (tmpVal2 == specialInd) {
      size_t specialLen = bitstream >> 30;
      loadIntoBitstream(2);
      if (specialLen) {
        memset(byteDict1 + tmpVal2, 0, specialLen);
        tmpVal2 += specialLen;
      }
    }
  }
  if (tmpVal2 < length) {
    memset(byteDict1 + tmpVal2, 0, length - tmpVal2);
  }
  processDicts(length, byteDict1, 8, wordDict0);
  return;
}

void setupByteDict0() {
  int16_t tmpVal = bitstream >> 23;
  loadIntoBitstream(9);
  if (!tmpVal) {
    tmpVal = bitstream >> 23;
    loadIntoBitstream(9);
    memset(byteDict0, 0, 510);
    for (int i = 0; i < 4096; ++i) {
      wordDict3[i] = tmpVal;
    }
    return;
  }
  uint16_t bytes = 0;
  if (tmpVal < 0) {
    memset(byteDict0, 0, 510);
    processDicts(510, byteDict0, 12, wordDict3);
    return;
  }
  while (bytes < tmpVal) {
    uint8_t tmpLen = bitstream >> 24;
    uint16_t tmpVal2 = wordDict0[tmpLen];
    if (tmpVal2 >= 19) {
      uint32_t mask = 0x800000;
      do {
        if (!(bitstream & mask)) {
          tmpVal2 = wordDict1[tmpVal2];
        } else {
          tmpVal2 = wordDict2[tmpVal2];
        }
        mask >>= 1;
      } while (tmpVal2 >= 19);
    }
    uint8_t bits = byteDict1[tmpVal2];
    loadIntoBitstream(bits);
    if (tmpVal2 > 2) {
      tmpVal2 -= 2;
      byteDict0[bytes++] = tmpVal2;
    } else {
      if (!tmpVal2) {
        tmpLen = 1;
      } else if (tmpVal2 == 1) {
        tmpVal2 = bitstream >> 28;
        loadIntoBitstream(4);
        tmpLen = tmpVal2 + 3;
      } else {
        tmpVal2 = bitstream >> 23;
        loadIntoBitstream(9);
        tmpLen = tmpVal2 + 20;
      }
      if (tmpLen) {
        memset(byteDict0 + bytes, 0, tmpLen);
        bytes += tmpLen;
      }
    }
  }
  if (bytes < 510) {
    memset(byteDict0 + bytes, 0, 510 - bytes);
  }
  processDicts(510, byteDict0, 12, wordDict3);
  return;
}

void processDicts(uint16_t bytesLen, uint8_t *bytes, uint8_t pivot,
                  uint16_t *words) {
  uint16_t srcDict[17] = {0};
  uint16_t destDict[18];
  destDict[1] = 0;
  for (int i = 0; i < bytesLen; ++i) {
    uint8_t tmp = bytes[i];
    srcDict[tmp]++;
  }
  uint8_t shift = 14, ind = 1;
  uint16_t low, high;
  while (ind <= 16) {
    low = srcDict[ind] << (shift + 1);
    high = srcDict[ind + 1] << shift;
    low += destDict[ind];
    ind += 4;
    high += low;
    destDict[ind - 3] = low;
    low = srcDict[ind - 2] << (shift - 1);
    destDict[ind - 2] = high;
    low += high;
    high = srcDict[ind - 1] << (shift - 2);
    destDict[ind - 1] = low;
    high += low;
    destDict[ind] = high;
    shift -= 4;
  }
  if (destDict[17]) {
    std::cerr << "Bad table\n";
  }
  shift = pivot - 1;
  uint8_t tmpVal = 16 - pivot;
  uint8_t tmpValCopy = tmpVal;
  for (int i = 1; i <= pivot; ++i) {
    destDict[i] >>= tmpVal;
    srcDict[i] = 1 << shift--;
  }
  tmpVal--;
  for (int i = pivot + 1; i <= 16; ++i) {
    srcDict[i] = 1 << tmpVal--;
  }
  uint16_t comp1 = destDict[pivot + 1];
  comp1 >>= 16 - pivot;
  if (comp1) {
    uint16_t comp2 = 1 << pivot;
    if (comp1 != comp2) {
      for (int i = comp1; i < comp2; ++i) {
        words[i] = 0;
      }
    }
  }
  if (bytesLen <= 0)
    return;
  shift = 15 - pivot;
  uint16_t mask = 1 << shift;
  uint16_t bytesLenCopy = bytesLen;
  for (int i = 0; i < bytesLen; ++i) {
    uint8_t tmpByte = bytes[i];
    if (tmpByte) {
      uint16_t destVal = destDict[tmpByte];
      uint16_t srcVal = srcDict[tmpByte] + destVal;
      if (tmpByte > pivot) {
        uint16_t *dictPtr = words;
        uint16_t tmpOffs = destVal >> tmpValCopy;
        uint8_t newLen = tmpByte - pivot;
        while (newLen) {
          if (!dictPtr[tmpOffs]) {
            wordDict1[bytesLenCopy] = 0;
            wordDict2[bytesLenCopy] = 0;
            dictPtr[tmpOffs] = bytesLenCopy++;
          }
          tmpOffs = dictPtr[tmpOffs];
          if (!(destVal & mask)) {
            dictPtr = wordDict1;
          } else {
            dictPtr = wordDict2;
          }
          destVal += destVal;
          newLen--;
        }
        dictPtr[tmpOffs] = i;
      } else if (destVal < srcVal) {
        for (int j = destVal; j < srcVal; ++j) {
          words[j] = i;
        }
      }
      destDict[tmpByte] = srcVal;
    }
  }
}