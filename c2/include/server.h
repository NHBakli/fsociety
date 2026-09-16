#ifndef FSO_SERVER_H
#define FSO_SERVER_H

#include <stdint.h>

typedef struct {
    int fd;
    uint16_t port;
} fso_server_t;

int fso_server_init(fso_server_t *srv, uint16_t port);
int fso_server_accept(fso_server_t *srv);
void fso_server_close(fso_server_t *srv);

#endif