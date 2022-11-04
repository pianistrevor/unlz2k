#include "filereading.h"
#include "pch.h"
#include "unlz2k.h"
#include <bit>
#include <format>
#include <iostream>

using ENDIAN = std::endian;

constexpr auto _MAX_CHUNK_SIZE = 0x40000;
constexpr auto _LZ2K = 0x4C5A324B;

// Globals that are used by other functions

char compressedFile[_MAX_CHUNK_SIZE];
size_t tmpSrcOffs;
size_t tmpSrcSize;
size_t tmpDestSize;
uint32_t bitstream;
uint8_t lastByteRead;
uint8_t previousBitAlign;
uint16_t chunksWithCurrentSetupLeft;
uint32_t readOffset;
int32_t literalsToCopy;
uint8_t tmpChunk[8192];
uint8_t smallByteDict[20];
uint8_t largeByteDict[510];
uint16_t smallWordDict[256];
uint16_t parallelDict0[1024];
uint16_t parallelDict1[1024];
uint16_t largeWordDict[4096];

size_t unlz2k_chunk(std::ifstream&, std::ofstream&, size_t, size_t);
void loadIntoBitstream(uint8_t);
uint32_t decodeBitstream();
uint32_t decodeBitstreamForLiterals();
void fillSmallDicts(uint8_t, uint8_t, uint8_t);
void fillLargeDicts();
void fillWordsUsingBytes(uint16_t, uint8_t*, uint8_t, uint16_t*);
void readAndDecrypt(size_t, uint8_t*);
void writeToFile(std::ofstream&, uint8_t*, size_t);

// This version does not check integrity. It relies on the rest of the
// program!
size_t unlz2k(std::ifstream& src, std::ofstream& dest) {
    src.seekg(0, std::ios::end);
    auto fileEnd = src.tellg();
    src.seekg(0);
    size_t bytesWritten = 0;
    while (src.tellg() < fileEnd) {
        uint32_t header = readUint32(src, ENDIAN::big);
        if (header != _LZ2K) {
            std::cerr << "Not valid LZ2K file or chunk at pos "
                << static_cast<size_t>(src.tellg()) - 4 << "\n";
            throw 2;
        }
        uint32_t unpacked = readUint32(src, ENDIAN::little);
        uint32_t packed = readUint32(src, ENDIAN::little);
        bytesWritten += unlz2k_chunk(src, dest, packed, unpacked);
    }
    return bytesWritten;
}
size_t unlz2k(std::ifstream& src, std::ofstream& dest, size_t srcSize,
    size_t destSize) {
    size_t bytesWritten = 0;
    while (bytesWritten < destSize) {
        uint32_t header = readUint32(src, ENDIAN::big);
        if (header != _LZ2K) {
            std::cerr << "Not valid LZ2K file or chunk at pos "
                << static_cast<size_t>(src.tellg()) - 4 << "\n";
            throw 2;
        }
        uint32_t unpacked = readUint32(src, ENDIAN::little);
        uint32_t packed = readUint32(src, ENDIAN::little);
        bytesWritten += unlz2k_chunk(src, dest, packed, unpacked);
    }
    if (src.tellg() != srcSize) {
        std::cerr << std::format("Expected to read 0x%08x bytes, but wrote 0x%08x",
            srcSize, static_cast<size_t>(src.tellg()));
        throw 3;
    }
    if (bytesWritten != destSize) {
        std::cerr << std::format("Expected to write 0x%08x bytes, but wrote 0x%08x",
            destSize, bytesWritten);
        throw 3;
    }
    return bytesWritten;
}

size_t unlz2k_chunk(std::ifstream& src, std::ofstream& dest, size_t srcSize,
    size_t destSize) {
    if (!destSize) {
        return 0;
    }
    // Read file into memory location
    src.read(compressedFile, srcSize);
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
    if (bits > previousBitAlign) {
        do {
            bits -= previousBitAlign;
            bitstream |= lastByteRead << bits;
            if (tmpSrcOffs == tmpSrcSize) {
                lastByteRead = 0;
            }
            else {
                lastByteRead = compressedFile[tmpSrcOffs++];
            }
            previousBitAlign = 8;
        } while (bits > previousBitAlign);
    }
    previousBitAlign -= bits;
    bitstream |= lastByteRead >> previousBitAlign;
}

void readAndDecrypt(size_t chunkSize, uint8_t* out) {
    uint32_t outputOffs = 0;
    --literalsToCopy;
    while (literalsToCopy >= 0) {
        out[outputOffs++] = out[readOffset++];
        readOffset &= 0x1FFF;
        if (outputOffs == chunkSize) {
            return;
        }
        literalsToCopy--;
    }
    while (outputOffs < chunkSize) {
        uint32_t tmpVal = decodeBitstream();
        if (tmpVal <= 255) {
            out[outputOffs++] = tmpVal;
            if (outputOffs == chunkSize)
                return;
        }
        else {
            uint32_t lbOffs = decodeBitstreamForLiterals();
            readOffset = (outputOffs - lbOffs - 1) & 0x1FFF;
            literalsToCopy = tmpVal - 254;
            while (literalsToCopy >= 0) {
                out[outputOffs++] = out[readOffset++];
                readOffset &= 0x1FFF;
                if (outputOffs == chunkSize)
                    return;
                literalsToCopy--;
            }
        }
    }
    if (outputOffs > chunkSize) {
        std::cerr << "Error: read farther than given length\n";
        throw 4;
    }
}

void writeToFile(std::ofstream& outFile, uint8_t* data, size_t length) {
    outFile.write(reinterpret_cast<char*>(data), length);
}

[[nodiscard]] uint32_t decodeBitstream() {
    if (!chunksWithCurrentSetupLeft) {
        chunksWithCurrentSetupLeft = bitstream >> 16;
        loadIntoBitstream(16);
        fillSmallDicts(19, 5, 3);
        fillLargeDicts();
        fillSmallDicts(14, 4, -1);
    }
    chunksWithCurrentSetupLeft--;
    uint16_t tmpVal = largeWordDict[bitstream >> 20];
    if (tmpVal >= 510) {
        uint32_t mask = 0x80000;
        do {
            if (!(bitstream & mask)) {
                tmpVal = parallelDict0[tmpVal];
            }
            else {
                tmpVal = parallelDict1[tmpVal];
            }
            mask >>= 1;
        } while (tmpVal >= 510);
    }
    uint8_t bits = largeByteDict[tmpVal];
    loadIntoBitstream(bits);
    return tmpVal;
}

[[nodiscard]] uint32_t decodeBitstreamForLiterals() {
    uint8_t tmpOffs = bitstream >> 24;
    uint16_t tmpVal = smallWordDict[tmpOffs];
    if (tmpVal >= 14) {
        uint32_t mask = 0x800000;
        do {
            if (!(bitstream & mask)) {
                tmpVal = parallelDict0[tmpVal];
            }
            else {
                tmpVal = parallelDict1[tmpVal];
            }
            mask >>= 1;
        } while (tmpVal >= 14);
    }
    uint8_t bits = smallByteDict[tmpVal];
    loadIntoBitstream(bits);
    if (!tmpVal)
        return 0;
    if (tmpVal == 1)
        return 2;
    tmpVal--;
    uint32_t tmpBitstream = bitstream >> (32 - tmpVal);
    loadIntoBitstream(static_cast<uint8_t>(tmpVal));
    return tmpBitstream + (1 << tmpVal);
}

void fillSmallDicts(uint8_t length, uint8_t bits, uint8_t specialInd) {
    uint32_t tmpVal = bitstream >> (32 - bits);
    loadIntoBitstream(bits);
    if (!tmpVal) {
        tmpVal = bitstream >> (32 - bits);
        loadIntoBitstream(bits);
        if (length > 0) {
            memset(smallByteDict, 0, length);
        }
        for (int i = 0; i < 256; ++i) {
            smallWordDict[i] = tmpVal;
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
            }
            else {
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
        smallByteDict[tmpVal2++] = tmpByte;
        if (tmpVal2 == specialInd) {
            uint8_t specialLen = bitstream >> 30;
            loadIntoBitstream(2);
            if (specialLen) {
                memset(smallByteDict + tmpVal2, 0, specialLen);
                tmpVal2 += specialLen;
            }
        }
    }
    if (tmpVal2 < length) {
        memset(smallByteDict + tmpVal2, 0, length - static_cast<size_t>(tmpVal2));
    }
    fillWordsUsingBytes(length, smallByteDict, 8, smallWordDict);
    return;
}

void fillLargeDicts() {
    int16_t tmpVal = bitstream >> 23;
    loadIntoBitstream(9);
    if (!tmpVal) {
        tmpVal = bitstream >> 23;
        loadIntoBitstream(9);
        memset(largeByteDict, 0, 510);
        for (int i = 0; i < 4096; ++i) {
            largeWordDict[i] = tmpVal;
        }
        return;
    }
    uint16_t bytes = 0;
    if (tmpVal < 0) {
        // Does this ever happen?
        memset(largeByteDict, 0, 510);
        fillWordsUsingBytes(510, largeByteDict, 12, largeWordDict);
        return;
    }
    while (bytes < tmpVal) {
        uint8_t tmpLen = bitstream >> 24;
        uint16_t tmpVal2 = smallWordDict[tmpLen];
        if (tmpVal2 >= 19) {
            uint32_t mask = 0x800000;
            do {
                if (!(bitstream & mask)) {
                    tmpVal2 = parallelDict0[tmpVal2];
                }
                else {
                    tmpVal2 = parallelDict1[tmpVal2];
                }
                mask >>= 1;
            } while (tmpVal2 >= 19);
        }
        uint8_t bits = smallByteDict[tmpVal2];
        loadIntoBitstream(bits);
        if (tmpVal2 > 2) {
            tmpVal2 -= 2;
            largeByteDict[bytes++] = static_cast<uint8_t>(tmpVal2);
        }
        else {
            if (!tmpVal2) {
                tmpLen = 1;
            }
            else if (tmpVal2 == 1) {
                tmpVal2 = bitstream >> 28;
                loadIntoBitstream(4);
                tmpLen = tmpVal2 + 3;
            }
            else {
                tmpVal2 = bitstream >> 23;
                loadIntoBitstream(9);
                tmpLen = tmpVal2 + 20;
            }
            if (tmpLen) {
                memset(largeByteDict + bytes, 0, tmpLen);
                bytes += tmpLen;
            }
        }
    }
    if (bytes < 510) {
        memset(largeByteDict + bytes, 0, static_cast<size_t>(510) - bytes);
    }
    fillWordsUsingBytes(510, largeByteDict, 12, largeWordDict);
    return;
}

void fillWordsUsingBytes(uint16_t bytesLen, uint8_t* bytes, uint8_t pivot,
    uint16_t* words) {
    uint16_t srcDict[17] = { 0 };
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
        destDict[ind - 2] = high;
        low = srcDict[ind - 2] << (shift - 1);
        low += high;
        high = srcDict[ind - 1] << (shift - 2);
        high += low;
        destDict[ind - 1] = low;
        destDict[ind] = high;
        shift -= 4;
    }
    if (destDict[17]) {
        std::cerr << "Bad table\n";
        throw 5;
    }
    shift = pivot - 1;
    uint8_t tmpVal = 16 - pivot;
    uint8_t tmpValCopy = tmpVal;
    for (int i = 1; i <= pivot; ++i) {
        destDict[i] >>= tmpValCopy;
        srcDict[i] = 1 << shift--;
    }
    tmpValCopy--;
    for (int i = pivot + 1; i <= 16; ++i) {
        srcDict[i] = 1 << tmpValCopy--;
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
                uint16_t* dictPtr = words;
                uint16_t tmpOffs = destVal >> tmpVal;
                uint8_t newLen = tmpByte - pivot;
                while (newLen) {
                    if (!dictPtr[tmpOffs]) {
                        parallelDict0[bytesLenCopy] = 0;
                        parallelDict1[bytesLenCopy] = 0;
                        dictPtr[tmpOffs] = bytesLenCopy++;
                    }
                    tmpOffs = dictPtr[tmpOffs];
                    if (!(destVal & mask)) {
                        dictPtr = parallelDict0;
                    }
                    else {
                        dictPtr = parallelDict1;
                    }
                    destVal += destVal;
                    newLen--;
                }
                dictPtr[tmpOffs] = i;
            }
            else if (destVal < srcVal) {
                for (int j = destVal; j < srcVal; ++j) {
                    words[j] = i;
                }
            }
            destDict[tmpByte] = srcVal;
        }
    }
}