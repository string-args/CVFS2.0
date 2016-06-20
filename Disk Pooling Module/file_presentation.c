#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "../Global/global_definitions.h"
#include "../Utilities/cmd_exec.h"
#include "file_presentation.h"

static int callback(void *notUsed, int argc, char **argv, char **colname){
   int i;
   String comm;

   if (argc != 2){
      fprintf(stderr, "Error in database table: missing columns\n");
      exit(1);
   } else {
      //link only the part1
      if (strstr(argv[0],"part1.") != NULL){
        sprintf(comm, "ln -s '%s/%s' '%s/%s'", argv[1], argv[0], SHARE_LOC, argv[0]);
        printf("Link Created: '/mnt/Share/%s'\n", argv[0]);
        system(comm);
      } else if (strstr(argv[0], "part") == NULL){ //link linear files
        sprintf(comm, "ln -s '%s/%s' '%s/%s'", argv[1], argv[0], SHARE_LOC, argv[0]);
        printf("Link Created: '/mnt/Share/%s'\n", argv[0]);
        system(comm); 
      } 
      //sprintf(comm, "ln -s '%s/%s' '%s/%s'", argv[1], argv[0], SHARE_LOC, argv[0]);
      //printf("Link Created: '/mnt/Share/%s'\n", argv[0]);
      //printf("comm = %s\n", comm);
      //system(comm);
   }
   return 0;
}

void* create_link(){
   int tcount = 0;
   int rc;  

   char *err = 0;  

   String file_list, comm, query, comm_out;

   sqlite3 *db;
   
   rc = sqlite3_open(DBNAME, &db);
   if (rc != SQLITE_OK){
     fprintf(stderr, "File Presentation: Can't open database: %s.\n", sqlite3_errmsg(db));
     sqlite3_close(db);
     exit(1);
   }

   /*get current contents of share folder*/
   sprintf(comm, "ls %s", SHARE_LOC);

   strcpy(file_list, "");
   strcpy(comm_out, "");
   //printf("File Presentation:");   
   runCommand(comm, comm_out);

   char *ptr = strtok(comm_out, "\n");

   while (ptr != NULL){
      strcat(file_list,"\"");
      strcat(file_list,ptr);
      strcat(file_list,"\"");
      ptr = strtok(NULL, "\n");
      if (ptr != NULL){
        strcat(file_list,",");
      }
   }   

   //sprintf(file_list, "%s", comm_out);    
   //printf("File List: %s\n", file_list);

   sprintf(query, "SELECT filename, fileloc FROM VolContent WHERE filename NOT IN (%s);", file_list);
   
   //printf("Query = %s\n", query);
   

   rc = sqlite3_exec(db, query, callback, 0, &err);
   if (rc != SQLITE_OK){
      fprintf(stderr, "SQL Error: %s.\n", err);
      sqlite3_free(err);
   }
  
   sqlite3_close(db);
}

void create_link_cache(String filename){
   String ln;

   sprintf(ln, "ln -s '%s/part1.%s' '%s/part1.%s'", CACHE_LOC, filename, SHARE_LOC, filename);
   //printf("ln := %s\n", ln);
   system(ln);   
   printf("Created Link for file in Cache: %s\n" , filename);
}

void update_link_cache(String filename, String fileloc){
    String ln, rm;

    sprintf(rm, "rm '/mnt/Share/%s'", filename);
    printf("Rm = %s\n", rm);
    system(rm);
    sprintf(ln, "ln -s '%s/%s' '/mnt/Share/%s'", fileloc, filename, filename);
    printf("LN = %s\n", ln);
    system(ln);
}
