/***************************************************************************//**
 * Author:  Dyinnz
 * Date  :  2015-08-08 * Email :  ml_143@sina.com
 * Description: 
 *   a simple benchmark for memcached using RDMA
 ******************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#define RDMA_RECV_BUFF 1024
#define RDMA_MAX_HEAD 16

/***************************************************************************//**
 * Testing parameters
 *
 ******************************************************************************/
static char     *str_server = "127.0.0.1";
static char     *str_port = "11211";
static int      thread_number = 1;
static int      request_number = 10000;
static int      last_time = 1000;    /* secs */
static int      is_recv = 0;

/***************************************************************************//**
 * Relative resources around connection
 *
 ******************************************************************************/
struct cm_connection {
    struct rdma_cm_id   *id;

    struct ibv_mr       *recv_mr;
    struct ibv_mr       *send_mr;

    char                recv_buff[RDMA_RECV_BUFF];
    char                head_buff[RDMA_MAX_HEAD];
};

/***************************************************************************//**
 * Function prototypes
 *
 ******************************************************************************/
static struct cm_connection* build_connection();
static void disconnect(struct cm_connection *cm_conn);
static int send_msg(struct cm_connection *cm_conn, char *msg, size_t size);
static int recv_msg(struct cm_connection *cm_conn);

/***************************************************************************//**
 * Build RDMA connection, return a pointer to struct cm_connection
 *
 ******************************************************************************/
static struct cm_connection*
build_connection() {
    struct ibv_qp_init_attr attr;
    struct rdma_addrinfo    hints = { .ai_port_space = RDMA_PS_TCP },
                            *res = NULL;
    struct cm_connection    *cm_conn = calloc(1, sizeof(struct cm_connection));

    int     ret = 0;

    if (0 != rdma_getaddrinfo(str_server, str_port, &hints, &res)) {
        perror("rdma_getaddrinfo():");
        return NULL;
    }

    memset(&attr, 0, sizeof(attr));
    attr.cap.max_send_wr = attr.cap.max_recv_wr = 8;
    attr.cap.max_send_sge = attr.cap.max_recv_sge = 8;
    attr.cap.max_inline_data = 16;
    attr.sq_sig_all = 1;

    ret = rdma_create_ep(&cm_conn->id, res, NULL, &attr);
    rdma_freeaddrinfo(res);
    if (0 != ret) {
        perror("rdma_create_ep():");
        return NULL;
    }

    if (0 != rdma_connect(cm_conn->id, NULL)) {
        rdma_destroy_ep(cm_conn->id);
        perror("rdma_connect()");
        return NULL;
    }

    if ( !(cm_conn->recv_mr = rdma_reg_msgs(cm_conn->id, cm_conn->recv_buff, RDMA_RECV_BUFF)) ) {
        rdma_destroy_ep(cm_conn->id);
        perror("rdma_reg_msgs():");
        return NULL;
    }

    return cm_conn;
}

/***************************************************************************//**
 * Disconnect the RDMA connection, and release relative resources
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
 * Send message by RDMA send operation
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

    /* printf("send msgs OK!\n"); */
    rdma_dereg_mr(cm_conn->send_mr);
    return 0;
}

/*
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

    while (true) {
        cqe = ibv_poll_cq(cm_conn->id->send_cq, q, &wc);
        if (IBV_WC_SUCCESS == wc.status && wc.opcode & IBV_WC_SEND) {
            break;
        }
    }

    if (cqe <= 0) {
        perror("rdma_get_send_comp()");
        rdma_dereg_mr(cm_conn->send_mr);
        return -1;
    }

    // printf("send msgs OK!\n");
    rdma_dereg_mr(cm_conn->send_mr);
    return 0;
}
*/

/***************************************************************************//**
 * Receive message bt RDMA recv operation
 *
 ******************************************************************************/
static int
recv_msg(struct cm_connection *cm_conn) {
    struct ibv_wc   wc;
    int             cqe = 0;

    if (0 != rdma_post_recv(cm_conn->id, cm_conn, cm_conn->recv_buff, RDMA_RECV_BUFF, cm_conn->recv_mr)) {
        perror("rdma_post_recv()");
        return -1;
    }

    cqe = rdma_get_recv_comp(cm_conn->id, &wc);
    if (cqe <= 0) {
        perror("rdma_get_recv_comp()");
        return -1;
    }

    return 0;
}

/***************************************************************************//**
 * The thread run function
 * 
 ******************************************************************************/
#define SEND_BUFF_SIZE 32

void *thread_run(void *arg) {
    struct cm_connection    *cm_conn = NULL;

    char    send_buff[SEND_BUFF_SIZE] = "hello world";
    int     i = 0;


    if ( !(cm_conn = build_connection()) ) {
        return NULL;
    }

    for (i = 0; i < request_number; ++i) {
        sprintf(send_buff, "message %d\n", i);

        if (0 != send_msg(cm_conn, send_buff, SEND_BUFF_SIZE)) {
            printf("send_msg() error!\n");
            break;
        }

        if (is_recv && 0 != recv_msg(cm_conn)) {
            printf("recv_msg() error!\n");
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
main(int argc, char *argv[]) {
    pthread_t   *threads = NULL;
    struct timespec start,
                    finish;
    char        c = '\0';
    int         i = 0;

    while (-1 != (c = getopt(argc, argv,
            "c:"    /* thread number */
            "r:"    /* request number per thread */
            "t:"    /* last time, secs */
            "p:"    /* listening port */
            "s:"    /* server ip */
            "R"     /* whether receive message from server */
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
                str_port = optarg;
                break;
            case 's':
                str_server = optarg;
                break;
            default:
                assert(0);
        }
    }

    threads = calloc(thread_number, sizeof(pthread_t));

    clock_gettime(CLOCK_REALTIME, &start);

    if (1 == thread_number) {
        thread_run(NULL);

    } else {
        for (i = 0; i < thread_number; ++i) {
            printf("Thread %d\n begin\n", i);

            if (0 != pthread_create(threads+i, NULL, thread_run, NULL)) {
                return -1;
            }
        }

        for (i = 0; i < thread_number; ++i) {
            pthread_join(threads[i], NULL);
            printf("Thread %d terminated.\n", i);
        }
    }

    clock_gettime(CLOCK_REALTIME, &finish);

    printf("Cost time: %lf secs\n", (double)(finish.tv_sec-start.tv_sec + 
                (double)(finish.tv_nsec - start.tv_nsec)/1000000000 ));

    free(threads);

    return 0;
}

