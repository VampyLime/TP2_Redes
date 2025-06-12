#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

// Incluir http_response e die()

void handle_connection(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    printf("[PID %d] Requisição recebida.\n", getpid());
    write(client_socket, http_response, strlen(http_response));
    close(client_socket);
    printf("[PID %d] Conexão fechada.\n", getpid());
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pid_t pid;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) die("socket failed");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) die("bind failed");
    if (listen(server_fd, 10) < 0) die("listen failed");

    printf("Servidor (Fork) escutando na porta %d\n", port);

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            continue; // Continua para a próxima iteração
        }
        
        printf("Conexão aceita! Criando processo filho...\n");
        pid = fork();

        if (pid < 0) {
            die("fork failed");
        }

        if (pid == 0) { // Processo Filho
            close(server_fd); // O filho não precisa do socket de escuta
            handle_connection(client_socket);
            exit(EXIT_SUCCESS); // Filho termina após tratar a conexão
        } else { // Processo Pai
            close(client_socket); // O pai não precisa do socket do cliente
            // Evita processos zumbis sem bloquear o pai
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }
    close(server_fd);
    return 0;
}