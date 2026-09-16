#include "server.h"   /* On inclut le .h, pas le .c */

int main(void)
{
    fso_server_t srv;
    fso_server_init(&srv, 8443);
    /* ... */
}