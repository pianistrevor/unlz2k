#ifndef UNLZ2K_H
#define UNLZ2K_H

#ifdef UNLZ2K_EXPORTS
#define UNLZ2K_API __declspec(dllexport)
#else
#define UNLZ2K_API __declspec(dllimport)
#endif

#include <fstream>

extern UNLZ2K_API size_t unlz2k(std::ifstream &src, std::ofstream &dest);
extern UNLZ2K_API size_t unlz2k(std::ifstream &src, std::ofstream &dest, size_t srcSize, size_t destSize);

#endif // UNLZ2K_H