/*
   Name: File Striping Module
   Description:
      Stripe files into smaller parts.
      File parts threshold = 124MB.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Global/global_definitions.h"
#include "../volume_management/file_mapping.h"
#include "file_striping.h"

void stripe(String dirpath, String fullpath, String filename){

  FILE *file_to_open;
  
  //size_t threshold = 536870912; /*124MB*/
  size_t bytes_read;

  int part_count = 1;
  char *buffer = malloc(STRIPE_SIZE); /*buffer to put data on*/

  String part_name;
  //String full;
  String comm;

  printf("File %s will be striped.\n", filename);
  //sprintf(full, "%s/%s", TEMP_LOC, fullpath);
  sprintf(comm, "rm '%s'", fullpath);

  file_to_open = fopen(fullpath, "rb");

  //printf("PATH : %s\n", fullpath);
  //printf("FILENAME : %s\n", filename);


  if (file_to_open == NULL){
    printf("fopen() unsuccessful!\n");
  }

  //String rootdir = "";
  //sprintf(rootdir, "%s/%s", TEMP_LOC, fullpath);
  //strcpy(rootdir, fullpath);
  //sprintf(rootdir, "%s", fullpath);
  //printf("ROOTDIR: %s\n", rootdir);

  //memmove(rootdir,rootdir+strlen(filename),1+strlen(rootdir+strlen(filename)));

  String path;
  sprintf(path, "%s/%s", TEMP_LOC, dirpath);
  
  
  memmove(filename, filename+strlen(dirpath), 1 +strlen(filename+strlen(dirpath)));

  while ((bytes_read = fread(buffer, sizeof(char), STRIPE_SIZE, file_to_open)) > 0){
       sprintf(part_name, "%spart%d.%s", path, part_count, filename);
       //printf("PARTNAME : %s\n", part_name);
       FILE *file_to_write = fopen(part_name, "wb");
       fwrite(buffer, 1, bytes_read, file_to_write);
       fclose(file_to_write);
       //String mapfile;
       //sprintf(mapfile, "part%d.%s", part_count, filename);
       String part_file = "";
       sprintf(part_file, "%s", part_name);
       memmove(part_file,part_file+strlen("/mnt/CVFSTemp/"),1+strlen(part_file+strlen("/mnt/CVFSTemp/")));
       //printf("Filename after memmove: %s\n", part_name);
       file_map(part_name, part_file);
       part_count++;
  }
  free(buffer);
  system(comm);
  fclose(file_to_open);

}
