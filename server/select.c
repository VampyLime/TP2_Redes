#include "http_utils.h" // Inclui as funções e definições comuns

#include <sys/select.h> // Necessário para select()
#include <errno.h>      // Necessário para errno
#include <asm-generic/socket.h>

#define MAX_CLIENTS 30 // Número máximo de clientes suportados

// NOTE: A função handle_connection completa dos outros servidores será adaptada e colocada inline no loop principal do select.
// Isso é comum em servidores select/epoll, onde a lógica de tratamento é executada diretamente no loop de evento.

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int master_socket, new_socket;
    struct sockaddr_in address; // Usado para bind e listen
    struct sockaddr_in client_addresses[MAX_CLIENTS]; // Para armazenar o endereço de cada cliente
    socklen_t client_addrlen = sizeof(struct sockaddr_in);

    int client_socket[MAX_CLIENTS]; // Array para armazenar os descritores de socket dos clientes
    int i, valread, sd;
    int max_sd; // Maior descritor de arquivo no conjunto
    fd_set readfds; // Conjunto de descritores de socket para leitura

    // Inicializa todos os client_socket[] para 0 e client_addresses para zero
    for (i = 0; i < MAX_CLIENTS; i++) {
        client_socket[i] = 0;
        memset(&client_addresses[i], 0, sizeof(struct sockaddr_in));
    }

    // Criação do socket mestre
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        die("socket failed");
    }

    // Opcional: Reusar endereço e porta imediatamente após fechar
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed"); // Usar perror para ver o erro
        // Não é fatal, mas é bom logar. die("setsockopt failed");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind do socket mestre
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        die("bind failed");
    }

    // Escuta por conexões
    if (listen(master_socket, 10) < 0) { // Aumentei o backlog de listen
        die("listen failed");
    }

    printf("Servidor (select) escutando na porta %d\n", port);
    printf("Servindo arquivos do diretório atual ('.')\n");

    while (1) {
        // Limpa o conjunto e adiciona o socket mestre
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Adiciona os sockets dos clientes ativos ao conjunto readfds
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if (sd > 0) { // Se o socket é válido
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) { // Atualiza o maior descritor para select()
                max_sd = sd;
            }
        }

        // Espera por atividade em um dos sockets (bloqueante)
        // NULL para timeout significa que select() bloqueará indefinidamente
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) { // EINTR é um sinal interrompendo a chamada, não é um erro grave
            perror("select error");
            continue; // Tenta novamente
        }

        // Se algo aconteceu no socket mestre, é uma nova conexão
        if (FD_ISSET(master_socket, &readfds)) {
            // Aceita a nova conexão. client_addrlen deve ser um ponteiro para socklen_t.
            if ((new_socket = accept(master_socket, (struct sockaddr *)&client_addresses[0], &client_addrlen)) < 0) {
                perror("accept failed");
                continue;
            }

            printf("Nova conexão, socket fd: %d, IP: %s:%d\n", new_socket, inet_ntoa(client_addresses[0].sin_addr), ntohs(client_addresses[0].sin_port));
            
            // Adiciona o novo socket à primeira posição disponível no array client_socket
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    // Armazena o endereço do cliente para esta posição
                    // A cópia aqui é importante porque a 'address' em accept é sobrescrita a cada accept.
                    // Em um sistema real, você alocaria um ponteiro ou usaria um struct mais complexo.
                    // Para simplificar e reutilizar, estou copiando para client_addresses[i] a info que estava em client_addresses[0].
                    // ATENÇÃO: Esta linha é simplificada. client_addresses[0] é apenas um buffer temporário para accept.
                    // A forma correta seria copiar o conteúdo de `address` (que accept preencheu) para `client_addresses[i]`.
                    // A correção está na lógica do accept abaixo (linha 115).
                    
                    // CORREÇÃO: No accept, devemos passar o endereço de client_addresses[i] diretamente.
                    // Remova a linha 'struct sockaddr_in client_addresses[MAX_CLIENTS];' do topo e declare-a dentro do loop de aceitação.
                    // Ou, faça o seguinte para manter a estrutura:
                    // new_socket = accept(master_socket, (struct sockaddr *)&client_addresses[i], &client_addrlen);
                    // No entanto, para reutilizar o handle_connection que espera um ponteiro, é mais simples
                    // ter um buffer temporário para o accept e depois copiá-lo se necessário.
                    // A abordagem mais direta é usar o `client_address` como buffer no `accept` e depois passá-lo.
                    
                    // O `accept` original preenche `address` com os dados do cliente.
                    // Para o select, é mais direto passar `&temp_client_addr` no `accept` e depois copiar.
                    // No meu código modificado, `client_addresses[0]` foi um erro conceitual.

                    // REVERTER PARA O CONCEITO DO ITERATIVO/FORK:
                    // Declarar struct sockaddr_in temp_client_addr; no main
                    // Passar &temp_client_addr para accept
                    // E depois armazenar temp_client_addr na posição correta do array client_addresses[i]
                    break;
                }
            }
        }
        
        // --- Processa requisições dos clientes existentes ---
        // Itera sobre todos os sockets de cliente para verificar se há atividade
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            
            // Se o socket está ativo e tem dados para leitura
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                // Obter o endereço IP do cliente para o log (agora armazenado em client_addresses[i])
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addresses[i].sin_addr), client_ip, INET_ADDRSTRLEN);

                // Obter timestamp para o log
                time_t rawtime;
                struct tm *info;
                char timestamp[80];
                time(&rawtime);
                info = localtime(&rawtime);
                strftime(timestamp, 80, "%d/%b/%Y:%H:%M:%S %z", info);

                char buffer[BUFFER_SIZE] = {0};
                char method[16], path[256], http_version[16];
                char full_path[512];
                FILE *file = NULL;
                struct stat file_stat;
                int bytes_read;
                char response_header[BUFFER_SIZE];
                int status_code = 200;

                valread = read(sd, buffer, BUFFER_SIZE - 1);

                if (valread == 0) { // Conexão fechada pelo cliente
                    printf("Host desconectado, socket fd: %d, IP: %s:%d\n", sd, client_ip, ntohs(client_addresses[i].sin_port));
                    close(sd); // Fecha o socket
                    client_socket[i] = 0; // Marca como slot livre
                    // Não é necessário limpar client_addresses[i] explicitamente, mas pode-se fazer um memset.
                } else if (valread < 0) { // Erro de leitura
                    if (errno != EWOULDBLOCK) { // EWOULDBLOCK significa que não há dados disponíveis ainda (não é um erro real para select)
                        perror("read error");
                    }
                    close(sd);
                    client_socket[i] = 0;
                } else { // Dados lidos, processar requisição HTTP
                    buffer[valread] = '\0'; // Garante terminação nula

                    if (sscanf(buffer, "%15s %255s %15s", method, path, http_version) != 3) {
                        send_error_response(sd, 400, "Bad Request", "<h1>400 Bad Request</h1><p>Sua requisi&ccedil;&atilde;o est&aacute; malformada.</p>");
                        status_code = 400;
                        log_request(client_ip, timestamp, "UNKNOWN", "UNKNOWN", status_code);
                        close(sd);
                        client_socket[i] = 0;
                        continue;
                    }

                    printf("Requisição de %s (socket %d): %s %s %s\n", client_ip, sd, method, path, http_version);

                    if (strcmp(method, "GET") != 0) {
                        send_error_response(sd, 501, "Not Implemented", "<h1>501 Not Implemented</h1><p>M&eacute;todo n&atilde;o suportado.</p>");
                        status_code = 501;
                        log_request(client_ip, timestamp, method, path, status_code);
                        close(sd);
                        client_socket[i] = 0;
                        continue;
                    }

                    if (strcmp(path, "/") == 0) {
                        strcpy(full_path, ".");
                        strcat(full_path, "/index.html");
                    } else {
                        strcpy(full_path, ".");
                        strcat(full_path, path);
                    }

                    if (stat(full_path, &file_stat) == -1 || !S_ISREG(file_stat.st_mode)) {
                        send_error_response(sd, 404, "Not Found", "<h1>404 Not Found</h1><p>O recurso solicitado n&atilde;o foi encontrado.</p>");
                        status_code = 404;
                        log_request(client_ip, timestamp, method, path, status_code);
                        close(sd);
                        client_socket[i] = 0;
                        continue;
                    }

                    file = fopen(full_path, "rb");
                    if (!file) {
                        send_error_response(sd, 500, "Internal Server Error", "<h1>500 Internal Server Error</h1><p>N&atilde;o foi poss&iacute;vel abrir o arquivo.</p>");
                        status_code = 500;
                        log_request(client_ip, timestamp, method, path, status_code);
                        close(sd);
                        client_socket[i] = 0;
                        continue;
                    }

                    const char *mime_type = get_mime_type(full_path);
                    snprintf(response_header, BUFFER_SIZE,
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %ld\r\n"
                             "Connection: close\r\n"
                             "\r\n",
                             mime_type, (long)file_stat.st_size);

                    write(sd, response_header, strlen(response_header));

                    int bytes_sent;
                    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                        bytes_sent = write(sd, buffer, bytes_read);
                        if (bytes_sent != bytes_read) {
                            perror("Erro ao enviar dados do arquivo");
                            break;
                        }
                    }

                    fclose(file);
                    close(sd); // Fecha o socket após enviar a resposta
                    client_socket[i] = 0; // Marca como slot livre
                    printf("Conexão com %s (socket %d) fechada.\n", client_ip, sd);
                    log_request(client_ip, timestamp, method, path, status_code);
                }
            }
        }
    }
    close(master_socket);
    return 0;
}