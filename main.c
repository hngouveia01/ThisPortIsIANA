#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <regex.h>

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

static int is_invalid_request(const char *buffer)
{
    regex_t regex;

    if (regcomp(&regex, TPI_REGEXP_VALIDATION, REG_EXTENDED))
        return -1;

    if (regexec(&regex, buffer, 0, NULL, 0))
        return -1;

    return 0;
}

static int is_in_between_port_range(const char *buffer,
                                    int port,
                                    const char *prot,
                                    char *serv_name)
{
    int i;
    unsigned int prot_id = 0;
    int found = 0;

    for (i = 0; prot_initial[i].initial; i++) {
        if (0 == strcmp(prot, prot_initial[i].initial)) {
            prot_id = prot_initial[i].prot;
            found = 1;
            break;
        }
    }

    if (!found)
        return 0;

    for (i = 0; port_interval[i].min_port != UINT_MAX; i++) {
        if ((port >= port_interval[i].min_port)
            && (port <= port_interval[i].max_port)) {
            if (prot_id & port_interval[i].prot) {
                serv_name = strdup(port_interval[i].serv_name);
                return 1;
            }
        }
    }
}

static ssize_t none_response(struct sockaddr_un *addr_cli)
{
    return sendto(sock,
                  "None",
                  sizeof("None"),
                  0,
                  (struct sockaddr *) addr_cli,
                  sizeof(struct sockaddr_un));
}

static ssize_t confirm_response(struct sockaddr_un *addr_cli,
                                char *serv_name,
                                char confirmation)
{
    if (NULL == serv_name)
        return none_response(addr_cli);

    if ('-' == confirmation) {
        char buffer[100];
        sprintf(buffer, "1/%s", serv_name);
        return sendto(sock,
                      buffer,
                      strlen(buffer) + 1,
                      0,
                      (struct sockaddr *) addr_cli,
                      sizeof(struct sockaddr_un));
    }

    return sendto(sock,
                  serv_name,
                  strlen(serv_name) + 1,
                  0, (struct sockaddr *) addr_cli,
                  sizeof(struct sockaddr_un));
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

static int set_transport_protocol(char *prot,
                                  struct tpi_prot_serv_name *data)
{
    int i;

    for (i = 0; prot_initial[i].initial; i++) {
        if (0 == strcmp(prot, prot_initial[i].initial)) {
            data->prot |= prot_initial[i].prot;
            return 1;
        }
    }
    return 0;
}

static int prot_cmp(struct tpi_prot_serv_name *a,
                    struct tpi_prot_serv_name *b)
{
    if (a->prot & b->prot)
        return 0;
    return 1;
}

static int tpi_loop(struct rbtree *tree)
{
    int sun_size = sizeof(struct sockaddr_un);

    while (!stop) {
        char buffer[100];
        struct tpi_node node;
        struct tpi_prot_serv_name *node_tmp = NULL;
        struct sockaddr_un addr_cli;

        bzero(buffer, sizeof(buffer));

        recvfrom(sock,
                 buffer,
                 sizeof(buffer),
                 0,
                 (struct sockaddr *) &addr_cli,
                 (socklen_t *) &sun_size);

        if (is_invalid_request(buffer)) {
            none_response(&addr_cli);
            continue;
        }

        char *p = buffer;
        char *cursor = strsep(&p, "/");
        char *prot = p;

        if ('-' == buffer[0])
            cursor++;

        int port = atoi(cursor);

        node.port = port;

        struct rbtree_node *node_found = rbtree_lookup(&node.node, tree);

        if (!node_found) {
            char *serv_name = NULL;
            int ret = is_in_between_port_range(buffer, port, prot, serv_name);

            if (ret)
                confirm_response(&addr_cli, serv_name, buffer[0]);
            else
                none_response(&addr_cli);
            free(serv_name);
            continue;
        }

        struct tpi_node *ret = rbtree_container_of(node_found, struct tpi_node, node);

        struct tpi_prot_serv_name node_to_search;
        bzero(&node_to_search, sizeof(node_to_search));
        int found = set_transport_protocol(p, &node_to_search);

        if (!found) {
            none_response(&addr_cli);
            continue;
        }

        LL_SEARCH(ret->list, node_tmp, &node_to_search, prot_cmp);

        if (NULL != node_tmp) {
            confirm_response(&addr_cli, node_tmp->serv_name, buffer[0]);
            continue;
        }
        none_response(&addr_cli);
    }

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
