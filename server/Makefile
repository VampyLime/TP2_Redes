# Variáveis de compilação
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g
LDFLAGS =

# Arquivos fonte comuns para os servidores
HTTP_UTILS_SRC = http_utils.c
HTTP_UTILS_OBJ = $(HTTP_UTILS_SRC:.c=.o)

# Executáveis dos servidores
SERVER_EXES = iterativo fork thread select


# Todos os executáveis que podem ser construídos
ALL_EXES = $(SERVER_EXES)

.PHONY: all clean

#Constrói todos os executáveis
all: $(ALL_EXES)

# Regras para os executáveis dos servidores
# Cada executável de servidor depende do seu próprio .o e do http_utils.o
$(SERVER_EXES): %: %.o $(HTTP_UTILS_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

# Regra especial para o servidor 'thread' porque ele precisa da biblioteca pthreads
thread: thread.o $(HTTP_UTILS_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ -pthread

# Regra genérica para compilar arquivos .c em .o
# Automaticamente compila SRC.c para SRC.o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpa os arquivos gerados (objetos e executáveis)
clean:
	rm -f $(ALL_EXES) $(SERVER_EXES:=.o) $(HTTP_UTILS_OBJ)
	rm -f *.o
	rm -f arquivo_recebido.txt arquivo_recebido_cliente_http.bin