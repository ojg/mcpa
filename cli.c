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

#define MAX_CMD 2

char cmdbuf[128];

struct Command_t cmdlist[MAX_CMD];
uint8_t numcmd = 0;

bool cli_task(void)
{
    int i;

    // read line from uart
	fgets(cmdbuf, 127, stdin);

    // parse command
    i=0;
    while (i < numcmd && strncmp(cmdbuf, cmdlist[i].cmdstr, cmdlist[i].cmdstrlen)) { i++; };

    if (i == numcmd) { //cmd not found
        printf("Could not find command %s", cmdbuf);
        printf(CLI_PROMPT);
        return true;
    }

    cmdlist[i].cmdfunc();

    printf(CLI_PROMPT);
	return true;
}

void register_cli_command(char * cmdstr, void (*cmdfunc)(void), void (*cmdhelp)(void))
{
    cmdlist[numcmd].cmdstr = cmdstr;
    cmdlist[numcmd].cmdstrlen = strlen(cmdstr);
    cmdlist[numcmd].cmdhelp = cmdhelp;
    cmdlist[numcmd].cmdfunc = cmdfunc;
    numcmd++;
}
