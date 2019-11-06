#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>


int main()
{
  char str[] = "Geeks for Geeks";
  char *token = strtok(str, " ");

  strcpy(str, strtok(NULL, ""));
  printf("%s\n", str);

  return 0;
}
