#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/session.h"
#include "../include/server.h"


void *handle_agent_connection(void *arg) {
    int client_sock = (int)(intptr_t)arg;
    char buf[2048] = {0};
    char current_agent_id[64] = {0};

    while (1) {
        memset(buf, 0, sizeof(buf));
        int n = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            break;
        buf[n] = '\0';

        char type[16] = {0};
        char agent_id[64] = {0};
        if (sscanf(buf, "%15s %63s", type, agent_id) < 2)
            continue;

        if (strcmp(type, "BEACON") == 0) {
            strncpy(current_agent_id, agent_id, sizeof(current_agent_id) - 1);
            handle_beacon(client_sock, agent_id);
        } else if (strcmp(type, "RESULT") == 0) {
            char *res_start = buf + strlen("RESULT ") + strlen(agent_id) + 1;
            handle_result(client_sock, agent_id, res_start);
        }
    }

    if (current_agent_id[0] != '\0') {
        pthread_mutex_lock(&agents_mutex);
        int idx = find_agent(current_agent_id);
        if (idx >= 0)
            memset(&agents[idx], 0, sizeof(AgentSlot));
        pthread_mutex_unlock(&agents_mutex);
        printf("[!] Agent %s déconnecté\n", current_agent_id);
    }
    close(client_sock);
    return NULL;
}

void *listen_agents(void *arg) {
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT_AGENTS);

    bind(srv, (struct sockaddr*)&addr, sizeof(addr));
    listen(srv, SOMAXCONN);
    printf("[*] En écoute agents sur le port %d\n", PORT_AGENTS);

    while (1) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_agent_connection, (void*)(intptr_t)client);
        pthread_detach(tid);
    }
    return NULL;
}