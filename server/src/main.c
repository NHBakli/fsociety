#include <string.h>
#include <pthread.h>
#include "../include/utils.h"
#include "../include/server.h"
#include "../include/controller.h"

int main() {
    memset(agents, 0, sizeof(agents));
    pthread_t t_agents, t_ctrl;

    pthread_create(&t_agents, NULL, listen_agents, NULL);
    pthread_create(&t_ctrl, NULL, listen_controler, NULL);

    pthread_join(t_agents, NULL);
    pthread_join(t_ctrl, NULL);

    return 0;
}
