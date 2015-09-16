#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "protocol_binary.h"

#define BUFF_SIZE 1024
#define RDMA_MAX_HEAD 16
#define POLL_WC_SIZE 128
#define REG_PER_CONN 128

#define ASCII_MIN_REQUEST
#define BIN_MIN_REQUEST
#define MEMCACHED_MAX_REQUEST

/***************************************************************************//**
 * Testing parameters
 *
 ******************************************************************************/
static bool 	bin_protocol = false;
static char     *pstr_server = "127.0.0.1";
static char     *pstr_port = "11211";
static int      thread_number = 1;
static int      request_number = 10000;
static int 	reuqest_size = 100;
static int      last_time = 1000;    /* secs */
static int      verbose = 0;
static int      cq_size = 1024;
static int      wr_size = 1024;
static int      max_sge = 8;

/***************************************************************************//**
 * Testing message
 *
 ******************************************************************************/

static char 	*add_ascii_noreply = "add foo 0 0 1 noreply\r\n1\r\n";
static char 	*set_ascii_noreply = "set foo 0 0 1 noreply\r\n1\r\n";
static char 	*replace_ascii_noreply = "replace foo 0 0 1 noreply\r\n1\r\n";
static char 	*append_ascii_noreply = "append foo 0 0 1 noreply\r\n1\r\n";
static char 	*prepend_ascii_noreply = "prepend foo 0 0 1 noreply\r\n1\r\n";
static char 	*incr_ascii_noreply = "incr foo 1 noreply\r\n";
static char 	*decr_ascii_noreply = "decr foo 1 noreply\r\n";
static char 	*delete_ascii_noreply = "delete foo noreply\r\n";

static char 	*add_ascii_reply = "add foo 0 0 1\r\n1\r\n";
static char 	*set_ascii_reply = "set foo 0 0 1\r\n1\r\n";
static char 	*replace_ascii_reply = "replace foo 0 0 1\r\n1\r\n";
static char 	*append_ascii_reply = "append foo 0 0 1\r\n1\r\n";
static char 	*prepend_ascii_reply = "prepend foo 0 0 1\r\n1\r\n";
static char 	*incr_ascii_reply = "incr foo 1\r\n";
static char 	*decr_ascii_reply = "decr foo 1\r\n";
static char 	*delete_ascii_reply = "delete foo\r\n";

/******************************************************************************
 * Bin request
 * ****************************************************************************/
static void 	*add_bin;
static void 	*set_bin;
static void 	*replace_bin;
static void 	*append_bin;
static void 	*prepend_bin;
static void 	*incr_bin;
static void 	*decr_bin;
static void 	*delete_bin;

/***************************************************************************//**
 * Relative resources around connection
 *
 ******************************************************************************/
struct rdma_context {
    struct ibv_context          **device_ctx_list;
    struct ibv_context          *device_ctx;
    struct ibv_comp_channel     *comp_channel;
    struct ibv_pd               *pd;
    struct ibv_cq               *send_cq;
    struct ibv_cq               *recv_cq;

    struct rdma_event_channel   *cm_channel;
    
    struct rdma_cm_id           *listen_id;

} rdma_ctx;

struct wr_context;

struct rdma_conn {
    struct rdma_cm_id   *id;

    struct ibv_pd       *pd;
    struct ibv_cq       *send_cq;
    struct ibv_cq       *recv_cq;

    char                *rbuf;
    struct ibv_mr       *rmr;

    struct ibv_mr           **rmr_list;
    char                    **rbuf_list;
    struct wr_context       *wr_ctx_list;
    size_t                  rsize; 
    size_t                  buff_list_size;
};

struct wr_context {
    struct rdma_conn       *c;
    struct ibv_mr           *mr;
};

void init_message(void)
{
    if (bin_protocol == true)
	init_binary_message();
    else
	init_ascii_message();
}

void write_to_buff(void **buff, void *data, int size)
{
    memcpy(*buff, data, size);
    *buff += size;
}


void build_ascii_cmd(char *cmd_cache, char *cmd_name, int cmd_length, bool if_extra, bool if_delta, bool if_reply)
{
    int keylen, bodyleni, i;
	
    cmd_cache = malloc(request_size);

    if (if_extra == true) // add set replace append prepend
	keylen = request_size - cmd_length - 12; // useless charactor
    else if (if_delta == true) // incr decr
	keylen = request_size - cmd_length - 4; // useless charactor
    else // delete
	keylen = request_size - cmd_length - 3; // useless cahractor

    if (keylen > 250) {
	bodylen = keylen - 250;
	keylen = 250;
    } else {
	bodylen = 1;
	keylen -= 1;
    }

    if (if_reply == false)
	keylen -= 8;
	
    write_to_buff(&cmd_cache, cmd_name, cmd_length);

    write_to_buff(&cmd_cache, " ", 1);
    for (i = 0; i < keylen; i++)
	write_to_buff(&cmd_cache, "1", 1);
	
    if (if_extra == true) // add set replace append prepend
	write_to_buff(&cmd_cache, " 0 0 1", 6);

    if (if_delta == true) // incr decr
	write_to_buff(&cmd_cache, " ", 1);
	for (i = 0; i < bodylen; i++)
	    write_to_buff(&cmd_cache, "1", 1);
	    
    if (if_reply == false)
	write_to_buff(%cmd_cache, " noreply", 8);
	
    write_to_buff(&cmd_cache, "\r\n", 2);
    
    if (if_extra == true) { // add set replace append prepend
	for (i = 1; i < keylen, i++)
	    write_to_buff(&cmd_cache, "1", 1);
	write_to_buff(%cmd_cache, "\r\n", 2);
    }

    return;
}


void init_ascii_message(void)
{
    build_ascii_cmd( 	add_ascii_noreply, 	"add", 		3, 	true, 	false, 	false);
    build_ascii_cmd( 	set_ascii_noreply, 	"set", 		3, 	true, 	false, 	false);
    build_ascii_cmd( 	replace_ascii_noreply, 	"replace", 	7, 	true, 	false, 	false);
    build_ascii_cmd( 	append_ascii_noreply, 	"append", 	6, 	true, 	false, 	false);
    build_ascii_cmd( 	prepend_ascii_noreply, 	"prepend", 	7, 	true, 	false, 	false);
    build_ascii_cmd( 	incr_ascii_noreply, 	"incr", 	4, 	false, 	true, 	false);
    build_ascii_cmd( 	decr_ascii_noreply, 	"decr", 	4, 	false, 	true, 	false);
    build_ascii_cmd( 	delete_ascii_noreply, 	"delete", 	6, 	false, 	false, 	false);


    build_ascii_cmd( 	add_ascii_reply, 	"add", 		3, 	true, 	false, 	true);
    build_ascii_cmd( 	set_ascii_reply, 	"set", 		3, 	true, 	false, 	true);
    build_ascii_cmd( 	replace_ascii_reply, 	"replace", 	7, 	true, 	false, 	true);
    build_ascii_cmd( 	append_ascii_reply, 	"append", 	6, 	true, 	false, 	true);
    build_ascii_cmd( 	prepend_ascii_reply, 	"prepend", 	7, 	true, 	false, 	true);
    build_ascii_cmd( 	incr_ascii_reply, 	"incr", 	4, 	false, 	true, 	true);
    build_ascii_cmd( 	decr_ascii_reply, 	"decr", 	4, 	false, 	true, 	true);
    build_ascii_cmd( 	delete_ascii_reply, 	"delete", 	6, 	false, 	false, 	true);
}

void build_bin_cmd(void *cmd_cache, protocol_binary_command cmd)
{
    int keylen, bodylen;
    protocol_binary_request_header *tmp_hd;
    char *body_ptr; // point to the position after the header

    cmd_cache = malloc(request_size);
    tmp_hd = (protocol_binary_request_header *)cmd_cache;

    switch (cmd) {
	case PROTOCOL_BINARY_CMD_ADD:
	case PROTOCOL_BINARY_CMD_SET:
	case PROTOCOL_BINARY_CMD_REPLACE:
	    keylen = request_size - 32; // for the reason of memory align, do not use sizeof(protocol_binary_request_header)!!!!!!
	    body_ptr = (char *)cmd_cache + 32;
	    tmp_hd.request.extlen = 8;
	    tmp_hd.body.flags = 0;
	    tmp_hd.body.expiration = 0;
	    break;
	case PROTOCOL_BINARY_CMD_APPEND:
	case PROTOCOL_BINARY_CMD_PREPEND:
	case PROTOCOL_BINARY_CMD_DELETE:
	    keylen = request_size - 24; // see above
	    body_ptr = (char *)cmd_cache + 24;
	    tmp_hd.request.extlen = 0;
	    break;
	case PROTOCOL_BINARY_CMD_INCR:
	case PROTOCOL_BINARY_CMD_DECR:
	    keylen = request_size - 44; // see above
	    body_ptr = (char *)cmd_cache + 44;
	    tmp_hd.request.extlen = 20;
	    tmp_hd.body.delta = 1;
	    tmp_hd.body.initial = 0;
	    tmp_hd.body.expiration = 0;
	    break;
    }

    if (keylen > 250) {
	bodylen = keylen - 250;
    else
	bodylen = 1;
    keylen -= bodylen;

    tmp_hd.request.magic = PROTOCOL_BINARY_REQ;
    tmp_hd.request.opcode = cmd;
    tmp_hd.request.keylen = htons(keylen);
    tmp_hd.requset.datatype = PROTOCOL_BINARY_RAW_BYTES;
    tmp_hd.request.bodylen = htonl(bodylen);
    tmp_hd.request.reserved = tmp_hd.request.opaque = tmp_hd.request.cas = 0;

    for (i = 0 ; i < keylen + bodylen; i++)
	write_to_buff(&body_ptr, "1", 1);

    return;
}

void init_binary_message(void)
{
    build_bin_cmd( 	add_bin, 	PROTOCOL_BINARY_CMD_ADD);
    build_bin_cmd( 	set_bin, 	PROTOCOL_BINARY_CMD_SET);
    build_bin_cmd( 	replace_bin, 	PROTOCOL_BINARY_CMD_REPLACE);
    build_bin_cmd( 	append_bin, 	PROTOCOL_BINARY_CMD_APPEND);
    build_bin_cmd( 	prepend_bin, 	PROTOCOL_BINARY_CMD_PREPEND);
    build_bin_cmd( 	incr_bin, 	PROTOCOL_BINARY_CMD_INCREMENT);
    build_bin_cmd( 	decr_bin, 	PROTOCOL_BINARY_CMD_DECREMENT);
    build_bin_cmd( 	delete_bin, 	PROTOCOL_BINARY_CMD_DELETE);

    return;
}

void init_binary_message(void)
{
    protocol_binary_request_add *add_bin_p;
    add_bin = malloc(sizeof(protocol_binary_request_add) + 2);
    add_bin_p = (protocol_binary_request_add *)add_bin;
    add_bin_p->message.header.request.magic = PROTOCOL_BINARY_REQ;
    add_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_ADD;   
    add_bin_p->message.header.request.keylen = htons(1);
    add_bin_p->message.header.request.extlen = 8;
    add_bin_p->message.header.request.datatype = PROTOCOL_BINARY_RAW_BYTES;
    add_bin_p->message.header.request.bodylen = htonl(10);
    add_bin_p->message.header.request.reserved = add_bin_p->message.header.request.opaque = add_bin_p->message.header.request.cas = 0;
    add_bin_p->message.body.flags = 0;
    add_bin_p->message.body.expiration = 0;
    *((char *)add_bin + sizeof(protocol_binary_request_add)) = '1';
    *((char *)add_bin + sizeof(protocol_binary_request_add) + 1) = '1';


    protocol_binary_request_set *set_bin_p;
    set_bin = malloc(sizeof(protocol_binary_request_set) + 2);
    set_bin_p = (protocol_binary_request_set *)set_bin;
    memcpy(set_bin, add_bin, sizeof(protocol_binary_request_set) + 2);
    set_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_SET;


    protocol_binary_request_replace *replace_bin_p;
    replace_bin = malloc(sizeof(protocol_binary_request_replace) + 2);
    replace_bin_p = (protocol_binary_request_replace *)replace_bin;
    memcpy(replace_bin, add_bin, sizeof(protocol_binary_request_replace) + 2);
    replace_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_REPLACE;

    
    protocol_binary_request_append *append_bin_p;
    append_bin = malloc(sizeof(protocol_binary_request_append) + 2);
    append_bin_p = (protocol_binary_request_append *)append_bin;
    memcpy(append_bin, add_bin, sizeof(protocol_binary_request_header));
    append_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_APPEND;
    append_bin_p->message.header.request.extlen = 0;
    append_bin_p->message.header.request.bodylen = htonl(2);
    *((char *)append_bin + sizeof(protocol_binary_request_append)) = '1';
    *((char *)append_bin + sizeof(protocol_binary_request_append) + 1) = '1';


    protocol_binary_request_prepend *prepend_bin_p;
    prepend_bin = malloc(sizeof(protocol_binary_request_prepend) + 2);
    prepend_bin_p = (protocol_binary_request_prepend *)prepend_bin;
    memcpy(prepend_bin, append_bin, sizeof(protocol_binary_request_append) + 2);
    prepend_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_PREPEND;


    protocol_binary_request_incr *incr_bin_p;
    incr_bin = malloc(sizeof(protocol_binary_request_incr) + 1);
    incr_bin_p = (protocol_binary_request_incr *)incr_bin;
    memcpy(incr_bin, add_bin, sizeof(protocol_binary_request_header));
    incr_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_INCREMENT;
    incr_bin_p->message.header.request.extlen = 20;
    incr_bin_p->message.header.request.bodylen = htonl(21);
    incr_bin_p->message.body.delta = 1;
    incr_bin_p->message.body.initial = 0;
    incr_bin_p->message.body.expiration = 0;
    *((char *)incr_bin + sizeof(protocol_binary_request_incr)) = '1';


    protocol_binary_request_decr *decr_bin_p;
    decr_bin = malloc(sizeof(protocol_binary_request_decr) + 1);
    decr_bin_p = (protocol_binary_request_decr *)decr_bin;
    memcpy(decr_bin, incr_bin, sizeof(protocol_binary_request_incr) + 1);
    decr_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_DECREMENT;


    protocol_binary_request_delete *delete_bin_p;
    delete_bin = malloc(sizeof(protocol_binary_request_delete) + 1);
    delete_bin_p = (protocol_binary_request_delete *)delete_bin;
    memcpy(delete_bin, append_bin, sizeof(protocol_binary_request_delete) + 1);
    delete_bin_p->message.header.request.opcode = PROTOCOL_BINARY_CMD_DELETE;
}

/***************************************************************************//**
 * Description 
 * Init rdma global resources
 *
 ******************************************************************************/
int
init_rdma_global_resources() {
    memset(&rdma_ctx, 0, sizeof(struct rdma_context));

    int num_device;
    if ( !(rdma_ctx.device_ctx_list = rdma_get_devices(&num_device)) ) {
        perror("rdma_get_devices()");
        return -1;
    }
    rdma_ctx.device_ctx = *rdma_ctx.device_ctx_list;
    printf("Get device: %d\n", num_device); 

    /*
    if ( !(rdma_ctx.comp_channel = ibv_create_comp_channel(rdma_ctx.device_ctx)) ) {
        perror("ibv_create_comp_channel");
        return -1;
    }
    */

    if ( !(rdma_ctx.pd = ibv_alloc_pd(rdma_ctx.device_ctx)) ) {
        perror("ibv_alloc_pd");
        return -1;
    }

    if ( !(rdma_ctx.send_cq = ibv_create_cq(rdma_ctx.device_ctx, 
                    cq_size, NULL, NULL, 0)) ) {
        perror("ibv_create_cq");
        return -1;
    }

    if ( !(rdma_ctx.recv_cq = ibv_create_cq(rdma_ctx.device_ctx, 
                    cq_size, NULL, NULL, 0)) ) {
        perror("ibv_create_cq");
        return -1;
    }

     return 0;
}

int init_socket_resources(void)
{
    int socket;
    struct sockaddr_in addr;

    socket = socket(AF_INET, SOCK_STREAM, 0);
    if (socket < 0)
    {
	printf("Alloc socket fail!\n");
	return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);//--------------------------------
    if (bind(socket, (struct sockaddr *)&addr, sizeof(addr)))
    {
	printf("Bind fail!\n");
	return 0;
    }




}

/***************************************************************************//**
 * Connection server
 *
 ******************************************************************************/
static struct rdma_conn *
build_connection() {
    struct rdma_conn        *c = calloc(1, sizeof(struct rdma_conn));
    if (0 != rdma_create_id(NULL, &c->id, c, RDMA_PS_TCP)) {
        perror("rdma_create_id()");
        return NULL;
    }
    struct rdma_addrinfo    hints = { .ai_port_space = RDMA_PS_TCP },
                            *res = NULL;
    if (0 != rdma_getaddrinfo(pstr_server, pstr_port, &hints, &res)) {
        perror("rdma_getaddrinfo()");
        return NULL;
    }

    int ret = 0;
    ret = rdma_resolve_addr(c->id, NULL, res->ai_dst_addr, 100);  // wait for 100 ms
    ret = rdma_resolve_route(c->id, 100); 

    rdma_freeaddrinfo(res);
    if (0 != ret) {
        perror("Error on resolving addr or route");
        return NULL;
    }


    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof(struct ibv_qp_init_attr));
    qp_attr.cap.max_send_wr = 8;
    qp_attr.cap.max_recv_wr = wr_size;
    qp_attr.cap.max_send_sge = max_sge;
    qp_attr.cap.max_recv_sge = max_sge;
    qp_attr.sq_sig_all = 1;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.send_cq = rdma_ctx.send_cq;
    qp_attr.recv_cq = rdma_ctx.recv_cq;

    if (0 != rdma_create_qp(c->id, rdma_ctx.pd, &qp_attr)) {
        perror("rdma_create_qp()");
        return NULL;;
    }

    if (0 != rdma_connect(c->id, NULL)) {
        perror("rdma_connect()");
        return NULL;
    }

    c->buff_list_size = REG_PER_CONN;
    c->rsize = BUFF_SIZE;
    c->rbuf_list = calloc(c->buff_list_size, sizeof(char*));
    c->rmr_list = calloc(c->buff_list_size, sizeof(struct ibv_mr*));
    c->wr_ctx_list = calloc(c->buff_list_size, sizeof(struct wr_context));

    int i = 0;
    for (i = 0; i < c->buff_list_size; ++i) {
        c->rbuf_list[i] = malloc(c->rsize);
        c->rmr_list[i] = rdma_reg_msgs(c->id, c->rbuf_list[i], c->rsize);
        c->wr_ctx_list[i].c = c;
        c->wr_ctx_list[i].mr = c->rmr_list[i];
        if (0 != rdma_post_recv(c->id, &c->wr_ctx_list[i], c->rmr_list[i]->addr, c->rmr_list[i]->length, c->rmr_list[i])) {
            perror("rdma_post_recv()");
            return NULL;
        }
    }

    return c;
}

/***************************************************************************//**
 * Test command with registered memory
 *
 ******************************************************************************/
void *
test_with_regmem(void *arg) {
    struct rdma_conn *c = NULL;
    struct timespec start,
                    finish;
    int i = 0;

    clock_gettime(CLOCK_REALTIME, &start);
    if ( !(c = build_connection()) ) {
        return NULL;
    }

    if (bin_protocol == false) {
	printf("ascii noreply:\n");
	
	for (i = 0; i < request_number; ++i) {
	    send(socket, add_ascii_noreply, 	request_size);
	    send(socket, set_ascii_noreply, 	request_size);
	    send(socket, replace_ascii_noreply, request_size);
	    send(socket, append_ascii_noreply, 	request_size);
	    send(socket, prepend_ascii_noreply, request_size);
	    send(socket, incr_ascii_noreply, 	request_size);
	    send(socket, decr_ascii_noreply, 	request_size);
	    send(socket, delete_ascii_noreply, 	request_size);
	}
	
	clock_gettime(CLOCK_REALTIME, &finish);
	printf("Cost time: %lf secs\n", (double)(finish.tv_sec-start.tv_sec + 
                (double)(finish.tv_nsec - start.tv_nsec)/1000000000 ));
	
	printf("\n ascii reply:\n");
	clock_gettime(CLOCK_REALTIME, &start);
	
	for (i = 0; i < request_number; ++i) {
	    send(socket, add_ascii_reply, 	request_size);
	    recv(socket, );
	    send(socket, set_ascii_reply, 	request_size);
	    recv;
	    send(socket, replace_ascii_reply, 	request_size);
	    recv;
	    send(socket, append_ascii_reply, 	request_size);
	    recv;
	    send(socket, prepend_ascii_reply, 	request_size);
	    recv;
	    send(socket, incr_ascii_reply, 	request_size);
	    recv;
	    send(socket, decr_ascii_reply, 	request_size);
	    recv;
	    send(socket, delete_ascii_reply, 	request_size);
	}
	
	clock_gettime(CLOCK_REALTIME, &finish);
	printf("Cost time: %lf secs\n", (double)(finish.tv_sec-start.tv_sec + 
                (double)(finish.tv_nsec - start.tv_nsec)/1000000000 ));

    } else {

	printf("\n bin reply:\n");
	clock_gettime(CLOCK_REALTIME, &start);

	for (i = 0; i < request_number; ++i){
	    send(socket, add_bin_reply, 	request_size);
	    send(socket, set_bin_reply, 	request_size);
	    send(socket, replace_bin_reply, 	request_size);
	    send(socket, append_bin_reply, 	request_size);
	    send(socket, prepend_bin_reply, 	request_size);
	    send(socket, incr_bin_reply, 	request_size);
	    send(socket, decr_bin_reply, 	request_size);
	    send(socket, delete_bin_reply, 	request_size);
	}

	clock_gettime(CLOCK_REALTIME, &finish);
	printf("Cost time: %lf secs\n", (double)(finish.tv_sec-start.tv_sec +
		(double)(finish.tv_nsec - start.tv_nsec)/1000000000 ));
    }

    return NULL;
}

/***************************************************************************//**
 * main
 *
 ******************************************************************************/
int 
main(int argc, char *argv[]) {
    char        c = '\0';
    while (-1 != (c = getopt(argc, argv,
            "c:"    /* thread number */
            "r:"    /* request number per thread */
            "t:"    /* last time, secs */
            "p:"    /* listening port */
            "s:"    /* server ip */
            "R"     /* whether receive message from server */
            "v"     /* verbose */
    ))) {
        switch (c) {
            case 'c':
                thread_number = atoi(optarg);
                break;
            case 'r':
                request_number = atoi(optarg);
                break;
            case 't':
                last_time = atoi(optarg);
                break;
            case 'p':
                pstr_port = optarg;
                break;
            case 's':
                pstr_server = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
	    case 'm':
	    	request_size = atoi(optarg);
	    case 'b':
	    	bin_protocol = true;
            default:
                assert(0);
        }
    }

    if (bin_protocol == true)
    {
	if (request_size < BIN_MIX_REQUEST)
	{
	    printf("request_size is smaller than BIN_ASCII_REQUEST.\n");
	    return 0;
	}
    } else {
	if (request_size < ASCII_MIX_REQUEST)
	{
	    printf("request_size is smaller than ASCII_MIX_REQUEST.\n");
	    return 0;
	}
    }
    
    if (request_size > MEMCACHED_MAX_REQUEST)
    {
	printf("request_size is larger than MEMCACHED_MAX_REQUEST.\n");
	return 0;
    }

    init_socket_resources();
    init_message();

    struct timespec start,
                    finish;
    clock_gettime(CLOCK_REALTIME, &start);

    test_with_regmem(NULL);

    clock_gettime(CLOCK_REALTIME, &finish);

    printf("Total cost time: %lf secs\n", (double)(finish.tv_sec-start.tv_sec + 
                (double)(finish.tv_nsec - start.tv_nsec)/1000000000 ));
    return 0;
}

