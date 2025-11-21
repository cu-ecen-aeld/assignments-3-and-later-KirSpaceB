#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

char userInputOfFilePath[1000];
char userInputOfFileName[1000];

FILE *createdFile;

/*
 * Validate arguments and fill in the global buffers.
 * Return 0 on success, non-zero on error.
 */
int checkUserNamePass(int argc, char *argv[]);

int main(int argc, char *argv[]) {
  openlog("writer.c app", LOG_PID, LOG_USER);
  syslog(LOG_INFO, "PROGRAM STARTED");

  /* Validate arguments and fill in our buffers */
  if (checkUserNamePass(argc, argv) != 0) {
    closelog();
    return 1;
  }

  /* Open the file for writing */
  createdFile = fopen(userInputOfFilePath, "w");
  if (createdFile == NULL) {
    syslog(LOG_ERR, "File is null when opening path: %s", userInputOfFilePath);
    perror("Error opening file");
    closelog();
    return 1;
  }

  /* Write the string to the file */
  if (fprintf(createdFile, "%s", userInputOfFileName) < 0) {
    syslog(LOG_ERR, "Error writing to file: %s", userInputOfFilePath);
    fclose(createdFile);
    closelog();
    return 1;
  }

  fclose(createdFile);
  syslog(LOG_INFO, "Successfully wrote string to file");
  closelog();
  return 0;
}

int checkUserNamePass(int argc, char *argv[]) {
  /* Expect exactly 2 user arguments: <file path> <string> */
  if (argc != 3) {
    syslog(LOG_ERR,
           "Invalid number of arguments: %d, expected 2 (file path and string)",
           argc - 1);
    fprintf(stderr, "Usage: writer <file path> <string to write>\n");
    return 1;
  }

  /* Copy argv into the global buffers (your original style) */
  strncpy(userInputOfFilePath, argv[1], sizeof(userInputOfFilePath) - 1);
  userInputOfFilePath[sizeof(userInputOfFilePath) - 1] = '\0';

  strncpy(userInputOfFileName, argv[2], sizeof(userInputOfFileName) - 1);
  userInputOfFileName[sizeof(userInputOfFileName) - 1] = '\0';

  syslog(LOG_DEBUG, "Received file path: %s", userInputOfFilePath);
  syslog(LOG_DEBUG, "Received write string: %s", userInputOfFileName);

  return 0;
}
