/*
    CVFS Inital Configuration
    @author: Aidz and Jeff           :">
    requires:
        open-iscsi
        nmap
        sqlite3
        libsqlite3-dev
    uses:
        cmd_exec.c
        volman.c
*/

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

#include "../Global/global_definitions.h"
#include "../Utilities/cmd_exec.h"
#include "../Volume Management Module/make_volumes.h"
#include "initial_configurations.h"

#include <stdlib.h>

#define TARGET_FILE "target_list.txt"
#define GBSTR  "GiB"
#define MBSTR  "MB"    // assumption
#define TBSTR  "TB"

double toBytes(String sUnit) {
    char *ptr;

    ptr = strtok(sUnit, " ");
    double val = atof(ptr);
    ptr = strtok(NULL, " ");
    if (strcmp(ptr, GBSTR) == 0) {
        val *= 1000000000;
    } else if (strcmp(ptr, MBSTR) == 0) {
        val *= 1000000;
    } else if (strcmp(ptr, TBSTR) == 0) {
        val *= 1000000000000;
    }
    // printf("val = %f\n", val);
    return val;
}

void initialize() {

    system("clear");


    String ip = "", netmask = "", command = "", alltargetsStr = "", currtarget = "", iqn = "", sendtargets = "", sql = "";
    String disklist = "", assocvol = "", mountpt = "", avspace = "", command1 = "", sql1 = "";


    int counter = 1;
    //sqlite3 information
    sqlite3 *db;
    int rc = 0;

    //open sqlite3 db
    rc = sqlite3_open(DBNAME,&db);
    if (rc){
       printf("\nCannot open database.\n");
       exit(0);
    }

    // get network information of initiator
    // what if interface is not eth0? what if eth1...?
    runCommand("ip addr show eth0 | grep \'inet \' | awk \'{print $2}\' | cut -f1 -d\'/\'", ip);
    runCommand("ip addr show eth0 | grep \'inet \' | awk \'{print $2}\' | cut -f2 -d\'/\'", netmask);
    printf("*****  Network information  *****\n");
    ip[strlen(ip)] = '\0';
    netmask[strlen(netmask)] = '\0';
    printf("IP address: %s/%s\n\n", ip, netmask);


    // do nmap for active hosts with port 3260 open
    printf("Scanning network...\n");
    sprintf(command, "nmap -v -n %s%s%s -p 3260 | grep open | awk '/Discovered/ {print $NF}'", ip, "/", netmask);
    runCommand(command, alltargetsStr);

    // discover iscsi targets on hosts scanned successfully
    char *ptr;
    ptr = strtok(alltargetsStr, "\n");

    while(ptr != NULL) {
	printf("%s is a target.\n", ptr);
	sprintf(sendtargets,"iscsiadm -m discovery -t sendtargets -p %s | awk '{print $2}'",ptr);
	runCommand(sendtargets,iqn);
        printf("%s\n", iqn);

        sprintf(sql,"insert into Target(tid, ipadd,iqn) values (%d, '%s','%s');",counter, ptr,iqn);

        rc = sqlite3_exec(db,sql,0,0,0);
	if (rc != SQLITE_OK){
	   printf("\nDid not insert successfully!\n");
           exit(0);
        }
	strcpy(sendtargets,"");
        strcpy(sql,"");
        strcpy(iqn, "");
        ptr = strtok(NULL, "\n");
        counter++;
    }

    printf("\n\nLogging in to targets...");
    system("iscsiadm -m node --login");
    printf("\n\nAvailable partitions written to file \"%s\"\n", AV_DISKS);
    sleep(5);
    sprintf(command, "cat /proc/partitions > '%s'", AV_DISKS);
    system(command);
    system("cat /proc/partitions");

    makeVolume(0);

    runCommand("cat '../File Transaction Module/AvailableDisks.txt' | grep sd[b-z] | awk '{print $4}'",disklist);

    printf("DONE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    printf("Disklist before: %s\n\n", disklist);
    //strcat(disklist, "\n");
    char *ptr1;
    //int counter = 1;    // to aidz: di ako sure dito ah.. pano kung nag delete then init_conf sure ba na 1 lagi tapos sunod sunod?
    ptr1 = strtok(disklist,"\n");
    printf("PTR Before: %s\n\n", ptr1);
    printf("DIskList after: %s\n\n", disklist);
    counter = 1;
    while(ptr1 != NULL){
       printf("PTR: %s\n\n", ptr1);
       printf("INSIDE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
       strcat(assocvol,"/dev/vg");
       strcat(assocvol,ptr1);
       strcat(assocvol,"/lv");
       strcat(assocvol,ptr1);

 
       strcat(mountpt,"/mnt/lv");
       strcat(mountpt,ptr1);

       sprintf(command1,"lvdisplay %s | grep 'LV Size' | awk '{print $3,$4}'",assocvol);

       runCommand(command1,avspace);

       // edit here not sure if working (assume: avspace = "12.3 GiB")
       double space_bytes = toBytes(avspace);

       sprintf(sql1,"update Target set assocvol = '%s', mountpt = '%s', avspace = %lf where tid = %d", assocvol, mountpt, space_bytes, counter);
 
       printf("SQL1 = %s\n", sql1);
       rc = sqlite3_exec(db,sql1,0,0,0);

       if (rc != SQLITE_OK){
           printf("Did not insert successfully!");
           exit(0);
       }

       strcpy(assocvol,"");
       strcpy(mountpt,"");
       strcpy(avspace,"");
       counter++;
       ptr1 = strtok(NULL,"\n");
       printf("***************\n\n\n%s\n\n\n", ptr1);
    }

    printf("\n\nInitialization finished\n");
}