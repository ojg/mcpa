/*
 * cli.c
 *
 * Created: 09.11.2014 19:51:50
 *  Author: ole
 */ 

#include <stdio.h>
#include <stdbool.h>

char cmdbuf[10];

bool cli_task(void)
{
	fgets(cmdbuf, 10, stdin);
	printf("%s", cmdbuf);
	return true;
}