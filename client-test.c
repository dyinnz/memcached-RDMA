#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdma/rdma_cma.h"
#include "rdma/rdma_verbs.h"

#define RDMA_RECV_BUFF 1024
#define RDMA_MAX_HEAD 16

struct cm_connection {
    struct rdma_cm_id   *id;
    struct ibv_mr       *recv_mr;
    char                head_buff[RDMA_MAX_HEAD];
};

static char *str_server = "127.0.0.1";
static char *str_port = "11211";

int BuildConnect() {
    struct ibv_qp_init_attr attr;
    struct ibv_wc           wc;

	struct rdma_addrinfo    hints = { .ai_port_space = RDMA_PS_TCP },
                            *res = NULL;

    struct rdma_cm_id       *id = NULL;
    struct cm_connection    *cm_conn = NULL;

    int                     cqe = 0;

    cm_conn = calloc(1, sizeof(struct cm_connection));

    if (0 != rdma_getaddrinfo(str_server, str_port, &hints, &res)) {
        perror("rdma_getaddrinfo():");
        return -1;
    }

    memset(&attr, 0, sizeof(attr));
	attr.cap.max_send_wr = attr.cap.max_recv_wr = 1;
	attr.cap.max_send_sge = attr.cap.max_recv_sge = 1;
	attr.cap.max_inline_data = 16;
	attr.qp_context = id;
	attr.sq_sig_all = 1;

    if (0 != rdma_create_ep(&id, res, NULL, &attr)) {
        perror("rdma_create_id():");
        return -1;
    }

    if ( !(cm_conn->recv_mr = rdma_reg_msgs(id, cm_conn->head_buff, RDMA_MAX_HEAD)) ) {
        perror("rdma_reg_msgs()");
        return -1;
    }

    if (0 != rdma_post_recv(id, cm_conn, cm_conn->head_buff, RDMA_MAX_HEAD, cm_conn->recv_mr)) {
        perror("rdma_post_recv()");
        return -1;
    }

    if (0 != rdma_connect(id, NULL)) {
        perror("rdma_connect()");
        return -1;
    }

    cqe = rdma_get_recv_comp(id, &wc);
    if (cqe <= 0) {
        perror("rdma_get_recv_comp()");
        return -1;
    }
    printf("get recv complete: %d\n", cqe);

    printf("[Recv head msg]:\n%s\n", cm_conn->head_buff);

    rdma_disconnect(id);
    rdma_dereg_mr(cm_conn->recv_mr);
    rdma_destroy_ep(id);
    free(cm_conn);

    return 0;
}

int main() {
    BuildConnect();
    return 0;
}

