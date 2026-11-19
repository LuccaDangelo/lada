/*
 * vm.c - Gerenciador de memória virtual (Implementação 6 - Infraestrutura de Software)
 *
 * Baseado em "Designing a Virtual Memory Manager" (Silberschatz, 10ª ed.).
 *
 * Uso: ./vm <arquivo_enderecos> <fifo|lru> <fifo|lru>
 *   2º argumento: algoritmo de substituição de página
 *   3º argumento: algoritmo de substituição da TLB
 *
 * Etapa atual: apenas estrutura e tratamento de erros (sem tradução).
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKING_STORE_FILE "BACKING_STORE.bin"
#define OUTPUT_FILE        "correct.txt"

#define PAGE_SIZE          256
#define NUM_PAGES          256
#define NUM_FRAMES         256
#define TLB_SIZE           16
#define MAX_LOGICAL_ADDR   65535

typedef enum { ALG_FIFO, ALG_LRU } algorithm_t;

static void print_usage(const char *prog) {
    fprintf(stderr, "Uso: %s <arquivo_enderecos> <fifo|lru> <fifo|lru>\n", prog);
    fprintf(stderr, "  2º argumento: algoritmo de substituição de página (fifo ou lru)\n");
    fprintf(stderr, "  3º argumento: algoritmo de substituição da TLB (fifo ou lru)\n");
}

/* Converte "fifo"/"lru" (somente minúsculas). Retorna 0 em sucesso, -1 caso contrário. */
static int parse_algorithm(const char *s, algorithm_t *out) {
    if (strcmp(s, "fifo") == 0) { *out = ALG_FIFO; return 0; }
    if (strcmp(s, "lru") == 0)  { *out = ALG_LRU;  return 0; }
    return -1;
}

/*
 * Lê e valida o arquivo de endereços inteiro antes de qualquer processamento.
 * Em sucesso, retorna 0 e preenche *addrs (alocado com malloc) e *count.
 * Em erro, imprime a mensagem em stderr e retorna -1.
 */
static int load_addresses(const char *path, unsigned int **addrs, size_t *count) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir o arquivo de endereços '%s': %s\n",
                path, strerror(errno));
        return -1;
    }

    unsigned int *list = NULL;
    size_t n = 0, cap = 0;
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t len;
    unsigned long line_no = 0;
    int status = 0;

    errno = 0;
    while ((len = getline(&line, &line_cap, fp)) != -1) {
        line_no++;

        /* Remove quebra de linha e espaços em branco no fim da linha. */
        while (len > 0 && isspace((unsigned char)line[len - 1]))
            line[--len] = '\0';

        if (len == 0)
            continue; /* linha vazia: ignorada */

        /* Apenas dígitos decimais são aceitos. */
        unsigned long value = 0;
        int too_big = 0;
        for (ssize_t i = 0; i < len; i++) {
            if (!isdigit((unsigned char)line[i])) {
                fprintf(stderr,
                        "Erro de formato em '%s', linha %lu: \"%s\" não é um número inteiro "
                        "não negativo\n", path, line_no, line);
                status = -1;
                goto done;
            }
            if (!too_big) {
                value = value * 10 + (unsigned long)(line[i] - '0');
                if (value > MAX_LOGICAL_ADDR)
                    too_big = 1;
            }
        }
        if (too_big) {
            fprintf(stderr,
                    "Erro de formato em '%s', linha %lu: endereço %s fora do intervalo "
                    "0..%d\n", path, line_no, line, MAX_LOGICAL_ADDR);
            status = -1;
            goto done;
        }

        if (n == cap) {
            size_t new_cap = cap ? cap * 2 : 1024;
            unsigned int *tmp = realloc(list, new_cap * sizeof *tmp);
            if (tmp == NULL) {
                fprintf(stderr, "Erro: memória insuficiente ao ler '%s'\n", path);
                status = -1;
                goto done;
            }
            list = tmp;
            cap = new_cap;
        }
        list[n++] = (unsigned int)value;
        errno = 0;
    }

    if (ferror(fp)) {
        fprintf(stderr, "Erro ao ler o arquivo de endereços '%s': %s\n",
                path, strerror(errno ? errno : EIO));
        status = -1;
        goto done;
    }

    if (n == 0) {
        fprintf(stderr, "Erro de formato em '%s': o arquivo não contém nenhum endereço\n", path);
        status = -1;
        goto done;
    }

done:
    free(line);
    fclose(fp);
    if (status != 0) {
        free(list);
        return -1;
    }
    *addrs = list;
    *count = n;
    return 0;
}

int main(int argc, char *argv[]) {
    algorithm_t page_alg, tlb_alg;

    if (argc != 4) {
        fprintf(stderr, "Erro: número de argumentos inválido (esperado 3, recebido %d)\n",
                argc - 1);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_algorithm(argv[2], &page_alg) != 0) {
        fprintf(stderr,
                "Erro: algoritmo de substituição de página inválido: '%s' "
                "(use \"fifo\" ou \"lru\", em minúsculas)\n", argv[2]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_algorithm(argv[3], &tlb_alg) != 0) {
        fprintf(stderr,
                "Erro: algoritmo de substituição da TLB inválido: '%s' "
                "(use \"fifo\" ou \"lru\", em minúsculas)\n", argv[3]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    unsigned int *addrs = NULL;
    size_t count = 0;
    if (load_addresses(argv[1], &addrs, &count) != 0)
        return EXIT_FAILURE;

    FILE *backing = fopen(BACKING_STORE_FILE, "rb");
    if (backing == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir '%s': %s\n",
                BACKING_STORE_FILE, strerror(errno));
        free(addrs);
        return EXIT_FAILURE;
    }

    (void)page_alg;
    (void)tlb_alg;

    /* TODO: tradução de endereços (próxima etapa). */
    printf("%zu endereços lidos de '%s'\n", count, argv[1]);

    fclose(backing);
    free(addrs);
    return EXIT_SUCCESS;
}
