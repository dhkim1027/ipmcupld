#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "IPMI.h"
#include "WorkSet.h"
#include "Terminal.h"

WS_t   ws_array[WS_ARRAY_SIZE];

void
WorkSetInit( void )
{
    uint32_t i;

    for ( i = 0; i < WS_ARRAY_SIZE; i++ )
    {
        ws_array[i].ws_state    = WS_FREE;
        ws_array[i].len_in      = 0;
        ws_array[i].len_out     = 0;
        ws_array[i].len_rx      = 0;
        ws_array[i].ipmi_completion_function = 0;
        memset(ws_array[i].rx_buf, 0, WS_BUF_LEN);
        memset(ws_array[i].pkt_in, 0, WS_BUF_LEN);
        memset(ws_array[i].pkt_out, 0, WS_BUF_LEN);
    }
}

WS_t *
WorkSetAlloc( void )
{
    WS_t *ws = 0;
    WS_t *ptr = ws_array;
    uint32_t i;

    for ( i = 0; i < WS_ARRAY_SIZE; i++ )
    {
        ptr = &ws_array[i];
        if( ptr->ws_state == WS_FREE ) {
            ptr->ws_state = WS_PENDING;
            ws = ptr;
            break;
        }
    }

    return ws;
}

void
WorkSetFree( WS_t *ws )
{
    uint32_t len, i;
    char *ptr = (char *)ws;

    len = sizeof( WS_t );
    for( i = 0 ; i < len ; i++ ) {
        *ptr++ = 0;
    }
    ws->ws_state = WS_FREE;
}

WS_t *
WorkSetGetElem( uint32_t state )
{
    WS_t *ws = 0;
    WS_t *ptr = ws_array;
    uint32_t i;

    for ( i = 0; i < WS_ARRAY_SIZE; i++ )
    {
        ptr = &ws_array[i];
        if( ptr->ws_state == state ) {
            ws = ptr;
        }
    }

    return ws;
}

void
WorkSetSetState( WS_t * ws, uint32_t state )
{
    ws->ws_state = state;
}

void
WorkSetProcessWorkList( void )
{
    WS_t *ws;

    ws = WorkSetGetElem( WS_ACTIVE_IN );
    if( ws ) {
        WorkSetSetState( ws, WS_ACTIVE_IN_PENDING );
//		printf("%s\n", ws->rx_buf);
        TerminalProcessPkt( ws );
    }

    ws = WorkSetGetElem( WS_ACTIVE_MASTER_WRITE );
    if( ws ) {
        WorkSetSetState( ws, WS_ACTIVE_MASTER_WRITE_PENDING );
        TerminalSendPkt( ws );
    }
}

