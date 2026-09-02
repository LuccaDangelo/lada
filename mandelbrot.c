
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOGIN "lada"

#define RE_MIN (-2.0)
#define IM_MIN (-1.5)
#define RE_LEN (3.0)
#define IM_LEN (3.0)

#define MAX_DIM 100000
#define MAX_THREADS 1024

#define IO_BUF_SIZE (1024 * 1024)

#define FILENAME_CAP 256

enum {
    EXIT_ARGS   = 2,
    EXIT_ALLOC  = 3,
    EXIT_FILE   = 4,
    EXIT_THREAD = 5,
    EXIT_CLOCK  = 6
};

typedef struct {
    int width;
    int height;
    int max_iter;
    int num_threads;
} Config;

static int parse_arg(const char *s, const char *name, long min_val,
                     long max_val, int *out)
{
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);

    if (errno == ERANGE || end == s || *end != '\0') {
        fprintf(stderr, "Erro: <%s> deve ser um numero inteiro valido, "
                        "recebido: \"%s\"\n", name, s);
        return EXIT_ARGS;
    }
    if (v < min_val || v > max_val) {
        fprintf(stderr, "Erro: <%s> deve estar entre %ld e %ld, recebido: %ld\n",
                name, min_val, max_val, v);
        return EXIT_ARGS;
    }

    *out = (int)v;
    return 0;
}

int parse_args(int argc, char **argv, Config *cfg)
{
    if (argc != 5) {
        fprintf(stderr,
                "Uso: mandelbrot <largura> <altura> <max_iteracoes> <num_threads>\n");
        return EXIT_ARGS;
    }

    if (parse_arg(argv[1], "largura", 1, MAX_DIM, &cfg->width) != 0) {
        return EXIT_ARGS;
    }
    if (parse_arg(argv[2], "altura", 1, MAX_DIM, &cfg->height) != 0) {
        return EXIT_ARGS;
    }
    if (parse_arg(argv[3], "max_iteracoes", 1, INT_MAX, &cfg->max_iter) != 0) {
        return EXIT_ARGS;
    }
    if (parse_arg(argv[4], "num_threads", 1, MAX_THREADS,
                  &cfg->num_threads) != 0) {
        return EXIT_ARGS;
    }

    if ((size_t)cfg->width > SIZE_MAX / (size_t)cfg->height) {
        fprintf(stderr, "Erro: dimensoes muito grandes: %d x %d\n",
                cfg->width, cfg->height);
        return EXIT_ARGS;
    }

    return 0;
}

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

static inline unsigned char normalize(int iter, int max_iter)
{
    long v = ((long)iter * 255L) / (long)max_iter;
    return (unsigned char)v;
}

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

void run_serial(const Config *cfg, unsigned char *img)
{
    int y;

    for (y = 0; y < cfg->height; y++) {
        compute_row(cfg, img, y);
    }
}

void run_openmp(const Config *cfg, unsigned char *img)
{
    int y;

#pragma omp parallel for schedule(dynamic, 1) num_threads(cfg->num_threads)
    for (y = 0; y < cfg->height; y++) {
        compute_row(cfg, img, y);
    }
}

static int spawn_and_join(pthread_t *tids, int n, void *(*worker)(void *),
                          void *arg, size_t stride)
{
    int i, created = 0, rc;

    for (i = 0; i < n; i++) {
        int err = pthread_create(&tids[i], NULL, worker,
                                 (char *)arg + (size_t)i * stride);

        if (err != 0) {
            fprintf(stderr, "Erro: falha ao criar a thread %d: %s\n",
                    i, strerror(err));
            break;
        }
        created++;
    }

    rc = (created == n) ? 0 : EXIT_THREAD;
    for (i = 0; i < created; i++) {
        int err = pthread_join(tids[i], NULL);

        if (err != 0) {
            fprintf(stderr, "Erro: falha ao aguardar a thread %d: %s\n",
                    i, strerror(err));
            rc = EXIT_THREAD;
        }
    }

    return rc;
}

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

int run_pthreads_block(const Config *cfg, unsigned char *img)
{
    pthread_t *tids = NULL;
    BlockArg *args = NULL;
    int nthreads = cfg->num_threads;
    int base, rest;
    int i, y = 0, rc = EXIT_THREAD;

    tids = (pthread_t *)malloc((size_t)nthreads * sizeof(*tids));
    args = (BlockArg *)malloc((size_t)nthreads * sizeof(*args));
    if (tids == NULL || args == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas para %d threads\n",
                nthreads);
        rc = EXIT_ALLOC;
        goto cleanup;
    }

    base = cfg->height / nthreads;
    rest = cfg->height % nthreads;

    for (i = 0; i < nthreads; i++) {
        int count = base + (i < rest ? 1 : 0);

        args[i].cfg = cfg;
        args[i].img = img;
        args[i].y_start = y;
        args[i].y_end = y + count;
        y += count;
    }

    rc = spawn_and_join(tids, nthreads, block_worker, args, sizeof(*args));

cleanup:
    free(args);
    free(tids);
    return rc;
}

typedef struct {
    const Config *cfg;
    unsigned char *img;
    int next_row;
    pthread_mutex_t mutex;
} QueueShared;

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

int run_pthreads_queue(const Config *cfg, unsigned char *img)
{
    pthread_t *tids = NULL;
    QueueShared sh;
    int nthreads = cfg->num_threads;
    int err, rc = EXIT_THREAD;

    sh.cfg = cfg;
    sh.img = img;
    sh.next_row = 0;

    err = pthread_mutex_init(&sh.mutex, NULL);
    if (err != 0) {
        fprintf(stderr, "Erro: falha ao inicializar o mutex: %s\n",
                strerror(err));
        return EXIT_THREAD;
    }

    tids = (pthread_t *)malloc((size_t)nthreads * sizeof(*tids));
    if (tids == NULL) {
        fprintf(stderr, "Erro: falha ao alocar estruturas para %d threads\n",
                nthreads);
        rc = EXIT_ALLOC;
        goto cleanup;
    }

    rc = spawn_and_join(tids, nthreads, queue_worker, &sh, 0);

cleanup:
    free(tids);
    pthread_mutex_destroy(&sh.mutex);
    return rc;
}

static char g_lut[256][4];
static uint8_t g_lut_len[256];

static void lut_init(void)
{
    int v;

    for (v = 0; v < 256; v++) {
        g_lut_len[v] = (uint8_t)snprintf(g_lut[v], sizeof(g_lut[v]), "%d", v);
    }
}

int write_image(const char *filename, const unsigned char *img,
                int width, int height)
{
    FILE *fp = NULL;
    char *line = NULL;
    size_t cap = (size_t)width * 4u + 2u;
    int rc = EXIT_FILE;
    int y;

    line = (char *)malloc(cap);
    if (line == NULL) {
        fprintf(stderr, "Erro: falha ao alocar buffer de linha (%zu bytes)\n",
                cap);
        rc = EXIT_ALLOC;
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

static int write_output(const Config *cfg, const unsigned char *img,
                        const char *suffix)
{
    char filename[FILENAME_CAP];

    snprintf(filename, sizeof(filename), "mandelbrot_%s_%s.pgm", LOGIN, suffix);

    return write_image(filename, img, cfg->width, cfg->height);
}

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

enum { IMPL_SERIAL, IMPL_OPENMP, IMPL_PTHREADS1, IMPL_PTHREADS2, NUM_IMPLS };

static const char *IMPL_NAMES[NUM_IMPLS] = {
    "serial", "openmp", "pthreads1", "pthreads2"
};

static const char *IMPL_LABELS[NUM_IMPLS] = {
    "Serial", "OpenMP", "Pthreads1", "Pthreads2"
};

static double g_times[NUM_IMPLS];

static int write_times(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    int i;

    if (fp == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir \"%s\" para escrita: %s\n",
                filename, strerror(errno));
        return EXIT_FILE;
    }

    for (i = 0; i < NUM_IMPLS; i++) {
        if (fprintf(fp, "%s: %.6fs\n", IMPL_LABELS[i], g_times[i]) < 0) {
            fprintf(stderr, "Erro: falha ao escrever em \"%s\"\n", filename);
            fclose(fp);
            return EXIT_FILE;
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Erro: falha ao fechar \"%s\": %s\n",
                filename, strerror(errno));
        return EXIT_FILE;
    }

    return 0;
}

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
        return EXIT_THREAD;
    }
    t1 = now_seconds();

    if (err != 0) {
        fprintf(stderr, "Erro: a implementacao \"%s\" falhou\n",
                IMPL_NAMES[impl]);
        return err;
    }
    if (t0 < 0.0 || t1 < 0.0) {
        return EXIT_CLOCK;
    }
    g_times[impl] = t1 - t0;

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
    int rc;

    rc = parse_args(argc, argv, &cfg);
    if (rc != 0) {
        return rc;
    }

    lut_init();

    npixels = (size_t)cfg.width * (size_t)cfg.height;

    rc = EXIT_ALLOC;
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

    t0 = now_seconds();
    run_serial(&cfg, img_ref);
    t1 = now_seconds();

    if (t0 < 0.0 || t1 < 0.0) {
        rc = EXIT_CLOCK;
        goto cleanup;
    }
    g_times[IMPL_SERIAL] = t1 - t0;

    rc = write_output(&cfg, img_ref, IMPL_NAMES[IMPL_SERIAL]);
    if (rc != 0) {
        goto cleanup;
    }

    rc = run_and_check(&cfg, IMPL_OPENMP, img_work, img_ref, npixels);
    if (rc != 0) {
        goto cleanup;
    }
    rc = run_and_check(&cfg, IMPL_PTHREADS1, img_work, img_ref, npixels);
    if (rc != 0) {
        goto cleanup;
    }
    rc = run_and_check(&cfg, IMPL_PTHREADS2, img_work, img_ref, npixels);
    if (rc != 0) {
        goto cleanup;
    }

    rc = write_times("times.txt");

cleanup:
    free(img_work);
    free(img_ref);
    return rc;
}
