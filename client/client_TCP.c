#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>  // Para temporização

#define TAM_BUFFER 4096  // Aumentado para lidar com respostas HTTP completas
#define MAX_HEADER_SIZE 2048 // Tamanho máximo esperado para os cabeçalhos HTTP

long tempo_em_milisegundos() {
    struct timeval tempo;
    gettimeofday(&tempo, NULL);
    return tempo.tv_sec * 1000 + tempo.tv_usec / 1000;
}

// Função para determinar a extensão do arquivo com base no Content-Type
const char *get_extension_from_mime(const char *mime_type) {
    if (strstr(mime_type, "text/html")) return ".html";
    if (strstr(mime_type, "text/css")) return ".css";
    if (strstr(mime_type, "application/javascript")) return ".js";
    if (strstr(mime_type, "application/json")) return ".json";
    if (strstr(mime_type, "text/plain")) return ".txt";
    if (strstr(mime_type, "image/jpeg")) return ".jpg";
    if (strstr(mime_type, "image/png")) return ".png";
    if (strstr(mime_type, "image/gif")) return ".gif";
    if (strstr(mime_type, "application/pdf")) return ".pdf";
    return ".bin"; // Padrão para tipos desconhecidos
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in servidor;
    char buffer[TAM_BUFFER];
    char header_buffer[MAX_HEADER_SIZE]; // Buffer para armazenar cabeçalhos
    FILE *arquivo = NULL; // Inicializa como NULL
    char *host = "127.0.0.1"; // Endereço padrão do servidor
    int port = 8080; // Porta padrão para o servidor HTTP
    char *request_path = "/"; // Caminho padrão para a requisição HTTP
    char file_name_buffer[256];

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    if (argc > 3) {
        request_path = argv[3]; // Permite especificar o caminho da requisição
    }

    // Determinar um nome de arquivo de saída baseado no path da requisição
    const char *last_slash = strrchr(request_path, '/');
    const char *file_base_name = (last_slash == NULL || strlen(last_slash) == 1) ? "index" : last_slash + 1;
    snprintf(file_name_buffer, sizeof(file_name_buffer), "recebido_%s", file_base_name);


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
    printf("Enviando requisição para '%s':\n%s\n", request_path, http_request);
    if (send(sockfd, http_request, strlen(http_request), 0) < 0) {
        perror("Erro ao enviar requisição");
        close(sockfd);
        exit(1);
    }

    long inicio = tempo_em_milisegundos();

    int total_bytes_received = 0;
    int header_bytes = 0;
    char *header_end = NULL;
    int content_length = -1;
    char content_type[256] = "application/octet-stream";
    
    // --- LER CABEÇALHOS HTTP ---
    // Loop para ler o cabeçalho até encontrar "\r\n\r\n"
    while (header_end == NULL && total_bytes_received < MAX_HEADER_SIZE) {
        int bytes_read = recv(sockfd, buffer, TAM_BUFFER, 0);
        if (bytes_read <= 0) {
            perror("Erro ou conexão fechada ao ler cabeçalhos");
            close(sockfd);
            if (arquivo) fclose(arquivo);
            exit(1);
        }
        // Copia para o buffer de cabeçalho
        if (total_bytes_received + bytes_read >= MAX_HEADER_SIZE) {
            fprintf(stderr, "Erro: Cabeçalho HTTP muito grande para o buffer. Aumente MAX_HEADER_SIZE.\n");
            close(sockfd);
            if (arquivo) fclose(arquivo);
            exit(1);
        }
        memcpy(header_buffer + total_bytes_received, buffer, bytes_read);
        total_bytes_received += bytes_read;
        header_buffer[total_bytes_received] = '\0'; // Garante terminação nula

        // Procura pelo fim do cabeçalho
        header_end = strstr(header_buffer, "\r\n\r\n");
    }

    if (!header_end) {
        fprintf(stderr, "Erro: Não foi possível encontrar o fim do cabeçalho HTTP.\n");
        close(sockfd);
        if (arquivo) fclose(arquivo);
        exit(1);
    }

    // Calcula o tamanho real dos cabeçalhos
    header_bytes = (header_end - header_buffer) + 4; // +4 para "\r\n\r\n"

    // Analisar cabeçalhos para Content-Length e Content-Type
    char *line = strtok(header_buffer, "\r\n");
    while (line != NULL) {
        if (strstr(line, "Content-Length:") == line) {
            sscanf(line, "Content-Length: %d", &content_length);
        } else if (strstr(line, "Content-Type:") == line) {
            // Copia o tipo, pulando "Content-Type: "
            strncpy(content_type, line + strlen("Content-Type: "), sizeof(content_type) -1);
            content_type[sizeof(content_type) -1] = '\0'; // Garante nulo
        }
        line = strtok(NULL, "\r\n");
    }

    printf("Cabeçalhos recebidos (tamanho: %d bytes):\n%s\n", header_bytes, header_buffer);
    printf("Content-Type: %s\n", content_type);
    printf("Content-Length: %d\n", content_length != -1 ? content_length : 0);

    // --- Abrir arquivo de saída com a extensão correta ---
    char final_file_name[300];
    snprintf(final_file_name, sizeof(final_file_name), "%s%s", file_name_buffer, get_extension_from_mime(content_type));
    
    arquivo = fopen(final_file_name, "wb");
    if (arquivo == NULL) {
        perror("Erro ao criar arquivo de saída");
        close(sockfd);
        exit(1);
    }
    printf("Salvando conteúdo em '%s'\n", final_file_name);

    // --- Escrever o corpo (payload) no arquivo ---
    // Primeiramente, escreve a parte do buffer que já contém dados do corpo
    int body_start_index = header_bytes;
    int bytes_in_buffer_after_headers = total_bytes_received - body_start_index;

    if (bytes_in_buffer_after_headers > 0) {
        fwrite(header_buffer + body_start_index, 1, bytes_in_buffer_after_headers, arquivo);
    }

    int bytes_to_receive = content_length - bytes_in_buffer_after_headers;
    if (content_length == -1) { // Se Content-Length não foi especificado, leia até o fim da conexão
        bytes_to_receive = -1; // Sinaliza leitura contínua
    }

    // Continua lendo e escrevendo o restante do corpo
    while (bytes_to_receive != 0) { // Loop infinito se content_length for -1 (leitura até EOF)
        int received_chunk_size = recv(sockfd, buffer, TAM_BUFFER, 0);
        if (received_chunk_size <= 0) {
            if (received_chunk_size < 0) perror("Erro ao receber dados do corpo");
            break; // Conexão fechada ou erro
        }
        fwrite(buffer, 1, received_chunk_size, arquivo);
        if (bytes_to_receive > 0) { // Se Content-Length foi especificado
            bytes_to_receive -= received_chunk_size;
            if (bytes_to_receive < 0) bytes_to_receive = 0; // Evita valores negativos
        }
    }

    long fim = tempo_em_milisegundos();

    printf("Transferência completa. Tempo: %ld ms\n", fim - inicio);

    fclose(arquivo);
    close(sockfd);
    return 0;
}