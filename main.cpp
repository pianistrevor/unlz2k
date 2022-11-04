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
  if (!src) {
    std::cerr << "Cannot open source file.\n";
    return 1;
  }
  std::ofstream dest;
  dest.open(destFilename, std::ios::out | std::ios::binary);
  if (!dest) {
    std::cerr << "Cannot open destination file.\n";
    return 1;
  }
  try {
    unlz2k(src, dest);
  } catch (int error) {
    std::cerr << "unlz2k exited with return code: " << error << '\n';
    return error;
  }
  std::cout << "Done.\n";
}