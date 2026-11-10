/*
 * banker.c - Algoritmo do banqueiro (Infraestrutura de Software, Implementacao 5).
 *
 * Uso: ./banker <r1> <r2> ...
 * Cada argumento e o numero de instancias de um tipo de recurso e forma o
 * vetor AVAILABLE inicial. O numero de recursos e definido por argc - 1.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* Estado do banqueiro. Os tamanhos sao definidos em tempo de execucao. */
typedef struct {
    int n_resources;
    int *available;
} Bank;

static void print_usage(void)
{
    fprintf(stderr, "Usage: ./banker <r1> <r2> ...\n");
}

/*
 * Converte str para um inteiro >= 0 que caiba em int.
 * Retorna 0 em caso de sucesso (valor em *out) e -1 se a string for invalida.
 */
static int parse_non_negative_int(const char *str, int *out)
{
    char *endptr;
    long value;

    if (str[0] == '\0') {
        return -1;
    }

    errno = 0;
    value = strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        return -1; /* sobrou texto que nao e numero (ex.: "5x", "abc") */
    }
    if (errno == ERANGE || value < 0 || value > INT_MAX) {
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

static void free_bank(Bank *bank)
{
    free(bank->available);
    bank->available = NULL;
    bank->n_resources = 0;
}

int main(int argc, char *argv[])
{
    Bank bank = {0, NULL};

    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    if (parse_resources(argc, argv, &bank) != 0) {
        free_bank(&bank);
        return EXIT_FAILURE;
    }

    /* TODO: ler e validar customer.txt (MAXIMUM, ALLOCATION e NEED). */
    /* TODO: ler e validar commands.txt (RQ, RL e *) antes de executar. */
    /* TODO: executar os comandos com o algoritmo do banqueiro. */
    /* TODO: escrever a saida em result.txt. */

    free_bank(&bank);
    return EXIT_SUCCESS;
}
