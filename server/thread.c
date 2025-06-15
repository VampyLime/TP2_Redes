#include "http_utils.h" // Inclui as funções e definições comuns

#include <pthread.h> // Necessário para threads POSIX

#define NUM_THREADS 4
#define QUEUE_SIZE 100

// Estrutura para armazenar a tarefa (socket + endereço do cliente)
typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
} Task;

// --- Fila de Tarefas ---
Task task_queue[QUEUE_SIZE]; // Fila de tarefas agora armazena a estrutura Task
int queue_count = 0;
int queue_head = 0;
int queue_tail = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void enqueue_task(int client_socket, struct sockaddr_in client_address) {
    pthread_mutex_lock(&queue_mutex);
    if (queue_count < QUEUE_SIZE) {
        task_queue[queue_tail].client_socket = client_socket;
        task_queue[queue_tail].client_address = client_address;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
        pthread_cond_signal(&queue_cond); // Sinaliza para uma thread trabalhadora
    } else {
        printf("Fila de tarefas cheia. Descartando conexão de %s.\n", inet_ntoa(client_address.sin_addr));
        close(client_socket);
    }
    pthread_mutex_unlock(&queue_mutex);
}

Task dequeue_task() {
    Task task;
    task.client_socket = -1; // Valor de erro
    pthread_mutex_lock(&queue_mutex);
    while (queue_count == 0) {
        // Fila vazia, espera por um sinal
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    task = task_queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_count--;
    pthread_mutex_unlock(&queue_mutex);
    return task;
}

// --- Funções de Conexão e Thread ---
// Esta função é agora a mesma que em iterativo.c e fork.c, mas com logs de thread ID.
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
        printf("[Thread %ld] Erro ao ler requisição ou conexão fechada pelo cliente %s.\n", pthread_self(), client_ip);
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0'; // Garante terminação nula

    // Parsear a linha de requisição (ex: GET /index.html HTTP/1.1)
    if (sscanf(buffer, "%15s %255s %15s", method, path, http_version) != 3) {
        send_error_response(client_socket, 400, "Bad Request", "<h1>400 Bad Request</h1><p>Sua requisi&ccedil;&atilde;o est&aacute; malformada.</p>");
        status_code = 400;
        log_request(client_ip, timestamp, "UNKNOWN", "UNKNOWN", status_code);
        close(client_socket);
        return;
    }

    printf("[Thread %ld] Requisição de %s: %s %s %s\n", pthread_self(), client_ip, method, path, http_version);

    // Apenas lidamos com requisições GET por enquanto
    if (strcmp(method, "GET") != 0) {
        send_error_response(client_socket, 501, "Not Implemented", "<h1>501 Not Implemented</h1><p>M&eacute;todo n&atilde;o suportado.</p>");
        status_code = 501;
        log_request(client_ip, timestamp, method, path, status_code);
        close(client_socket);
        return;
    }

    // Construir o caminho completo do arquivo
    if (strcmp(path, "/") == 0) {
        strcpy(full_path, ".");
        strcat(full_path, "/index.html");
    } else {
        strcpy(full_path, ".");
        strcat(full_path, path);
    }

    // Verificar se o arquivo existe e é um arquivo regular
    if (stat(full_path, &file_stat) == -1 || !S_ISREG(file_stat.st_mode)) {
        send_error_response(client_socket, 404, "Not Found", "<h1>404 Not Found</h1><p>O recurso solicitado n&atilde;o foi encontrado.</p>");
        status_code = 404;
        log_request(client_ip, timestamp, method, path, status_code);
        close(client_socket);
        return;
    }

    // Abrir o arquivo em modo binário
    file = fopen(full_path, "rb");
    if (!file) {
        send_error_response(client_socket, 500, "Internal Server Error", "<h1>500 Internal Server Error</h1><p>N&atilde;o foi poss&iacute;vel abrir o arquivo.</p>");
        status_code = 500;
        log_request(client_ip, timestamp, method, path, status_code);
        close(client_socket);
        return;
    }

    // Construir cabeçalhos da resposta HTTP 200 OK
    const char *mime_type = get_mime_type(full_path);
    snprintf(response_header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n" // Fechar a conexão após a resposta
             "\r\n",
             mime_type, (long)file_stat.st_size);

    // Enviar cabeçalhos
    write(client_socket, response_header, strlen(response_header));

    // Enviar conteúdo do arquivo
    int bytes_sent;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        bytes_sent = write(client_socket, buffer, bytes_read);
        if (bytes_sent != bytes_read) {
            perror("[Thread %ld] Erro ao enviar dados do arquivo");
            break;
        }
    }

    fclose(file);
    close(client_socket);
    printf("[Thread %ld] Conexão com %s fechada.\n", pthread_self(), client_ip);

    log_request(client_ip, timestamp, method, path, status_code);
}

void* worker_thread() {
    while (1) {
        Task task = dequeue_task();
        if (task.client_socket != -1) {
            // Passa o socket e o endereço do cliente para handle_connection
            handle_connection(task.client_socket, &task.client_address);
        }
    }
    return NULL;
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

    // Criação do pool de threads
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            die("pthread_create failed");
        }
    }

    // Criar o socket do servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        die("socket failed");
    }

    // Opcional: Reusar endereço e porta imediatamente após fechar
    // int opt = 1;
    // if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    //     die("setsockopt failed");
    // }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Vincular o socket à porta
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        die("bind failed");
    }

    // Escutar por conexões
    if (listen(server_fd, 20) < 0) { // Aumentei o backlog de listen
        die("listen failed");
    }

    printf("Servidor (Thread Pool) escutando na porta %d com %d threads\n", port, NUM_THREADS);
    printf("Servindo arquivos do diretório atual ('.')\n");

    // Thread principal (produtor)
    while (1) {
        printf("Aguardando nova conexão...\n");
        // accept agora preenche client_address
        if ((client_socket = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen)) < 0) {
            perror("accept failed");
            continue;
        }
        printf("Conexão aceita de %s:%d! Enfileirando tarefa...\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        enqueue_task(client_socket, client_address); // Enfileira o socket e o endereço
    }

    close(server_fd);
    return 0;
}