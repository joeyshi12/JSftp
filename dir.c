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
    if (dirEntry->d_name[0] == '.') {
      continue;
    }
    char typechar;
    buf.st_size = 0;
    if (dirEntry->d_type == DT_REG) {
      stat(dirEntry->d_name, &buf);
      typechar = '-';
    } else if (dirEntry->d_type == DT_DIR) {
      typechar = 'd';
    } else {
      typechar = 'l';
    }
    // TODO: nice-to-have non-hardcoded fields here
    dprintf(fd, "%cr--r--r--    0 1        0  %12lld Jan 1  2022 %s\r\n", typechar, buf.st_size, dirEntry->d_name);
    entriesPrinted++;
  }

  // Release resources
  closedir(dir);
  return entriesPrinted;
}
