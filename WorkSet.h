#ifndef __WORKSET_H__
#define __WORKSET_H__

/* Working Set states */
#define WS_FREE             0x1 /* ws free */
#define WS_PENDING          0x2 /* ws not in any queue, ready for use */
#define WS_ACTIVE_IN            0x3 /* ws in incoming queue, ready for ipmi processing */
#define WS_ACTIVE_IN_PENDING        0x4 /* ws in use by the ipmi layer */
#define WS_ACTIVE_MASTER_WRITE      0x5 /* ws in outgoing queue */
#define WS_ACTIVE_MASTER_WRITE_PENDING  0x6 /* outgoing request in progress */
#define WS_ACTIVE_MASTER_WRITE_SUCCESS  0x7


#define WS_ARRAY_SIZE   64
#define WS_BUF_LEN      128

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct pkt_hdr {
	uint8_t   lun;
	uint32_t    req_data_len;
	uint32_t    resp_data_len;
	uint32_t    cmd_len;
	uint8_t       netfn;
	uint8_t       responder_lun;
	char        *ws;
} pkt_hdr_t;

typedef struct ipmi_cmd_req {
	uint8_t command;
	uint8_t data;
} ipmi_cmd_req_t;

typedef struct ipmi_cmd_resp {
	uint8_t completion_code;
	uint8_t data;
} ipmi_cmd_resp_t;

typedef struct ipmi_pkt {
	pkt_hdr_t       hdr;
	ipmi_cmd_req_t  *req;
	ipmi_cmd_resp_t *resp;
	void (*xport_completion_function)(void);
} ipmi_pkt_t;

typedef struct ws {
    uint32_t ws_state;
    ipmi_pkt_t pkt;
    uint8_t len_in;        /* lenght of incoming pkt */
    uint8_t len_out;       /* length of outgoing pkt */
    uint8_t len_rx;
    uint8_t rx_buf[WS_BUF_LEN];
    uint8_t pkt_in[WS_BUF_LEN];
    uint8_t pkt_out[WS_BUF_LEN];
    void (*ipmi_completion_function)( void );
} WS_t;


void WorkSetInit( void );
WS_t *WorkSetAlloc( void );
void WorkSetSetState( WS_t * ws, uint32_t state );
void WorkSetProcessWorkList( void );
void WorkSetFree( WS_t *ws );


#endif