#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "rdma/rdma_cma.h"
#include "rdma/rdma_verbs.h"

#define RDMA_RECV_BUFF 1024
#define RDMA_MAX_HEAD 16
#define THREAD_NUMBER 4

struct cm_connection {
    struct rdma_cm_id   *id;
    struct ibv_mr       *recv_mr;
    struct ibv_mr       *send_mr;

    char                *recv_buff;
    char                *send_buff;

    char                head_buff[RDMA_MAX_HEAD];
};

static char *str_server = "127.0.0.1";
static char *str_port = "11211";

static struct cm_connection* build_connection();
static void disconnect(struct cm_connection *cm_conn);
static int send_msg(struct cm_connection *cm_conn, char *msg, size_t size);
/*static void recv_msg(struct cm_connection *cm_conn, char *msg);*/

/***************************************************************************//**
 * build connection
 *
 ******************************************************************************/
static struct cm_connection*
build_connection() {
    struct ibv_qp_init_attr attr;
	struct rdma_addrinfo    hints = { .ai_port_space = RDMA_PS_TCP },
                            *res = NULL;

    struct cm_connection    *cm_conn = NULL;


    cm_conn = calloc(1, sizeof(struct cm_connection));

    if (0 != rdma_getaddrinfo(str_server, str_port, &hints, &res)) {
        perror("rdma_getaddrinfo():");
        return NULL;
    }

    memset(&attr, 0, sizeof(attr));
	attr.cap.max_send_wr = attr.cap.max_recv_wr = 1;
	attr.cap.max_send_sge = attr.cap.max_recv_sge = 1;
	attr.cap.max_inline_data = 16;
	attr.sq_sig_all = 1;

    if (0 != rdma_create_ep(&cm_conn->id, res, NULL, &attr)) {
        perror("rdma_create_id():");
        return NULL;
    }

    if (0 != rdma_connect(cm_conn->id, NULL)) {
        perror("rdma_connect()");
        return NULL;
    }

    return cm_conn;
}

/***************************************************************************//**
 * disconnect 
 *
 ******************************************************************************/
static void 
disconnect(struct cm_connection *cm_conn) {
    rdma_disconnect(cm_conn->id);
    /* rdma_dereg_mr(cm_conn->recv_mr); */
    rdma_destroy_ep(cm_conn->id);
    free(cm_conn);
}

/***************************************************************************//**
 * send msg 
 *
 ******************************************************************************/
static int 
send_msg(struct cm_connection *cm_conn, char *msg, size_t size) {
    struct ibv_wc   wc;
    int             cqe = 0;

    if ( !(cm_conn->send_mr = rdma_reg_msgs(cm_conn->id, msg, size)) ) {
        perror("rdma_reg_msgs():");
        return -1;
    }

    if (0 != rdma_post_send(cm_conn->id, cm_conn, msg, size, cm_conn->send_mr, 0)) {
        perror("rdma_post_send()");
        rdma_dereg_mr(cm_conn->send_mr);
        return -1;
    }

    cqe = rdma_get_send_comp(cm_conn->id, &wc);
    if (cqe <= 0) {
        perror("rdma_get_send_comp()");
        rdma_dereg_mr(cm_conn->send_mr);
        return -1;
    }

    printf("send msgs OK!\n");
    rdma_dereg_mr(cm_conn->send_mr);
    return 0;
}

/***************************************************************************//**
 * thread function
 * 
 ******************************************************************************/
void *thread_run(void *arg) {
    struct cm_connection    *cm_conn = NULL;

    char    send_buff[100] = "hello world";
    int     i = 0;


    if ( !(cm_conn = build_connection()) ) {
        return NULL;
    }

    for (i = 0; i < 100; ++i) {
        if (0 != send_msg(cm_conn, send_buff, 100)) {
            printf("send_msg() error!\n");
            break;
        }
    }

    disconnect(cm_conn);
    return NULL;
}

/***************************************************************************//**
 * main
 *
 ******************************************************************************/
int 
main() {
    pthread_t threads[THREAD_NUMBER];
    int     i = 0;

    memset(threads, 0, sizeof(threads));

    for (i = 0; i < THREAD_NUMBER; ++i) {
        if (0 != pthread_create(&threads[i], NULL, thread_run, NULL)) {
            return -1;
        }
    }

    for (i = 0; i < THREAD_NUMBER; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

