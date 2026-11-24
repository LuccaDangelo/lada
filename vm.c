/*
 * vm.c - Gerenciador de memória virtual (Implementação 6 - Infraestrutura de Software)
 *
 * Baseado em "Designing a Virtual Memory Manager" (Silberschatz, 10ª ed.).
 *
 * Uso: ./vm <arquivo_enderecos> <fifo|lru> <fifo|lru>
 *   2º argumento: algoritmo de substituição de página
 *   3º argumento: algoritmo de substituição da TLB
 *
 * Tradução de endereços com substituição de página (FIFO ou LRU) e TLB de 16
 * entradas com substituição FIFO ou LRU. A busca na TLB usa uma thread por entrada.
 *
 * O número de frames é definido na compilação (padrão 128):
 *   make              -> 128 frames
 *   make FRAMES=256   -> 256 frames
 *
 * A TLB pode ser desligada na compilação (fluxo sem TLB: coluna "TLB" = -1, 0 hits):
 *   make              -> com TLB (padrão)
 *   make TLB=0        -> sem TLB
 *
 * A busca na TLB pode ser feita sem threads (busca sequencial), só com TLB=1:
 *   make              -> uma thread por entrada da TLB (padrão)
 *   make THREADS=0    -> busca sequencial
 */

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKING_STORE_FILE "BACKING_STORE.bin"
#define OUTPUT_FILE        "correct.txt"

#define PAGE_SIZE          256
#define NUM_PAGES          256
#ifndef NUM_FRAMES
#define NUM_FRAMES         128
#endif
#define TLB_SIZE           16
#define MAX_LOGICAL_ADDR   65535
#ifndef USE_TLB
#define USE_TLB            1
#endif
#ifndef USE_THREADS
#define USE_THREADS        1
#endif

#if NUM_FRAMES < 1
#error "NUM_FRAMES deve ser maior que zero"
#endif

typedef enum { ALG_FIFO, ALG_LRU } algorithm_t;

/* Tabela de páginas: frame onde a página está carregada, ou -1. */
static int page_table[NUM_PAGES];
/* Página carregada em cada frame (para invalidar a entrada na substituição), ou -1. */
static int frame_to_page[NUM_FRAMES];
static signed char physical_memory[NUM_FRAMES][PAGE_SIZE];

/* Próximo frame livre (frames são preenchidos de 0 a NUM_FRAMES-1, em ordem). */
static int next_free_frame = 0;

/*
 * FIFO: fila circular com a ordem de carregamento dos frames. Como os frames são
 * preenchidos em ordem e o frame substituído recebe a página nova (indo para o fim
 * da fila), a fila é sempre 0,1,...,N-1 rotacionada; basta guardar a cabeça.
 */
static int fifo_queue[NUM_FRAMES];
static int fifo_head = 0;
static int fifo_count = 0;

/* LRU: contador global de acessos; last_used[f] = instante do último acesso ao frame f. */
static unsigned long access_clock = 0;
static unsigned long last_used[NUM_FRAMES];

#if USE_TLB
/* TLB: cache de traduções página -> frame. */
typedef struct {
    int page;
    int frame;
    int valid;
    unsigned long last_used; /* instante do último uso da entrada (TLB LRU) */
} tlb_entry_t;

static tlb_entry_t tlb[TLB_SIZE];
static unsigned long tlb_clock = 0;
/* TLB FIFO: ponteiro circular para a próxima entrada a ser sobrescrita. */
static int tlb_fifo_next = 0;
#endif

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

/* Registra um acesso ao frame (para LRU). Chamado em TODO acesso, não só no page fault. */
static void touch_frame(int frame) {
    last_used[frame] = ++access_clock;
}

static void fifo_push(int frame) {
    fifo_queue[(fifo_head + fifo_count) % NUM_FRAMES] = frame;
    fifo_count++;
}

static int fifo_pop(void) {
    int frame = fifo_queue[fifo_head];
    fifo_head = (fifo_head + 1) % NUM_FRAMES;
    fifo_count--;
    return frame;
}

static int lru_victim(void) {
    int victim = 0;
    for (int f = 1; f < NUM_FRAMES; f++)
        if (last_used[f] < last_used[victim])
            victim = f;
    return victim;
}

#if USE_TLB
#if !USE_THREADS
/* Busca sequencial na TLB. Retorna o índice da entrada válida da página, ou -1. */
static int tlb_lookup(int page) {
    for (int i = 0; i < TLB_SIZE; i++)
        if (tlb[i].valid && tlb[i].page == page)
            return i;
    return -1;
}
#endif

#if USE_THREADS
/* Argumento de cada thread da busca: compara só a entrada "index" da TLB. */
typedef struct {
    int index;
    int page;
    int *found; /* array local de tlb_lookup_threaded; a thread escreve só em found[index] */
} tlb_search_arg_t;

static void *tlb_search_entry(void *arg) {
    tlb_search_arg_t *a = arg;
    a->found[a->index] = (tlb[a->index].valid && tlb[a->index].page == a->page);
    return NULL;
}

/*
 * Busca na TLB com uma thread por entrada (TLB_SIZE threads). Ninguém escreve na TLB
 * durante a busca e cada thread grava só a sua posição de found[], então não há lock.
 * Retorna o índice encontrado, -1 se não encontrado, ou -2 em erro (já reportado).
 */
static int tlb_lookup_threaded(int page) {
    pthread_t threads[TLB_SIZE];
    tlb_search_arg_t args[TLB_SIZE];
    int found[TLB_SIZE] = {0};
    int created = 0;
    int error = 0;
    int rc;

    for (int i = 0; i < TLB_SIZE; i++) {
        args[i].index = i;
        args[i].page = page;
        args[i].found = found;
        rc = pthread_create(&threads[i], NULL, tlb_search_entry, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "Erro: pthread_create falhou ao criar a thread da entrada %d "
                    "da TLB: %s\n", i, strerror(rc));
            error = 1;
            break;
        }
        created++;
    }

    /* Espera todas as threads criadas, mesmo em caso de erro na criação. */
    for (int i = 0; i < created; i++) {
        rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "Erro: pthread_join falhou na thread da entrada %d da TLB: %s\n",
                    i, strerror(rc));
            error = 1;
        }
    }

    if (error)
        return -2;

    for (int i = 0; i < TLB_SIZE; i++)
        if (found[i])
            return i;
    return -1;
}
#endif

/* Insere {page, frame} na TLB conforme o algoritmo. Retorna o índice usado. */
static int tlb_insert(int page, int frame, algorithm_t tlb_alg) {
    int idx;

    if (tlb_alg == ALG_FIFO) {
        /* Avança sempre, mesmo que exista entrada inválida. */
        idx = tlb_fifo_next;
        tlb_fifo_next = (tlb_fifo_next + 1) % TLB_SIZE;
    } else {
        idx = -1;
        for (int i = 0; i < TLB_SIZE; i++) {
            if (!tlb[i].valid) { idx = i; break; }
        }
        if (idx < 0) {
            idx = 0;
            for (int i = 1; i < TLB_SIZE; i++)
                if (tlb[i].last_used < tlb[idx].last_used)
                    idx = i;
        }
    }

    tlb[idx].page = page;
    tlb[idx].frame = frame;
    tlb[idx].valid = 1;
    tlb[idx].last_used = ++tlb_clock;
    return idx;
}

/* Invalida qualquer entrada da TLB que aponte para a página (que saiu da memória). */
static void tlb_invalidate_page(int page) {
    for (int i = 0; i < TLB_SIZE; i++)
        if (tlb[i].valid && tlb[i].page == page)
            tlb[i].valid = 0;
}
#endif

/*
 * Trata um page fault: escolhe o frame (livre ou vítima), invalida a página antiga
 * e carrega a página do backing store. Retorna o frame, ou -1 em erro (já reportado).
 */
static int handle_page_fault(int page, algorithm_t page_alg, FILE *backing) {
    int frame;

    if (next_free_frame < NUM_FRAMES) {
        frame = next_free_frame++;
    } else {
        frame = (page_alg == ALG_FIFO) ? fifo_pop() : lru_victim();
        int old_page = frame_to_page[frame];
        if (old_page >= 0) {
            page_table[old_page] = -1;
#if USE_TLB
            tlb_invalidate_page(old_page);
#endif
        }
    }

    if (fseek(backing, (long)page * PAGE_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Erro: falha ao posicionar em '%s' para a página %d: %s\n",
                BACKING_STORE_FILE, page, strerror(errno));
        return -1;
    }
    size_t got = fread(physical_memory[frame], 1, PAGE_SIZE, backing);
    if (got != PAGE_SIZE) {
        if (ferror(backing))
            fprintf(stderr, "Erro: falha ao ler a página %d de '%s': %s\n",
                    page, BACKING_STORE_FILE, strerror(errno ? errno : EIO));
        else
            fprintf(stderr, "Erro: '%s' truncado: página %d tem só %zu de %d bytes\n",
                    BACKING_STORE_FILE, page, got, PAGE_SIZE);
        return -1;
    }

    page_table[page] = frame;
    frame_to_page[frame] = page;
    if (page_alg == ALG_FIFO)
        fifo_push(frame);
    return frame;
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

#if !USE_TLB
    (void)tlb_alg; /* validado, mas a TLB foi desligada na compilação (TLB=0) */
#endif

    FILE *out = fopen(OUTPUT_FILE, "w");
    if (out == NULL) {
        fprintf(stderr, "Erro: não foi possível criar '%s': %s\n",
                OUTPUT_FILE, strerror(errno));
        fclose(backing);
        free(addrs);
        return EXIT_FAILURE;
    }

    for (int p = 0; p < NUM_PAGES; p++)
        page_table[p] = -1;
    for (int f = 0; f < NUM_FRAMES; f++)
        frame_to_page[f] = -1;

    int page_faults = 0;
    int tlb_hits = 0;
    int status = EXIT_SUCCESS;

    for (size_t i = 0; i < count; i++) {
        unsigned int addr = addrs[i];
        int page = (int)((addr >> 8) & 0xFF);
        int offset = (int)(addr & 0xFF);

        int frame;
        int tlb_index = -1;

#if USE_TLB
#if USE_THREADS
        tlb_index = tlb_lookup_threaded(page);
        if (tlb_index == -2) {
            status = EXIT_FAILURE;
            break;
        }
#else
        tlb_index = tlb_lookup(page);
#endif
        if (tlb_index >= 0) {
            /* TLB hit */
            tlb_hits++;
            frame = tlb[tlb_index].frame;
            if (tlb_alg == ALG_LRU)
                tlb[tlb_index].last_used = ++tlb_clock;
        } else
#endif
        {
            /* TLB miss (ou TLB desligada): consulta a tabela de páginas. */
            frame = page_table[page];
            if (frame < 0) {
                page_faults++;
                frame = handle_page_fault(page, page_alg, backing);
                if (frame < 0) {
                    status = EXIT_FAILURE;
                    break;
                }
            }
#if USE_TLB
            tlb_index = tlb_insert(page, frame, tlb_alg);
#endif
        }
        touch_frame(frame); /* LRU de página conta todo acesso */

        int physical = frame * PAGE_SIZE + offset;
        int value = physical_memory[frame][offset];

        if (fprintf(out, "Virtual address: %u TLB: %d Physical address: %d Value: %d\n",
                    addr, tlb_index, physical, value) < 0) {
            fprintf(stderr, "Erro ao escrever em '%s': %s\n", OUTPUT_FILE, strerror(errno));
            status = EXIT_FAILURE;
            break;
        }
    }

    if (status == EXIT_SUCCESS) {
        if (fprintf(out,
                    "Number of Translated Addresses = %zu\n"
                    "Page Faults = %d\n"
                    "Page Fault Rate = %.3f\n"
                    "TLB Hits = %d\n"
                    "TLB Hit Rate = %.3f\n",
                    count, page_faults, (double)page_faults / (double)count,
                    tlb_hits, (double)tlb_hits / (double)count) < 0) {
            fprintf(stderr, "Erro ao escrever em '%s': %s\n", OUTPUT_FILE, strerror(errno));
            status = EXIT_FAILURE;
        }
    }

    if (fclose(out) != 0 && status == EXIT_SUCCESS) {
        fprintf(stderr, "Erro ao fechar '%s': %s\n", OUTPUT_FILE, strerror(errno));
        status = EXIT_FAILURE;
    }

    if (status == EXIT_SUCCESS)
        printf("%zu endereços traduzidos (%d frames, página: %s, TLB: %s%s): "
               "%d page faults, %d TLB hits -> %s\n",
               count, NUM_FRAMES, argv[2], USE_TLB ? argv[3] : "desligada",
               USE_TLB ? (USE_THREADS ? ", busca com threads" : ", busca sequencial") : "",
               page_faults, tlb_hits, OUTPUT_FILE);

    fclose(backing);
    free(addrs);
    return status;
}
