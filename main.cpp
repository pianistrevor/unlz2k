#include "unlz2k.hpp"
#include <cstring>
#include <iostream>
#include <istream>
#include <vector>

// This is just a test program to use on a single file.
// Give it one command-line argument with the file name.
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Please specify a filename.\n";
    return 1;
  }
  char *filename = argv[1];
  char destFilename[256];
  strcpy(destFilename, filename);
  strcat(destFilename, ".dec");
  std::ifstream src;
  src.open(filename, std::ios::in | std::ios::binary);
  std::ofstream dest;
  dest.open(destFilename, std::ios::out | std::ios::binary);
  unlz2k(src, dest);
  std::cout << "Done.\n";
}