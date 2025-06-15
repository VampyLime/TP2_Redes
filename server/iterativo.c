#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Resposta HTTP básica
const char *http_response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 12\r\n"
                            "\r\n"
                            "Hello, World";

void die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void handle_connection(int client_socket)
{
    char buffer[1024] = {0};
    // Lê a requisição do cliente (não fazemos nada com ela neste exemplo)
    //read(client_socket, buffer, 1024);
    printf("Requisição recebida.\n");

    // Envia a resposta HTTP
    write(client_socket, http_response, strlen(http_response));

    // Fecha a conexão com o cliente
    close(client_socket);
    printf("Conexão fechada.\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 1. Criar o socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        die("socket failed");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer IP
    address.sin_port = htons(port);

    // 2. Vincular o socket à porta
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        die("bind failed");
    }

    // 3. Escutar por conexões
    if (listen(server_fd, 3) < 0)
    {
        die("listen failed");
    }

    printf("Servidor iterativo escutando na porta %d\n", port);

    // 4. Loop principal para aceitar e tratar conexões
    while (1)
    {
        printf("Aguardando nova conexão...\n");
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            die("accept failed");
        }

        printf("Conexão aceita!\n");
        handle_connection(client_socket); // Trata a conexão e a fecha
    }

    return 0;
}