/*
 * cli.c
 *
 * Created: 09.11.2014 19:51:50
 *  Author: ole
 */ 

#include "cli.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAX_CMD 16

char cmdbuf[128];

struct Command_t cmdlist[MAX_CMD];
uint8_t numcmd = 0;

bool cli_task(void)
{
    // read line from uart
    memset(cmdbuf, 0, 128 * sizeof(char));
	fgets(cmdbuf, 127, stdin);

    // parse command
    int i=0;
    while (i < numcmd && strncmp(cmdbuf, cmdlist[i].cmdstr, cmdlist[i].cmdstrlen)) { i++; };

    if (i == numcmd) { //cmd not found
        printf("Could not find command %s", cmdbuf);
        printf(CLI_PROMPT);
        return true;
    }

    cmdlist[i].cmdfunc(&cmdbuf[cmdlist[i].cmdstrlen+1]); // pass rest of string to command function

    printf(CLI_PROMPT);
	return true;
}

void register_cli_command(char * cmdstr, void (*cmdfunc)(char*), void (*cmdhelp)(void))
{
    cmdlist[numcmd].cmdstr = cmdstr;
    cmdlist[numcmd].cmdstrlen = strlen(cmdstr);
    cmdlist[numcmd].cmdhelp = cmdhelp;
    cmdlist[numcmd].cmdfunc = cmdfunc;
    numcmd++;
}

void cmd_help(char * stropt)
{
    if (*stropt == '\n' || *stropt == 0) {
        printf("Available commands:\n");
        for (int i=0; i<numcmd; i++) {
            printf("%s\n", cmdlist[i].cmdstr);
        }
        printf("Type help <command> to get help for that command.\n");
        return;
    }

    // parse stropt
    int i=0;
    while (i < numcmd && !strstr(stropt, cmdlist[i].cmdstr)) { i++; };

    if (i == numcmd) { //cmd not found
        printf("Could not find help for %s", stropt);
        return;
    }

    cmdlist[i].cmdhelp(); // run help function for this command
}
