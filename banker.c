/*
 * banker.c - Algoritmo do banqueiro (Infraestrutura de Software, Implementacao 5).
 *
 * Uso: ./banker <r1> <r2> ...
 * Cada argumento e o numero de instancias de um tipo de recurso e forma o
 * vetor AVAILABLE inicial. O numero de recursos e definido por argc - 1.
 * O MAXIMUM de cada cliente vem de customer.txt (uma linha por cliente).
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUSTOMER_FILE "customer.txt"
#define LINE_BUFFER_SIZE 4096

/* Estado do banqueiro. Os tamanhos sao definidos em tempo de execucao. */
typedef struct {
    int n_resources;
    int n_customers;
    int *available;   /* [n_resources] */
    int **maximum;    /* [n_customers][n_resources] */
    int **allocation; /* [n_customers][n_resources] */
    int **need;       /* [n_customers][n_resources] */
} Bank;

static void print_usage(void)
{
    fprintf(stderr, "Usage: ./banker <r1> <r2> ...\n");
}

/*
 * Converte str para um inteiro >= 0 que caiba em int.
 * Aceita apenas digitos decimais: rejeita string vazia, sinal, espacos e
 * qualquer texto extra.
 * Retorna 0 em caso de sucesso (valor em *out) e -1 se a string for invalida.
 */
static int parse_non_negative_int(const char *str, int *out)
{
    char *endptr;
    long value;

    /* strtol aceitaria espacos iniciais e sinal ("+5", " 5"); exige digito. */
    if (!isdigit((unsigned char) str[0])) {
        return -1;
    }

    errno = 0;
    value = strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        return -1; /* sobrou texto que nao e numero (ex.: "5x", "5 3") */
    }
    if (errno == ERANGE || value > INT_MAX) {
        return -1;
    }

    *out = (int) value;
    return 0;
}

/*
 * Valida os argumentos da linha de comando e preenche bank->available.
 * Retorna 0 em caso de sucesso e -1 em caso de erro (mensagem ja impressa).
 */
static int parse_resources(int argc, char *argv[], Bank *bank)
{
    int i;

    bank->n_resources = argc - 1;
    bank->available = malloc((size_t) bank->n_resources * sizeof(int));
    if (bank->available == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para os recursos\n");
        return -1;
    }

    for (i = 1; i < argc; i++) {
        if (parse_non_negative_int(argv[i], &bank->available[i - 1]) != 0) {
            fprintf(stderr,
                    "Erro: argumento %d (\"%s\") invalido: esperado inteiro >= 0 (ate %d)\n",
                    i, argv[i], INT_MAX);
            return -1;
        }
    }

    return 0;
}

/*
 * Le uma linha de fp em buf (tamanho LINE_BUFFER_SIZE), removendo o "\n" e
 * um "\r" final (arquivos salvos no Windows).
 * Retorna 1 se leu uma linha, 0 no fim do arquivo e -1 se a linha nao cabe
 * no buffer.
 */
static int read_line(FILE *fp, char *buf)
{
    size_t len;
    int c;

    if (fgets(buf, LINE_BUFFER_SIZE, fp) == NULL) {
        return 0;
    }

    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    } else {
        /* Sem "\n": ou e a ultima linha do arquivo, ou a linha e longa demais. */
        c = getc(fp);
        if (c != EOF) {
            ungetc(c, fp);
            return -1;
        }
    }

    if (len > 0 && buf[len - 1] == '\r') {
        buf[--len] = '\0';
    }

    return 1;
}

/*
 * Interpreta uma linha de customer.txt ("7,5,3") e guarda os valores em row
 * (n_resources posicoes). Altera line (troca as virgulas por '\0').
 * Retorna 0 em caso de sucesso e -1 em caso de erro (mensagem ja impressa).
 */
static int parse_customer_line(char *line, int line_no, int n_resources, int *row)
{
    char *field = line;
    char *comma;
    int n_columns = 0;
    int value;

    for (;;) {
        comma = strchr(field, ',');
        if (comma != NULL) {
            *comma = '\0';
        }
        n_columns++;

        if (field[0] == '\0') {
            fprintf(stderr, "Erro: %s, linha %d: campo %d vazio\n",
                    CUSTOMER_FILE, line_no, n_columns);
            return -1;
        }
        if (parse_non_negative_int(field, &value) != 0) {
            fprintf(stderr,
                    "Erro: %s, linha %d: campo %d (\"%s\") invalido: esperado inteiro >= 0 "
                    "(ate %d), separado por virgula e sem espacos\n",
                    CUSTOMER_FILE, line_no, n_columns, field, INT_MAX);
            return -1;
        }
        if (n_columns <= n_resources) {
            row[n_columns - 1] = value;
        }

        if (comma == NULL) {
            break;
        }
        field = comma + 1;
    }

    if (n_columns != n_resources) {
        fprintf(stderr,
                "Erro: %s, linha %d: esperados %d valores (um por recurso), encontrados %d\n",
                CUSTOMER_FILE, line_no, n_resources, n_columns);
        return -1;
    }

    return 0;
}

/*
 * Acrescenta uma copia de row ao final de bank->maximum, aumentando o vetor
 * de linhas quando necessario (*capacity guarda o tamanho alocado).
 * Retorna 0 em caso de sucesso e -1 se faltar memoria.
 */
static int append_customer(Bank *bank, int *capacity, const int *row)
{
    int **grown;
    int *copy;

    if (bank->n_customers == *capacity) {
        int new_capacity = (*capacity == 0) ? 8 : *capacity * 2;

        grown = realloc(bank->maximum, (size_t) new_capacity * sizeof(int *));
        if (grown == NULL) {
            return -1;
        }
        bank->maximum = grown;
        *capacity = new_capacity;
    }

    copy = malloc((size_t) bank->n_resources * sizeof(int));
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, row, (size_t) bank->n_resources * sizeof(int));

    bank->maximum[bank->n_customers] = copy;
    bank->n_customers++;
    return 0;
}

/*
 * Le e valida customer.txt do diretorio atual, preenchendo bank->maximum e
 * bank->n_customers. Linhas totalmente vazias so sao aceitas no fim do arquivo.
 * Retorna 0 em caso de sucesso e -1 em caso de erro (mensagem ja impressa).
 */
static int read_customers(Bank *bank)
{
    FILE *fp;
    char line[LINE_BUFFER_SIZE];
    int *row;
    int capacity = 0;
    int line_no = 0;
    int first_blank_line = 0; /* numero da primeira linha vazia (0 = nenhuma) */
    int status;
    int result = -1;

    fp = fopen(CUSTOMER_FILE, "r");
    if (fp == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir %s: %s\n",
                CUSTOMER_FILE, strerror(errno));
        return -1;
    }

    row = malloc((size_t) bank->n_resources * sizeof(int));
    if (row == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para os clientes\n");
        fclose(fp);
        return -1;
    }

    while ((status = read_line(fp, line)) != 0) {
        line_no++;

        if (status < 0) {
            fprintf(stderr, "Erro: %s, linha %d: linha maior que %d caracteres\n",
                    CUSTOMER_FILE, line_no, LINE_BUFFER_SIZE - 2);
            goto out;
        }

        if (line[0] == '\0') {
            if (first_blank_line == 0) {
                first_blank_line = line_no;
            }
            continue;
        }

        if (first_blank_line != 0) {
            fprintf(stderr,
                    "Erro: %s, linha %d: linha vazia no meio do arquivo "
                    "(so sao aceitas linhas vazias no fim)\n",
                    CUSTOMER_FILE, first_blank_line);
            goto out;
        }

        if (parse_customer_line(line, line_no, bank->n_resources, row) != 0) {
            goto out;
        }

        if (append_customer(bank, &capacity, row) != 0) {
            fprintf(stderr, "Erro: falha ao alocar memoria para os clientes\n");
            goto out;
        }
    }

    if (ferror(fp)) {
        fprintf(stderr, "Erro: falha ao ler %s\n", CUSTOMER_FILE);
        goto out;
    }

    if (bank->n_customers == 0) {
        fprintf(stderr, "Erro: %s esta vazio (nenhum cliente)\n", CUSTOMER_FILE);
        goto out;
    }

    result = 0;

out:
    free(row);
    fclose(fp);
    return result;
}

/*
 * Aloca ALLOCATION (zerada) e NEED (= MAXIMUM) para os clientes lidos.
 * Retorna 0 em caso de sucesso e -1 se faltar memoria.
 */
static int init_customer_state(Bank *bank)
{
    size_t row_size = (size_t) bank->n_resources * sizeof(int);
    int i;

    /* calloc: linhas ainda nao alocadas ficam NULL e free_bank pode liberar tudo. */
    bank->allocation = calloc((size_t) bank->n_customers, sizeof(int *));
    bank->need = calloc((size_t) bank->n_customers, sizeof(int *));
    if (bank->allocation == NULL || bank->need == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memoria para os clientes\n");
        return -1;
    }

    for (i = 0; i < bank->n_customers; i++) {
        bank->allocation[i] = calloc((size_t) bank->n_resources, sizeof(int));
        bank->need[i] = malloc(row_size);
        if (bank->allocation[i] == NULL || bank->need[i] == NULL) {
            fprintf(stderr, "Erro: falha ao alocar memoria para os clientes\n");
            return -1;
        }
        memcpy(bank->need[i], bank->maximum[i], row_size);
    }

    return 0;
}

static void free_matrix(int **matrix, int n_rows)
{
    int i;

    if (matrix == NULL) {
        return;
    }
    for (i = 0; i < n_rows; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

static void free_bank(Bank *bank)
{
    free_matrix(bank->maximum, bank->n_customers);
    free_matrix(bank->allocation, bank->n_customers);
    free_matrix(bank->need, bank->n_customers);
    free(bank->available);
    bank->maximum = NULL;
    bank->allocation = NULL;
    bank->need = NULL;
    bank->available = NULL;
    bank->n_customers = 0;
    bank->n_resources = 0;
}

int main(int argc, char *argv[])
{
    Bank bank = {0, 0, NULL, NULL, NULL, NULL};
    int exit_code = EXIT_FAILURE;

    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    if (parse_resources(argc, argv, &bank) != 0) {
        goto out;
    }

    if (read_customers(&bank) != 0) {
        goto out;
    }

    if (init_customer_state(&bank) != 0) {
        goto out;
    }

    /* TODO: ler e validar commands.txt (RQ, RL e *) antes de executar. */
    /* TODO: executar os comandos com o algoritmo do banqueiro. */
    /* TODO: escrever a saida em result.txt (so depois de tudo validado). */

    exit_code = EXIT_SUCCESS;

out:
    free_bank(&bank);
    return exit_code;
}
