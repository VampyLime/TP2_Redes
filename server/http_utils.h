#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>

#define BUFFER_SIZE 4096 // Tamanho do buffer para leitura/escrita

// Declaração da função para tratamento de erros
void die(const char *msg);

// Declaração da função para obter o tipo MIME
const char *get_mime_type(const char *file_name);

// Declaração da função para enviar uma resposta de erro HTTP
void send_error_response(int client_socket, int status_code, const char *status_msg, const char *body);

// Declaração da função para logar as requisições
void log_request(const char *client_ip, const char *timestamp, const char *method, const char *path, int status_code);
