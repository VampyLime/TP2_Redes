#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define TAM_BUFFER 4096

long tempo_em_milisegundos() {
    struct timeval tempo;
    gettimeofday(&tempo, NULL);
    return tempo.tv_sec * 1000 + tempo.tv_usec / 1000;
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in servidor;
    char buffer[TAM_BUFFER];
    FILE *arquivo;
    char *host = "127.0.0.1"; // Endereço IP padrão
    int port = 8080; // Porta padrão para o servidor HTTP
    char *request_path = "/"; // Caminho padrão para a requisição HTTP

    if (argc > 1) {
        char *host = argv[1];
    }
    if (argc > 2) {
        int port = atoi(argv[2]);
    }
    if (argc > 3) {
        request_path = argv[3]; // Permite especificar o caminho da requisição
    }


    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(port);
    servidor.sin_addr.s_addr = inet_addr(host);

    printf("Tentando conectar a %s:%d...\n", host, port);
    if (connect(sockfd, (struct sockaddr*)&servidor, sizeof(servidor)) < 0) {
        perror("Erro ao conectar ao servidor");
        close(sockfd);
        exit(1);
    }
    printf("Conectado ao servidor.\n");

    // Construir a requisição HTTP GET
    char http_request[512];
    snprintf(http_request, sizeof(http_request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: MeuClienteTCP/1.0\r\n"
             "Accept: */*\r\n"
             "Connection: close\r\n"
             "\r\n",
             request_path, host, port);

    // Enviar a requisição HTTP
    printf("Enviando requisição:\n%s\n", http_request);
    if (send(sockfd, http_request, strlen(http_request), 0) < 0) {
        perror("Erro ao enviar requisição");
        close(sockfd);
        exit(1);
    }

    arquivo = fopen("arquivo_recebido_cliente_http.bin", "wb"); // Nome do arquivo alterado para evitar conflito
    if (arquivo == NULL) {
        perror("Erro ao criar arquivo");
        close(sockfd);
        exit(1);
    }

    long inicio = tempo_em_milisegundos();

    int total_bytes_recebidos = 0;
    int bytes_recebidos;
    while ((bytes_recebidos = recv(sockfd, buffer, TAM_BUFFER, 0)) > 0) {
        fwrite(buffer, 1, bytes_recebidos, arquivo);
        total_bytes_recebidos += bytes_recebidos;
    }

    long fim = tempo_em_milisegundos();

    if (bytes_recebidos < 0) {
        perror("Erro ao receber dados");
    } else {
        printf("Arquivo recebido com sucesso! Total de bytes: %d\n", total_bytes_recebidos);
    }
    printf("Tempo de transferência: %ld ms\n", fim - inicio);

    fclose(arquivo);
    close(sockfd);
    return 0;
}