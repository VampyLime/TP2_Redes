#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define NUM_THREADS 4
#define QUEUE_SIZE 100

// Incluir http_response e die()

// --- Fila de Tarefas ---
int task_queue[QUEUE_SIZE];
int queue_count = 0;
int queue_head = 0;
int queue_tail = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void enqueue_task(int client_socket) {
    pthread_mutex_lock(&queue_mutex);
    if (queue_count < QUEUE_SIZE) {
        task_queue[queue_tail] = client_socket;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
        pthread_cond_signal(&queue_cond); // Sinaliza para uma thread trabalhadora
    } else {
        printf("Fila de tarefas cheia. Descartando conexão.\n");
        close(client_socket);
    }
    pthread_mutex_unlock(&queue_mutex);
}

int dequeue_task() {
    int client_socket = -1;
    pthread_mutex_lock(&queue_mutex);
    while (queue_count == 0) {
        // Fila vazia, espera por um sinal
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    client_socket = task_queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_count--;
    pthread_mutex_unlock(&queue_mutex);
    return client_socket;
}

// --- Funções de Conexão e Thread ---
void handle_connection(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    printf("[Thread %ld] Processando socket %d\n", pthread_self(), client_socket);
    write(client_socket, http_response, strlen(http_response));
    close(client_socket);
}

void* worker_thread(void* arg) {
    while (1) {
        int client_socket = dequeue_task();
        if (client_socket != -1) {
            handle_connection(client_socket);
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
    int addrlen = sizeof(address);

    // Criação do pool de threads
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            die("pthread_create failed");
        }
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) die("socket failed");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) die("bind failed");
    if (listen(server_fd, 20) < 0) die("listen failed");

    printf("Servidor (Thread Pool) escutando na porta %d com %d threads\n", port, NUM_THREADS);

    // Thread principal (produtor)
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept failed");
            continue;
        }
        enqueue_task(client_socket);
    }

    close(server_fd);
    return 0;
}