#include "http_utils.h" // Inclui o cabeçalho que acabamos de criar

// Função para tratamento de erros
void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Função para obter o tipo MIME com base na extensão do arquivo
const char *get_mime_type(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) return "application/octet-stream"; // Nenhum tipo, ou nome começa com .

    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".txt") == 0) return "text/plain";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".gif") == 0) return "image/gif";
    if (strcmp(dot, ".pdf") == 0) return "application/pdf";
    // Adicione mais tipos MIME conforme necessário
    return "application/octet-stream"; // Tipo padrão para desconhecidos
}

// Função para enviar uma resposta de erro HTTP
void send_error_response(int client_socket, int status_code, const char *status_msg, const char *body) {
    char response_header[BUFFER_SIZE];
    int body_len = strlen(body);

    snprintf(response_header, BUFFER_SIZE,
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_msg, body_len);

    write(client_socket, response_header, strlen(response_header));
    write(client_socket, body, body_len);
    close(client_socket);
}

// Função para logar as requisições
void log_request(const char *client_ip, const char *timestamp, const char *method, const char *path, int status_code) {
    printf("[%s] %s %s \"%s %s\" %d\n", timestamp, client_ip, method, path, "HTTP/1.1", status_code);
    // Para logar em arquivo, você pode abrir um arquivo aqui e escrever nele.
    // Ex: FILE *log_file = fopen("server.log", "a");
    //     fprintf(log_file, "[%s] %s %s \"%s %s\" %d\n", timestamp, client_ip, method, path, "HTTP/1.1", status_code);
    //     fclose(log_file);
}