#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>  // Para temporização

#define PORTA 8088
#define TAM_BUFFER 1024

long tempo_em_milisegundos() {
    struct timeval tempo;
    gettimeofday(&tempo, NULL);
    return tempo.tv_sec * 1000 + tempo.tv_usec / 1000;
}

int main() {
    int sockfd;
    struct sockaddr_in servidor;
    char buffer[TAM_BUFFER];
    FILE *arquivo;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(PORTA);
    servidor.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr*)&servidor, sizeof(servidor)) < 0) {
        perror("Erro ao conectar ao servidor");
        close(sockfd);
        exit(1);
    }

    arquivo = fopen("arquivo_recebido.txt", "wb");
    if (arquivo == NULL) {
        perror("Erro ao criar arquivo");
        close(sockfd);
        exit(1);
    }

    long inicio = tempo_em_milisegundos();

    int recebidos;
    while ((recebidos = recv(sockfd, buffer, TAM_BUFFER, 0)) > 0) {
        fwrite(buffer, 1, recebidos, arquivo);
    }

    long fim = tempo_em_milisegundos();

    printf("Arquivo recebido com sucesso!\n");
    printf("Tempo de transferência: %ld ms\n", fim - inicio);

    fclose(arquivo);
    close(sockfd);
    return 0;
}