#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 30

// Incluir http_response e die()

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], activity, i, valread;
    int max_clients = MAX_CLIENTS;
    int max_sd;
    struct sockaddr_in address;

    // Conjunto de descritores de socket
    fd_set readfds;

    // Inicializa todos os client_socket[] para 0
    for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) die("socket failed");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) die("bind failed");
    if (listen(master_socket, 5) < 0) die("listen failed");

    addrlen = sizeof(address);
    printf("Servidor (select) escutando na porta %d\n", port);

    while (1) {
        FD_ZERO(&readfds); // Limpa o conjunto
        FD_SET(master_socket, &readfds); // Adiciona o socket mestre ao conjunto
        max_sd = master_socket;

        // Adiciona sockets filhos ao conjunto
        for (i = 0; i < max_clients; i++) {
            if (client_socket[i] > 0) FD_SET(client_socket[i], &readfds);
            if (client_socket[i] > max_sd) max_sd = client_socket[i];
        }

        // Espera por atividade em um dos sockets (bloqueante)
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        // Se algo aconteceu no socket mestre, é uma nova conexão
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                die("accept");
            }

            printf("Nova conexão, socket fd: %d\n", new_socket);
            
            // Envia a resposta imediatamente e fecha. Simplificação.
            // Um servidor real leria a requisição primeiro.
            write(new_socket, http_response, strlen(http_response));
            printf("Resposta enviada. Fechando conexão %d.\n", new_socket);
            close(new_socket);
        }
    }
    return 0;
}