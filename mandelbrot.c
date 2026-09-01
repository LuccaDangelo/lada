/*
 * mandelbrot.c - Conjunto de Mandelbrot em quatro implementacoes.
 *
 * Bloco 3: as quatro implementacoes (serial, OpenMP, pthreads por blocos
 * e pthreads com fila dinamica), todas chamando o mesmo compute_row().
 *
 * Uso: mandelbrot <largura> <altura> <max_iteracoes> <num_threads>
 */

#include <errno.h>
#include <limits.h>
#include <omp.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOGIN "lada"

/* Regiao do plano complexo. */
#define RE_MIN (-2.0)
#define IM_MIN (-1.5)
#define RE_LEN (3.0)
#define IM_LEN (3.0)

/* Limites superiores sãos para os argumentos. */
#define MAX_DIM 100000
#define MAX_THREADS 1024

/* Buffer de E/S de 1 MB para a escrita do arquivo. */
#define IO_BUF_SIZE (1024 * 1024)

/* Capacidade do nome de arquivo montado com snprintf. */
#define FILENAME_CAP 256

typedef struct {
    int width;
    int height;
    int max_iter;
    int num_threads;
} Config;

/*
 * Le um argumento inteiro obrigatorio: converte a string exigindo consumo
 * total e valida a faixa [min_val, max_val].
 * Imprime mensagem especifica em stderr e retorna != 0 em caso de erro.
 */
static int parse_arg(const char *s, const char *name, long min_val,
                     long max_val, int *out)
{
    char *end = NULL;
    long v;

    if (s == NULL || s[0] == '\0') {
        fprintf(stderr, "Erro: <%s> deve ser um numero inteiro valido, "
                        "recebido: \"\"\n", name);
        return 1;
    }

    errno = 0;
    v = strtol(s, &end, 10);

    if (errno == ERANGE) {
        fprintf(stderr, "Erro: <%s> fora do intervalo representavel: \"%s\"\n",
                name, s);
        return 1;
    }
    if (end == s || *end != '\0') {
        fprintf(stderr, "Erro: <%s> deve ser um numero inteiro valido, "
                        "recebido: \"%s\"\n", name, s);
        return 1;
    }
    if (v < min_val) {
        fprintf(stderr, "Erro: <%s> deve ser >= %ld, recebido: %ld\n",
                name, min_val, v);
        return 1;
    }
    if (v > max_val) {
        fprintf(stderr, "Erro: <%s> deve ser <= %ld, recebido: %ld\n",
                name, max_val, v);
        return 1;
    }

    *out = (int)v;
    return 0;
}

/*
 * Valida a linha de comando e preenche cfg.
 * Retorna 0 em sucesso; != 0 em erro (mensagem ja impressa em stderr).
 */
int parse_args(int argc, char **argv, Config *cfg)
{
    if (argc != 5) {
        fprintf(stderr,
                "Uso: mandelbrot <largura> <altura> <max_iteracoes> <num_threads>\n");
        return 1;
    }

    if (parse_arg(argv[1], "largura", 1, MAX_DIM, &cfg->width) != 0) {
        return 1;
    }
    if (parse_arg(argv[2], "altura", 1, MAX_DIM, &cfg->height) != 0) {
        return 1;
    }
    if (parse_arg(argv[3], "max_iteracoes", 1, INT_MAX, &cfg->max_iter) != 0) {
        return 1;
    }
    if (parse_arg(argv[4], "num_threads", 1, MAX_THREADS,
                  &cfg->num_threads) != 0) {
        return 1;
    }

    if ((size_t)cfg->width > SIZE_MAX / (size_t)cfg->height) {
        fprintf(stderr, "Erro: dimensoes muito grandes: %d x %d\n",
                cfg->width, cfg->height);
        return 1;
    }

    return 0;
}

/*
 * Numero de iteracoes de z = z^2 + c (z0 = 0) ate |z|^2 > 4, limitado a
 * max_iter. Sem sqrt: 3 multiplicacoes por iteracao.
 */
static inline int mandelbrot_point(double cr, double ci, int max_iter)
{
    double zr = 0.0, zi = 0.0;
    double zr2 = 0.0, zi2 = 0.0;
    int iter = 0;

    while (iter < max_iter && zr2 + zi2 <= 4.0) {
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
        zr2 = zr * zr;
        zi2 = zi * zi;
        iter++;
    }

    return iter;
}

/*
 * Mapeia a contagem de iteracoes para [0, 255] com aritmetica inteira,
 * garantindo resultado bit-a-bit deterministico.
 */
static inline unsigned char normalize(int iter, int max_iter)
{
    long v = ((long)iter * 255L) / (long)max_iter;
    return (unsigned char)v;
}

/*
 * Calcula a linha y inteira em img[y * width + x].
 *
 * Este e o UNICO ponto de calculo do programa: as quatro implementacoes
 * (serial, OpenMP, pthreads estatico e pthreads dinamico) chamam exatamente
 * esta funcao, o que garante saidas byte-a-byte identicas.
 */
void compute_row(const Config *cfg, unsigned char *img, int y)
{
    const int width = cfg->width;
    const double dr = RE_LEN / (double)width;
    const double ci = IM_MIN + (double)y * (IM_LEN / (double)cfg->height);
    unsigned char *row = img + (size_t)y * (size_t)width;
    int x;

    for (x = 0; x < width; x++) {
        double cr = RE_MIN + (double)x * dr;
        row[x] = normalize(mandelbrot_point(cr, ci, cfg->max_iter),
                           cfg->max_iter);
    }
}

/*
 * Implementacao serial: percorre todas as linhas em ordem, uma de cada vez.
 * E a referencia de corretude para as versoes paralelas.
 */
void run_serial(const Config *cfg, unsigned char *img)
{
    int y;

    for (y = 0; y < cfg->height; y++) {
        compute_row(cfg, img, y);
    }
}

/* ------------------------------------------------------------------ */
/* Implementacao 2: OpenMP                                              */
/* ------------------------------------------------------------------ */

/*
 * Paraleliza o laco de linhas com OpenMP.
 *
 * schedule(dynamic, 1) e nao static: o custo de uma linha do Mandelbrot e
 * altamente desbalanceado. Linhas que cortam o interior do conjunto rodam
 * max_iter iteracoes em cada pixel, enquanto linhas das bordas escapam em
 * poucas iteracoes — uma diferenca de ordens de magnitude. Com static as
 * threads que recebem as faixas baratas terminam cedo e ficam ociosas ate a
 * mais lenta acabar; com dynamic cada thread pega a proxima linha livre assim
 * que termina a anterior, e a carga se equilibra sozinha.
 *
 * Nenhuma sincronizacao e necessaria: cada iteracao escreve em posicoes
 * disjuntas de img (a linha y) e nao le nada escrito por outra thread.
 */
void run_openmp(const Config *cfg, unsigned char *img)
{
    int y;

#pragma omp parallel for schedule(dynamic, 1) num_threads(cfg->num_threads)
    for (y = 0; y < cfg->height; y++) {
        compute_row(cfg, img, y);
    }
}

/* ------------------------------------------------------------------ */
/* Implementacao 3: pthreads com divisao estatica por blocos de linhas  */
/* ------------------------------------------------------------------ */

typedef struct {
    const Config *cfg;
    unsigned char *img;
    int y_start;
    int y_end;
} BlockArg;

static void *block_worker(void *arg)
{
    BlockArg *a = (BlockArg *)arg;
    int y;

    for (y = a->y_start; y < a->y_end; y++) {
        compute_row(a->cfg, a->img, y);
    }

    return NULL;
}

/*
 * Divide as linhas em intervalos contiguos, um por thread.
 *
 * Esta e a estrategia que EXPOE o desbalanceamento do Mandelbrot: a thread que
 * ficar com as linhas centrais (que atravessam o interior do conjunto) demora
 * muito mais que as threads das bordas, e todas as outras ficam paradas no
 * join esperando por ela. O tempo total e ditado pela thread mais lenta.
 *
 * O resto da divisao e espalhado: as primeiras (height % nthreads) threads
 * recebem uma linha a mais, em vez de acumular tudo na ultima.
 *
 * Retorna 0 em sucesso; != 0 em erro (mensagem em stderr).
 */
int run_pthreads_block(const Config *cfg, unsigned char *img)
{
    pthread_t *tids = NULL;
    BlockArg *args = NULL;
    int nthreads = cfg->num_threads;
    int base, rest, created = 0;
    int i, y = 0, rc = 1;

    tids = (pthread_t *)malloc((size_t)nthreads * sizeof(*tids));
    args = (BlockArg *)malloc((size_t)nthreads * sizeof(*args));
    if (tids == NULL || args == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas para %d threads\n",
                nthreads);
        goto cleanup;
    }

    base = cfg->height / nthreads;
    rest = cfg->height % nthreads;

    for (i = 0; i < nthreads; i++) {
        int count = base + (i < rest ? 1 : 0);

        args[i].cfg = cfg;
        args[i].img = img;
        args[i].y_start = y;
        args[i].y_end = y + count;   /* intervalo vazio se nthreads > height */
        y += count;
    }

    for (i = 0; i < nthreads; i++) {
        int err = pthread_create(&tids[i], NULL, block_worker, &args[i]);

        if (err != 0) {
            fprintf(stderr, "Erro: falha ao criar a thread %d: %s\n",
                    i, strerror(err));
            break;
        }
        created++;
    }

    /* Junta TODAS as threads criadas, mesmo se a criacao falhou no meio. */
    rc = (created == nthreads) ? 0 : 1;
    for (i = 0; i < created; i++) {
        int err = pthread_join(tids[i], NULL);

        if (err != 0) {
            fprintf(stderr, "Erro: falha ao aguardar a thread %d: %s\n",
                    i, strerror(err));
            rc = 1;
        }
    }

cleanup:
    free(args);
    free(tids);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Implementacao 4: pthreads com fila dinamica de linhas                */
/* ------------------------------------------------------------------ */

typedef struct {
    const Config *cfg;
    unsigned char *img;
    int next_row;
    pthread_mutex_t mutex;
} QueueShared;

/*
 * A secao critica contem APENAS a leitura e o incremento do contador.
 * O compute_row fica deliberadamente fora do mutex: se o calculo acontecesse
 * com o lock na mao, as threads se revezariam uma de cada vez e o programa
 * seria uma execucao serial com o custo extra da contencao.
 */
static void *queue_worker(void *arg)
{
    QueueShared *sh = (QueueShared *)arg;
    const int height = sh->cfg->height;

    for (;;) {
        int y;

        pthread_mutex_lock(&sh->mutex);
        y = sh->next_row++;
        pthread_mutex_unlock(&sh->mutex);

        if (y >= height) {
            break;
        }

        compute_row(sh->cfg, sh->img, y);
    }

    return NULL;
}

/*
 * Fila dinamica: um contador compartilhado com a proxima linha a processar.
 * Cada thread pega uma linha, calcula, e volta para pegar a proxima.
 *
 * Esta estrategia balanceia a carga automaticamente: a thread que pegar uma
 * linha barata volta imediatamente para a fila e pega outra, em vez de ficar
 * ociosa como acontece na divisao estatica por blocos.
 *
 * O custo e a contencao no mutex, desprezivel aqui: a granularidade do
 * trabalho (uma linha inteira, milhares de pixels) e ordens de magnitude
 * maior que o custo de travar e destravar o lock uma vez por linha.
 *
 * Retorna 0 em sucesso; != 0 em erro (mensagem em stderr).
 */
int run_pthreads_queue(const Config *cfg, unsigned char *img)
{
    pthread_t *tids = NULL;
    QueueShared sh;
    int nthreads = cfg->num_threads;
    int created = 0;
    int i, err, rc = 1;

    sh.cfg = cfg;
    sh.img = img;
    sh.next_row = 0;

    err = pthread_mutex_init(&sh.mutex, NULL);
    if (err != 0) {
        fprintf(stderr, "Erro: falha ao inicializar o mutex: %s\n",
                strerror(err));
        return 1;
    }

    tids = (pthread_t *)malloc((size_t)nthreads * sizeof(*tids));
    if (tids == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas para %d threads\n",
                nthreads);
        goto cleanup;
    }

    for (i = 0; i < nthreads; i++) {
        err = pthread_create(&tids[i], NULL, queue_worker, &sh);

        if (err != 0) {
            fprintf(stderr, "Erro: falha ao criar a thread %d: %s\n",
                    i, strerror(err));
            break;
        }
        created++;
    }

    rc = (created == nthreads) ? 0 : 1;
    for (i = 0; i < created; i++) {
        err = pthread_join(tids[i], NULL);

        if (err != 0) {
            fprintf(stderr, "Erro: falha ao aguardar a thread %d: %s\n",
                    i, strerror(err));
            rc = 1;
        }
    }

cleanup:
    free(tids);
    pthread_mutex_destroy(&sh.mutex);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Escrita rapida do arquivo PGM (apenas o raster, um valor por coluna) */
/* ------------------------------------------------------------------ */

/*
 * Tabela de conversao inteiro -> texto. Montada uma unica vez por lut_init(),
 * evita qualquer chamada de formatacao durante a escrita.
 */
static char g_lut[256][4];
static uint8_t g_lut_len[256];

static void lut_init(void)
{
    int v;

    for (v = 0; v < 256; v++) {
        g_lut_len[v] = (uint8_t)snprintf(g_lut[v], sizeof(g_lut[v]), "%d", v);
    }
}

/*
 * Grava a imagem em filename: uma linha de texto por linha da imagem,
 * valores separados por um unico espaco, sem espaco no fim da linha.
 *
 * O buffer de linha e montado a mao e despejado com UM fwrite por linha;
 * com fprintf por pixel a escrita dominaria o tempo total do programa.
 *
 * Retorna 0 em sucesso; != 0 em erro (mensagem em stderr).
 */
int write_image(const char *filename, const unsigned char *img,
                int width, int height)
{
    FILE *fp = NULL;
    char *line = NULL;
    size_t cap = (size_t)width * 4u + 2u;
    int rc = 1;
    int y;

    line = (char *)malloc(cap);
    if (line == NULL) {
        fprintf(stderr, "Erro: falha ao alocar buffer de linha (%zu bytes)\n",
                cap);
        goto cleanup;
    }

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir \"%s\" para escrita: %s\n",
                filename, strerror(errno));
        goto cleanup;
    }

    setvbuf(fp, NULL, _IOFBF, IO_BUF_SIZE);

    for (y = 0; y < height; y++) {
        const unsigned char *row = img + (size_t)y * (size_t)width;
        char *p = line;
        int x;

        for (x = 0; x < width; x++) {
            unsigned char v = row[x];

            memcpy(p, g_lut[v], g_lut_len[v]);
            p += g_lut_len[v];
            if (x + 1 < width) {
                *p++ = ' ';
            }
        }
        *p++ = '\n';

        if (fwrite(line, 1, (size_t)(p - line), fp) != (size_t)(p - line)) {
            fprintf(stderr, "Erro: falha ao escrever a linha %d de \"%s\"\n",
                    y, filename);
            goto cleanup;
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Erro: falha ao fechar \"%s\": %s\n",
                filename, strerror(errno));
        fp = NULL;
        goto cleanup;
    }
    fp = NULL;
    rc = 0;

cleanup:
    if (fp != NULL) {
        fclose(fp);
    }
    free(line);
    return rc;
}

/*
 * Monta "mandelbrot_<LOGIN>_<sufixo>.pgm" e grava a imagem nele.
 * Retorna 0 em sucesso; != 0 em erro (mensagem em stderr).
 */
static int write_output(const Config *cfg, const unsigned char *img,
                        const char *suffix)
{
    char filename[FILENAME_CAP];
    int n = snprintf(filename, sizeof(filename), "mandelbrot_%s_%s.pgm",
                     LOGIN, suffix);

    if (n < 0) {
        fprintf(stderr, "Erro: falha ao montar o nome do arquivo\n");
        return 1;
    }
    if ((size_t)n >= sizeof(filename)) {
        fprintf(stderr, "Erro: nome de arquivo truncado para a versao \"%s\"\n",
                suffix);
        return 1;
    }

    return write_image(filename, img, cfg->width, cfg->height);
}

/* ------------------------------------------------------------------ */
/* Medicao de tempo                                                     */
/* ------------------------------------------------------------------ */

/*
 * Relogio de parede para medir o tempo de execucao do calculo.
 *
 * CLOCK_MONOTONIC e obrigatorio aqui: clock() devolveria tempo de CPU, que e
 * a SOMA do tempo gasto por todas as threads. Com ele, as versoes paralelas
 * apareceriam mais lentas que a serial e o relatorio de speedup ficaria
 * invertido.
 *
 * Retorna o instante atual em segundos; -1.0 em caso de falha.
 */
double now_seconds(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        fprintf(stderr, "Erro: falha ao ler o relogio monotonico: %s\n",
                strerror(errno));
        return -1.0;
    }

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ------------------------------------------------------------------ */
/* Coleta e escrita dos tempos                                          */
/* ------------------------------------------------------------------ */

enum { IMPL_SERIAL, IMPL_OPENMP, IMPL_PTHREADS1, IMPL_PTHREADS2, NUM_IMPLS };

static const char *IMPL_NAMES[NUM_IMPLS] = {
    "serial", "openmp", "pthreads1", "pthreads2"
};

static double g_times[NUM_IMPLS];

/* Todas as quatro implementacoes estao prontas. */
#define IMPLS_READY NUM_IMPLS

/*
 * Escreve times.txt: uma linha por implementacao, nome alinhado a esquerda
 * em 12 colunas e tempo com 6 casas decimais.
 */
static int write_times(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    int i;

    if (fp == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir \"%s\" para escrita: %s\n",
                filename, strerror(errno));
        return 1;
    }

    for (i = 0; i < IMPLS_READY; i++) {
        if (fprintf(fp, "%-12s%.6f\n", IMPL_NAMES[i], g_times[i]) < 0) {
            fprintf(stderr, "Erro: falha ao escrever em \"%s\"\n", filename);
            fclose(fp);
            return 1;
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Erro: falha ao fechar \"%s\": %s\n",
                filename, strerror(errno));
        return 1;
    }

    return 0;
}

/*
 * Roda uma implementacao paralela sobre img_work, mede apenas o calculo,
 * confere o resultado contra a referencia serial e grava o .pgm.
 *
 * Retorna 0 em sucesso; != 0 em erro (mensagem em stderr).
 */
static int run_and_check(const Config *cfg, int impl,
                         unsigned char *img_work,
                         const unsigned char *img_ref, size_t npixels)
{
    double t0, t1;
    int err = 0;

    memset(img_work, 0, npixels);

    t0 = now_seconds();
    switch (impl) {
    case IMPL_OPENMP:
        run_openmp(cfg, img_work);
        break;
    case IMPL_PTHREADS1:
        err = run_pthreads_block(cfg, img_work);
        break;
    case IMPL_PTHREADS2:
        err = run_pthreads_queue(cfg, img_work);
        break;
    default:
        fprintf(stderr, "Erro interno: implementacao desconhecida (%d)\n", impl);
        return 1;
    }
    t1 = now_seconds();

    if (err != 0) {
        fprintf(stderr, "Erro: a implementacao \"%s\" falhou\n",
                IMPL_NAMES[impl]);
        return 1;
    }
    if (t0 < 0.0 || t1 < 0.0) {
        return 1;
    }
    g_times[impl] = t1 - t0;

    /* Evidencia automatica de corretude: silencio total se tudo bater. */
    if (memcmp(img_work, img_ref, npixels) != 0) {
        fprintf(stderr, "AVISO: a implementacao \"%s\" divergiu da serial\n",
                IMPL_NAMES[impl]);
    }

    return write_output(cfg, img_work, IMPL_NAMES[impl]);
}

int main(int argc, char **argv)
{
    Config cfg;
    unsigned char *img_ref = NULL;
    unsigned char *img_work = NULL;
    size_t npixels;
    double t0, t1;
    int rc = 1;

    if (parse_args(argc, argv, &cfg) != 0) {
        return 1;
    }

    lut_init();

    npixels = (size_t)cfg.width * (size_t)cfg.height;

    img_ref = (unsigned char *)malloc(npixels);
    if (img_ref == NULL) {
        fprintf(stderr, "Erro: falha ao alocar %zu bytes para a imagem de "
                        "referencia\n", npixels);
        goto cleanup;
    }

    img_work = (unsigned char *)malloc(npixels);
    if (img_work == NULL) {
        fprintf(stderr, "Erro: falha ao alocar %zu bytes para a imagem de "
                        "trabalho\n", npixels);
        goto cleanup;
    }

    /* Serial: apenas o calculo entra na medicao; a escrita fica de fora. */
    t0 = now_seconds();
    run_serial(&cfg, img_ref);
    t1 = now_seconds();

    if (t0 < 0.0 || t1 < 0.0) {
        goto cleanup;
    }
    g_times[IMPL_SERIAL] = t1 - t0;

    if (write_output(&cfg, img_ref, IMPL_NAMES[IMPL_SERIAL]) != 0) {
        goto cleanup;
    }

    if (run_and_check(&cfg, IMPL_OPENMP, img_work, img_ref, npixels) != 0 ||
        run_and_check(&cfg, IMPL_PTHREADS1, img_work, img_ref, npixels) != 0 ||
        run_and_check(&cfg, IMPL_PTHREADS2, img_work, img_ref, npixels) != 0) {
        goto cleanup;
    }

    if (write_times("times.txt") != 0) {
        goto cleanup;
    }

    rc = 0;

cleanup:
    free(img_work);
    free(img_ref);
    return rc;
}
