/*
 * cli.h
 *
 * Created: 09.11.2014 19:53:06
 *  Author: ole
 */ 


#ifndef CLI_H_
#define CLI_H_

#include <stdbool.h>
#include <inttypes.h>

#define CLI_PROMPT ">"

struct Command_t {
    char * cmdstr;
    uint8_t cmdstrlen;
    void (*cmdfunc)(void);
    void (*cmdhelp)(void);
};

bool cli_task(void);
void register_cli_command(char * cmdstr, void (*cmdfunc)(void), void (*cmdhelp)(void));

#endif /* CLI_H_ */