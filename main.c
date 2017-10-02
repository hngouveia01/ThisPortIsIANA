#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include "tpi_definitions.h"

static volatile sig_atomic_t stop = 0;

static void stop_signal(int signo)
{
    if (SIGTERM == signo)
        stop = 1;
}

static int cmp_nds(const struct rbtree_node *a,
                   const struct rbtree_node *b)
{
    struct tpi_node *p = rbtree_container_of(a, struct tpi_node, node);
    struct tpi_node *q = rbtree_container_of(b, struct tpi_node, node);
    return p->port - q->port;
}

static int read_all_file(char **content, size_t *file_size)
{
    FILE *f = fopen(TPI_IANA_TXT, "r");

    if (NULL == f)
        return -1;

    fseek(f, 0, SEEK_END);
    *file_size = ftell(f);
    rewind(f);

    *content = malloc(*file_size + 1);

    if (NULL == *content) {
        fclose(f);
        return -1;
    }

    size_t nmemb_read = fread(*content, *file_size, 1, f);
    fclose(f);

    if ((0 == nmemb_read) && (0 == feof(f))) {
        free(*content);
        return -1;
    }
    return 0;
}

static void skip_header(char **content, char **cursor)
{
    char *p = strstr(*content, "$");
    p = strstr(++p, "$");
    p += 2;
    *cursor = p;
}

static void get_tpi_line(struct tpi_line_parsed *data, char **p)
{
    data->port_number = strsep(p, "/");
    data->prot = *p;
    strsep(p, ">");
    data->serv_name = strsep(p, "\n");
}

static int socket_init(void)
{
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);

    if (-1 == sock) {
        perror(NULL);
        return -1;
    }

    unlink(TPI_SRV_SOCK);

    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TPI_SRV_SOCK, sizeof(addr.sun_path) - 1);

    if (-1 == bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))) {
        perror(NULL);
        return -1;
    }
    return 0;
}

static int service_init(struct rbtree *tree)
{
    char *content = NULL;
    char *p = NULL;
    size_t file_size = 0;

    rbtree_init(tree, cmp_nds, 0);

    if (socket_init())
        return -1;

    if (read_all_file(&content, &file_size))
        return -1;

    skip_header(&content, &p);

    do {
        int i;
        struct tpi_line_parsed line;

        get_tpi_line(&line, &p);

        struct tpi_node *node = calloc(1, sizeof(struct tpi_node));

        struct tpi_prot_serv_name *prot_serv_name = calloc(1, sizeof(struct tpi_prot_serv_name));

        node->port = atoi(line.port_number);

        for (i = 0; prot_initial[i].initial; i++) {
            if (0 == strcmp(line.prot, prot_initial[i].initial)) {
                prot_serv_name->prot |= prot_initial[i].prot;
                break;
            }
        }
        prot_serv_name->serv_name = strdup(line.serv_name);
        LL_APPEND(node->list, prot_serv_name);
        struct rbtree_node *ret = rbtree_insert(&node->node, tree);

        if (NULL != ret) {
            LL_DELETE(node->list, prot_serv_name);
            struct tpi_node *node = rbtree_container_of(ret, struct tpi_node, node);
            LL_APPEND(node->list, prot_serv_name);
            free(node);
        }
    } while (p < (content + file_size));

    free(content);

    return 0;
}

static int tpi_loop(struct rbtree *tree)
{
    return 0;
}

int main(int argc, const char * argv[]) {
    struct rbtree tree;

    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &stop_signal;
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);

    if (service_init(&tree))
        return -1;

    if (tpi_loop(&tree))
        return -2;

    return 0;
}
