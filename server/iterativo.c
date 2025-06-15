#include "http_utils.h"

void handle_connection(int client_socket, struct sockaddr_in *client_address) {
    char buffer[BUFFER_SIZE] = {0};
    char method[16], path[256], http_version[16];
    char full_path[512];
    FILE *file = NULL;
    struct stat file_stat;
    int bytes_read;
    char response_header[BUFFER_SIZE];
    int status_code = 200; // Default OK

    // Obter IP do cliente para o log
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_address->sin_addr), client_ip, INET_ADDRSTRLEN);

    // Obter timestamp para o log
    time_t rawtime;
    struct tm *info;
    char timestamp[80];
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(timestamp, 80, "%d/%b/%Y:%H:%M:%S %z", info);

    bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        printf("Erro ao ler requisição ou conexão fechada pelo cliente.\n");
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0'; // Garante terminação nula

    // Parsear a linha de requisição (ex: GET /index.html HTTP/1.1)
    // Usamos sscanf para extrair o método, o path e a versão HTTP
    if (sscanf(buffer, "%15s %255s %15s", method, path, http_version) != 3) {
        send_error_response(client_socket, 400, "Bad Request", "<h1>400 Bad Request</h1><p>Sua requisi&ccedil;&atilde;o est&aacute; malformada.</p>");
        status_code = 400;
        log_request(client_ip, timestamp, "UNKNOWN", "UNKNOWN", status_code);
        return;
    }

    printf("Requisição de %s: %s %s %s\n", client_ip, method, path, http_version);

    // Apenas lidamos com requisições GET por enquanto
    if (strcmp(method, "GET") != 0) {
        send_error_response(client_socket, 501, "Not Implemented", "<h1>501 Not Implemented</h1><p>M&eacute;todo n&atilde;o suportado.</p>");
        status_code = 501;
        log_request(client_ip, timestamp, method, path, status_code);
        return;
    }

    // Construir o caminho completo do arquivo
    // Se o path for "/", servir index.html
    if (strcmp(path, "/") == 0) {
        strcpy(full_path, "."); // Servir da raiz do diretório do servidor
        strcat(full_path, "/index.html"); // Padrão
    } else {
        strcpy(full_path, ".");
        strcat(full_path, path);
    }

    // Verificar se o arquivo existe e é um arquivo regular
    if (stat(full_path, &file_stat) == -1 || !S_ISREG(file_stat.st_mode)) {
        send_error_response(client_socket, 404, "Not Found", "<h1>404 Not Found</h1><p>O recurso solicitado n&atilde;o foi encontrado.</p>");
        status_code = 404;
        log_request(client_ip, timestamp, method, path, status_code);
        return;
    }

    // Abrir o arquivo em modo binário
    file = fopen(full_path, "rb");
    if (!file) {
        send_error_response(client_socket, 500, "Internal Server Error", "<h1>500 Internal Server Error</h1><p>N&atilde;o foi poss&iacute;vel abrir o arquivo.</p>");
        status_code = 500;
        log_request(client_ip, timestamp, method, path, status_code);
        return;
    }

    // Construir cabeçalhos da resposta HTTP 200 OK
    const char *mime_type = get_mime_type(full_path);
    snprintf(response_header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n" // Fechar a conexão após a resposta (servidor iterativo)
             "\r\n",
             mime_type, (long)file_stat.st_size);

    // Enviar cabeçalhos
    write(client_socket, response_header, strlen(response_header));

    // Enviar conteúdo do arquivo
    int bytes_sent;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        bytes_sent = write(client_socket, buffer, bytes_read);
        if (bytes_sent != bytes_read) {
            perror("Erro ao enviar dados do arquivo");
            break;
        }
    }

    fclose(file);
    close(client_socket);
    printf("Conexão com %s fechada.\n", client_ip);

    log_request(client_ip, timestamp, method, path, status_code);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd, client_socket;
    struct sockaddr_in address;
    struct sockaddr_in client_address; // Para armazenar o endereço do cliente
    socklen_t client_addrlen = sizeof(client_address); // Para accept()

    // 1. Criar o socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        die("socket failed");
    }

    // Opcional: Reusar endereço e porta imediatamente após fechar
    // int opt = 1;
    // if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    //     die("setsockopt failed");
    // }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer IP
    address.sin_port = htons(port);

    // 2. Vincular o socket à porta
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        die("bind failed");
    }

    // 3. Escutar por conexões
    if (listen(server_fd, 10) < 0) { // Aumentei o backlog de listen
        die("listen failed");
    }

    printf("Servidor iterativo escutando na porta %d\n", port);
    printf("Servindo arquivos do diretório atual ('.')\n");

    // 4. Loop principal para aceitar e tratar conexões
    while (1) {
        printf("Aguardando nova conexão...\n");
        // accept agora preenche client_address
        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen)) < 0) {
            perror("accept failed"); // Usar perror para erros de accept
            continue; // Continua para a próxima iteração
        }

        printf("Conexão aceita de %s:%d!\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        handle_connection(client_socket, &client_address); // Passa o endereço do cliente
    }

    close(server_fd);
    return 0;
}