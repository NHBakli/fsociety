#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ncurses.h>

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { perror("socket"); return -1; }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(5555);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("connect"); close(sock); return -1;
    }
    return sock;
}

char *send_command(const char *cmd) {
    int sock = connect_to_server();
    if (sock == -1) return NULL;

    send(sock, cmd, strlen(cmd), 0);

    static char buf[65536];
    memset(buf, 0, sizeof(buf));
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    close(sock);

    if (n <= 0) return NULL;
    buf[n] = '\0';
    return buf;
}

int get_agents(char agents[][64]) {
    char *response = send_command("LIST");
    if (response == NULL || strlen(response) == 0)
        return 0;

    int count = 0;
    char *line = strtok(response, "\n");
    while (line != NULL && count < 32) {
        strncpy(agents[count], line, 63);
        count++;
        line = strtok(NULL, "\n");
    }
    return count;
}

int select_from_menu(const char *title, char items[][64], int n) {
    if (n == 0) {
        clear();
        mvprintw(0, 0, "Aucun agent connecté.");
        mvprintw(2, 0, "Appuie sur une touche...");
        getch();
        return -1;
    }

    int selected = 0;
    while (1) {
        clear();
        mvprintw(0, 0, "%s", title);
        for (int i = 0; i < n; i++) {
            if (i == selected)
                mvprintw(2 + i, 2, "> %s", items[i]);
            else
                mvprintw(2 + i, 2, "  %s", items[i]);
        }
        mvprintw(2 + n + 1, 0, "[ESC] Retour");

        int key = getch();
        if (key == KEY_UP && selected > 0)
            selected--;
        else if (key == KEY_DOWN && selected < n - 1)
            selected++;
        else if (key == 10)
            return selected;
        else if (key == 27)
            return -1;
    }
}

void action_list() {
    char *response = send_command("LIST");
    clear();
    if (response == NULL || strlen(response) == 0)
        mvprintw(0, 0, "Aucun agent connecté.");
    else {
        mvprintw(0, 0, "=== Agents connectés ===");
        mvprintw(2, 0, "%s", response);
    }
    mvprintw(10, 0, "Appuie sur une touche...");
    getch();
}

void action_task() {
    char agents[32][64];
    int n = get_agents(agents);

    int idx = select_from_menu("=== Choisir un agent (TASK) ===", agents, n);
    if (idx == -1) return;

    clear();
    mvprintw(0, 0, "Agent: %s", agents[idx]);
    mvprintw(2, 0, "Commande: ");
    refresh();

    echo();
    curs_set(1);
    char cmd[1024] = {0};
    getnstr(cmd, sizeof(cmd) - 1);
    noecho();
    curs_set(0);

    char msg[2048] = {0};
    snprintf(msg, sizeof(msg), "TASK %s %s", agents[idx], cmd);
    char *response = send_command(msg);

    clear();
    mvprintw(0, 0, "Réponse: %s", response ? response : "erreur");
    mvprintw(2, 0, "Appuie sur une touche...");
    getch();
}

void action_result() {
    char agents[32][64];
    int n = get_agents(agents);

    int idx = select_from_menu("=== Choisir un agent (RESULT) ===", agents, n);
    if (idx == -1) return;

    char msg[128] = {0};
    snprintf(msg, sizeof(msg), "RESULT %s", agents[idx]);
    char *response = send_command(msg);

    clear();
    if (response == NULL || strcmp(response, "NONE") == 0)
        mvprintw(0, 0, "Pas de résultat disponible.");
    else {
        mvprintw(0, 0, "=== Résultat de %s ===", agents[idx]);
        mvprintw(2, 0, "%s", response);
    }
    mvprintw(20, 0, "Appuie sur une touche...");
    getch();
}

int main() {
    const char *menu[] = { "LIST", "TASK", "RESULT", "Exit" };
    int n_menu = 4;
    int selected = 0;

    initscr();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);

    while (1) {
        clear();
        mvprintw(0, 0, "=== C2 Controller ===");
        for (int i = 0; i < n_menu; i++) {
            if (i == selected)
                mvprintw(2 + i, 2, "> %s", menu[i]);
            else
                mvprintw(2 + i, 2, "  %s", menu[i]);
        }

        int key = getch();
        if (key == KEY_UP && selected > 0)
            selected--;
        else if (key == KEY_DOWN && selected < n_menu - 1)
            selected++;
        else if (key == 10) {
            if (selected == 0) action_list();
            else if (selected == 1) action_task();
            else if (selected == 2) action_result();
            else if (selected == 3) break;
        }
    }

    endwin();
    return 0;
}
