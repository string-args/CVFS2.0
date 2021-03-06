#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sqlite3.h>
#include <syslog.h>

#include "../Global/global_definitions.h"
#include "../disk_pooling/file_presentation.h"
#include "../cache_access/cache_operation.h"
#include "file_mapping.h"

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
	int i;
	for (i = 0; i < argc; i++){
		//printf("%s = %s\n", azColName[i], argv[i]?argv[i]:"NULL");
	}
	printf("\n");
	return 0;
}

void update_target_size(sqlite3 *db, String filename, const char* fileloc){
    int rc;

    char *err = 0;
    const char *tail;

    String file;
    String sql;

    sqlite3_stmt *res;

    strcpy(file,fileloc);
    strcat(file,"/");
    strcat(file,filename);

    //get file size of mountpt
    sprintf(sql, "SELECT avspace FROM Target WHERE mountpt = '%s'", fileloc);
    rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
    if (rc != SQLITE_OK){
       fprintf(stderr, "Update(VolContent): SQL Error: %s\n", sqlite3_errmsg(db));
       sqlite3_close(db);
       exit(1);
    }

    if (sqlite3_step(res) == SQLITE_ROW){
       //do the operation here: you already retrieved the space of the mountpt
       double avspace = sqlite3_column_double(res,0);
       FILE *fp = fopen(file, "rb");
       fseek(fp, 0L, SEEK_END);
       double sz = ftell(fp); //get filesize of file
       rewind(fp);
       avspace = avspace - sz;
       sprintf(sql, "UPDATE Target SET avspace = %lf WHERE mountpt = '%s';", avspace, fileloc);
    }
     sqlite3_finalize(res);

    int good = 0;
    while (!good){
    	rc = sqlite3_exec(db, sql, callback, 0, &err);
    	if (rc != SQLITE_OK){
		//printf("Update Target: SQL Error: %s\n", sqlite3_errmsg(db));
	} else {good = 1;}
    }

	//syslog(LOG_INFO, "VolumeManagement: sql for update size: %s\n", sql);
    if (rc != SQLITE_OK){
       fprintf(stderr, "SQL Error: %s\n", err);
       sqlite3_free(err);
    }
    //sqlite3_finalize(res);
}


//this function insert a new entry to the volcontent table
void update_list(sqlite3 *db, String filename, const char* fileloc){
    int rc;

    char *err = 0;

    double sz;
    String file_to_open;
    sprintf(file_to_open, "%s/%s", fileloc, filename);
    //syslog(LOG_INFO, "VolumeManagement: FILE: %s |", file_to_open);
    FILE *fp = fopen(file_to_open, "rb");
    fseek(fp, 0L, SEEK_END);
    sz = ftell(fp);
    rewind(fp);
    fclose(fp);
    //syslog(LOG_INFO, "VolumeManagement: SIZE : %lf\n", sz);

    String sql;
    sprintf(sql, "INSERT INTO VolContent (filename, fileloc, filesize) VALUES ('%s','%s', %lf);", filename, fileloc, sz);

    /*Database is already open*/

   int good = 0;
    while (!good){
    	rc = sqlite3_exec(db, sql, callback, 0, &err);
    	if (rc != SQLITE_OK){
       		good = 0;
		//printf("Update List: SQL Error: %s\n", sqlite3_errmsg(db));
    	} else {
		good = 1;
	}
   }
    update_target_size(db, filename, fileloc);
}



//this function redirects the file to targets
void file_map(String fullpath, String filename, long sz){
	//printf("IN FILE MAP!\n");
	//printf("FULLPATH: %s\n", fullpath);

    int rc;

    String sql;
    String comm;

    sqlite3 *db;
    sqlite3_stmt *res;

    char *err = 0;
    const char *tail;

    sprintf(sql, "SELECT avspace, mountpt FROM TARGET WHERE avspace = (SELECT max(avspace) from Target) AND avspace >= %ld;", sz);

    rc = sqlite3_open(DBNAME, &db); /*Open database*/
    if (rc != SQLITE_OK){
       fprintf(stderr, "File Mapping: Can't open database: %s\n", sqlite3_errmsg(db));
       sqlite3_close(db);
       exit(1);
    }

    String pragma = "";
    strcpy(pragma, "PRAGMA journal_mode=WAL;");
    rc = sqlite3_exec(db, pragma, 0, 0, 0);

    //printf("IN FILE_MAP FUNCTION!\n");
    int good = 0;
     while(!good) {
	rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
	if (rc != SQLITE_OK) {
		//printf("db is locked\n");
		//printf("FILE MAP: SQL Error: %s\n", sqlite3_errmsg(db));
	} else {
		good = 1;
	}
      }

    //printf("DONE WITH LOOP:!\n");
    if(sqlite3_step(res) == SQLITE_ROW){

       //printf("IN SQLITE ROW!\n");
       syslog(LOG_INFO, "VolumeManagement: File %s is redirected to %s.\n", filename, sqlite3_column_text(res,1));
       sprintf(comm, "mv '%s' '%s/%s'", fullpath, sqlite3_column_text(res,1), filename);
       system(comm);
	//this part update the entry of the file to volcontent table
       //printf("Updating VolContent!\n");
       printf("[+] %s: %s\n", sqlite3_column_text(res,1), filename);
       update_list(db, filename, sqlite3_column_text(res,1));
   } else {
       // did not fetch anything, probably targets are full, cannot fit this file inside
       printf("Targets FULL. Cannot do this.\n");
       // delete file in TEMP_LOC
       String rm = "";
       sprintf(rm, "rm '%s'", fullpath);
       //printf("file_map: rm = %s\n", rm);
       system(rm);
       //printf("Nilinis ko na yung kinalat mong file.");
       //exit(1);
   }

    sqlite3_finalize(res);
    sqlite3_close(db);
    //printf("Done with File map!\n");
}

void file_map_stripe(String *fullpaths, String *filenames, int parts) {
    printf("in file_map_stripe function!\n");
    int rc;

    String sql;
    String comm;

    sqlite3 *db;
    sqlite3_stmt *res;

    char *err = 0;
    const char *tail;


    rc = sqlite3_open(DBNAME, &db); /*Open database*/
    if (rc != SQLITE_OK){
       fprintf(stderr, "File Mapping: Can't open database: %s\n", sqlite3_errmsg(db));
       sqlite3_close(db);
       exit(1);
    }
 
    String pragma = "";
    strcpy(pragma, "PRAGMA journal_mode=WAL;");
    rc = sqlite3_exec(db, pragma, 0, 0, 0);
 
    // check kung kasya
    sprintf(sql, "SELECT sum(avspace) FROM Target;");
    int good = 0;
     while(!good) {
    rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
    if (rc != SQLITE_OK) {
        //printf("FILE MAP: SQL Error: %s\n", sqlite3_errmsg(db));
    } else {
        good = 1;
    }
      }
    long totalspace = 0;
    if(sqlite3_step(res) == SQLITE_ROW) {
        totalspace = sqlite3_column_double(res,0);
    }
   printf("total space = %ld | parts = %d\n", totalspace, parts);
    if (totalspace / STRIPE_SIZE < parts) {
	
        printf("Targets FULL. Cannot do this.\n");
        exit(1);
    }


    sprintf(sql, "SELECT avspace, mountpt FROM Target WHERE avspace >= %ld ORDER BY avspace DESC;", STRIPE_SIZE);
    //printf("sql = %s!\n", sql);
    //printf("IN FILE_MAP_STRIPE FUNCTION!\n");
    good = 0;
     while(!good) {
    rc = sqlite3_prepare_v2(db, sql, 1000, &res, &tail);
    if (rc != SQLITE_OK) {
        //printf("FILE MAP: SQL Error: %s\n", sqlite3_errmsg(db));
    } else {
        good = 1;
    }
      }
    String mountpts[parts];
    double avspaces[parts];
    int num_targs = 0, zi = 0, k = 0;
    //printf("DONE WITH LOOP:!\n");
    while(sqlite3_step(res) == SQLITE_ROW){
        avspaces[num_targs] = sqlite3_column_double(res,0);
        strcpy(mountpts[num_targs], sqlite3_column_text(res,1));

	//printf("Target: %s | Avspace: %lf\n", mountpts[num_targs], avspaces[num_targs]);

	   num_targs++;
    }
    sqlite3_finalize(res);
    //printf("num targets = %d\n", num_targs);
    //printf("Num targs: %d\n", num_targs);
    //for checking
    //int pi = 0;
    //for (pi = 0; pi < parts; pi++){
    //	printf("[%d] Fullpath : %s | Filename : %s\n", pi, fullpaths[pi], filenames[pi]);
    //}

    // write each part to respective target
    //int target_num = 0;
    int act_cnt = -1;   // possible solution to weird round robin behavior
    for (zi = 0; zi < parts; zi++) {
        act_cnt = act_cnt + 1;
        if (num_targs == 0) {   // wala nang target pero meron pang part = HINDI NA KASYA :(
            //printf("stripe function cannot do this\n");
	    printf("Targets FULL. Cannot do this.\n");
            exit(1);
        }

        String filename = "";
        String fullpath = "";
        strcpy(filename, filenames[zi]);
        strcpy(fullpath, fullpaths[zi]);

        //printf("Filename : %s\n", filename);
	//printf("Fullpath : %s\n", fullpath);

        int target_num = act_cnt % num_targs;   // replace to act_cnt % zi if round robin fails to work
	//printf("Target Num : %d\n", target_num);

        long curr_avspace = avspaces[target_num];
	//printf("Curr Avspace = %lf\n", avspaces[target_num]);
        if (avspaces[target_num] < STRIPE_SIZE) {   // if hindi kasya sa target
            //printf("Hindi na kasya! Curr avspace : %ld | Stripe : %ld\n", curr_avspace, STRIPE_SIZE);
            num_targs = num_targs - (num_targs - target_num);
            act_cnt = -1;   // reset
            zi--;
            continue;
       }


        String curr_mount = "";
        strcpy(curr_mount, mountpts[target_num]);

        //printf("Curr mount: %s | Space: %ld\n", curr_mount, curr_avspace);

        syslog(LOG_INFO, "VolumeManagement: File %s is redirected to %s.\n", filename, curr_mount);
        sprintf(comm, "mv '%s' '%s/%s'", fullpath, curr_mount, filename);
        system(comm);

        // update our target copy, we assume all parts are of STRIPE_SIZE length only approximate
        avspaces[target_num] = avspaces[target_num] - STRIPE_SIZE;

        // this part update the entry of the file to volcontent table AND target size
        //printf("Updating VolContent!\n");
        //target_num++;
        printf("[+] %s: %s\n", curr_mount, filename);
        update_list(db, filename, curr_mount);
    }


    //sqlite3_finalize(res);
    sqlite3_close(db);

    //printf("Done with File map STRIPE!\n");
}

void file_map_cache(String filename, String event_name){
    String cp = "", source = "", dest = "";


    int flag = 1;
    FILE *fp = fopen ("../file_transaction/is_striping.txt", "wb");
    fprintf(fp, "%d", flag);
    fclose(fp); //not yet done striping;
    syslog(LOG_INFO, "VolumeManagement: Copying file to cache...\n");
    sprintf(cp, "cp '%s/%s' '%s/part1.%s'", TEMP_LOC, filename, CACHE_LOC, event_name);
    system(cp); //copy file to cache

    printf("[+] Cache: part1.%s\n", event_name);
  
}
