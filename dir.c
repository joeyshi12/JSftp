#include "dir.h"
#include <string.h>


int listFiles(int fd, char * directory) {

  // Get resources to see if the directory can be opened for reading
  
  DIR * dir = NULL;
  
  dir = opendir(directory);
  if (!dir) return -1;
  
  // Setup to read the directory. When printing the directory
  // only print regular files and directories. 

  struct dirent *dirEntry;
  int entriesPrinted = 0;

  char buf[4096];
  for (dirEntry = readdir(dir); dirEntry; dirEntry = readdir(dir)) {
    if (dirEntry->d_name[0] == '.') {
      continue;
    }
    if (dirEntry->d_type == DT_REG) {  // Regular file
      struct stat buf;

      // This call really should check the return value
      // stat returns a structure with the properties of a file
      stat(dirEntry->d_name, &buf);
      
      dprintf(fd, "-r--r--r--    0 1        0  %12d Jan 1  2022 %s\r\n", buf.st_size, dirEntry->d_name);
    } else if (dirEntry->d_type == DT_DIR) { // Directory
      dprintf(fd, "dr--r--r--    0 1        0  %12d Jan 1  2022 %s\r\n", 0, dirEntry->d_name);
    } else {
      dprintf(fd, "lr--r--r--    0 1        0  %12d Jan 1  2022 %s\r\n", 0, dirEntry->d_name);
    }
    entriesPrinted++;
  }
  
  // Release resources
  closedir(dir);
  return entriesPrinted;
}

   
