#ifndef UNLZ2K_H
#define UNLZ2K_H

#include <fstream>

size_t unlz2k(std::ifstream &, std::ofstream &);
size_t unlz2k(std::ifstream &, std::ofstream &, size_t, size_t);

#endif // UNLZ2K_H