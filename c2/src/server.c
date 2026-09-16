#include "server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int fso_server_init(fso_server_t *srv, uint16_t port)
{
    /* Création du socket, bind, listen... */
    srv->fd = socket(AF_INET, SOCK_STREAM, 0);
    srv->port = port;
    /* ... */
    return 0;
}
