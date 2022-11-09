#ifndef FILEREADING_H
#define FILEREADING_H

#include <fstream>
#include <istream>

using ENDIAN = std::endian;

[[nodiscard]] inline unsigned int byteswap32(unsigned int data) {
    return (data >> 24) | (data << 24) | (data >> 8 & 0xFF00) | (data << 8 & 0xFF0000);
}

[[nodiscard]] inline unsigned short int byteswap16(unsigned short int data) {
    return (data >> 8) | (data << 8);
}

// Read 32 bit unsigned integer from file
[[nodiscard]] inline unsigned int readUint32(std::ifstream& src,
    ENDIAN endianness) {
    unsigned int data;
    src.read(reinterpret_cast<char*>(&data), 4);
    if (ENDIAN::native != endianness) {
        return byteswap32(data);
    }
    return data;
}

// Read 16 bit unsigned integer from file
[[nodiscard]] inline unsigned short int readUint16(std::ifstream& src,
    ENDIAN endianness) {
    unsigned short int data;
    src.read(reinterpret_cast<char*>(&data), 2);
    if (ENDIAN::native != endianness) {
        return byteswap16(data);
    }
    return data;
}

// Read 32 bit signed integer from file
[[nodiscard]] inline long int readInt32(std::ifstream& src, ENDIAN endianness) {
    unsigned int data;
    src.read(reinterpret_cast<char*>(&data), 4);
    if (ENDIAN::native != endianness) {
        return static_cast<int>(byteswap32(data));
    }
    return data;
}

// Read 16 bit signed integer from file
[[nodiscard]] inline short int readInt16(std::ifstream& src,
    ENDIAN endianness) {
    unsigned short int data;
    src.read(reinterpret_cast<char*>(&data), 2);
    if (ENDIAN::native != endianness) {
        return static_cast<short int>(byteswap16(data));
    }
    return data;
}

#endif // FILEREADING_H