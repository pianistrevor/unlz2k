#ifndef FILEREADING_H
#define FILEREADING_H

#include <fstream>
#include <iostream>
#include <istream>

using ENDIAN = std::endian;

// Read 32 bit unsigned integer from file
[[nodiscard]] inline unsigned int readUint32(std::ifstream &src,
                                             ENDIAN endianness) {
  unsigned int data;
  src.read(reinterpret_cast<char *>(&data), 4);
  if (ENDIAN::native != endianness) {
    return _byteswap_ulong(data);
  }
  return data;
}

// Read 16 bit unsigned integer from file
[[nodiscard]] inline unsigned short int readUint16(std::ifstream &src,
                                                   ENDIAN endianness) {
  unsigned short int data;
  src.read(reinterpret_cast<char *>(&data), 2);
  if (ENDIAN::native != endianness) {
    return _byteswap_ushort(data);
  }
  return data;
}

// Read 32 bit signed integer from file
[[nodiscard]] inline long int readInt32(std::ifstream &src, ENDIAN endianness) {
  int data;
  src.read(reinterpret_cast<char *>(&data), 4);
  if (ENDIAN::native != endianness) {
    return _byteswap_ulong(data);
  }
  return data;
}

// Read 16 bit signed integer from file
[[nodiscard]] inline short int readInt16(std::ifstream &src,
                                         ENDIAN endianness) {
  short int data;
  src.read(reinterpret_cast<char *>(&data), 2);
  if (ENDIAN::native != endianness) {
    return _byteswap_ushort(data);
  }
  return data;
}

#endif // FILEREADING_H