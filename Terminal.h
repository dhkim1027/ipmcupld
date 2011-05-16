#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#define CR     0x0D
#define LF     0x0A

void TerminalProcessPkt( WS_t *ws );
void TerminalSendPkt( WS_t *ws );

#endif