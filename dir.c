#include "dir.h"


int listFiles(int fd, char * directory) {
  DIR * dir = NULL;

  dir = opendir(directory);
  if (!dir) return -1;

  // Setup to read the directory. When printing the directory
  // only print regular files and directories.

  struct dirent *dirEntry;
  int entriesPrinted = 0;
  struct stat buf;
  for (dirEntry = readdir(dir); dirEntry; dirEntry = readdir(dir)) {
    dprintf(fd, "%s\r\n", dirEntry->d_name);
    entriesPrinted++;
  }

  // Release resources
  closedir(dir);
  return entriesPrinted;
}
