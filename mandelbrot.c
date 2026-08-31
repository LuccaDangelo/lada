/*
 * mandelbrot.c - Conjunto de Mandelbrot em quatro implementacoes.
 *
 * Bloco 2: pipeline completo com a versao serial (calculo, escrita
 * do arquivo e coleta de tempos).
 *
 * Uso: mandelbrot <largura> <altura> <max_iteracoes> <num_threads>
 */

#include <errno.h>
#include <limits.h>
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
 * Converte uma string em long, exigindo consumo total da string.
 * Retorna 0 em sucesso; -1 se a string for vazia/invalida/com lixo no final;
 * -2 se o valor estourar o intervalo representavel.
 */
static int parse_long(const char *s, long *out)
{
    char *end = NULL;
    long v;

    if (s == NULL || s[0] == '\0') {
        return -1;
    }

    errno = 0;
    v = strtol(s, &end, 10);

    if (errno == ERANGE) {
        return -2;
    }
    if (end == s || *end != '\0') {
        return -1;
    }

    *out = v;
    return 0;
}

/*
 * Le um argumento inteiro obrigatorio, validando faixa [min_val, max_val].
 * Imprime mensagem especifica em stderr e retorna != 0 em caso de erro.
 */
static int parse_arg_int(const char *s, const char *name, long min_val,
                         long max_val, int *out)
{
    long v = 0;
    int rc = parse_long(s, &v);

    if (rc == -2) {
        fprintf(stderr, "Erro: <%s> fora do intervalo representavel: \"%s\"\n",
                name, s);
        return 1;
    }
    if (rc != 0) {
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
    if (cfg == NULL) {
        fprintf(stderr, "Erro interno: configuracao nula\n");
        return 1;
    }

    if (argc != 5) {
        fprintf(stderr,
                "Uso: mandelbrot <largura> <altura> <max_iteracoes> <num_threads>\n");
        return 1;
    }

    if (parse_arg_int(argv[1], "largura", 1, MAX_DIM, &cfg->width) != 0) {
        return 1;
    }
    if (parse_arg_int(argv[2], "altura", 1, MAX_DIM, &cfg->height) != 0) {
        return 1;
    }
    if (parse_arg_int(argv[3], "max_iteracoes", 1, INT_MAX, &cfg->max_iter) != 0) {
        return 1;
    }
    if (parse_arg_int(argv[4], "num_threads", 1, MAX_THREADS,
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
/* Escrita rapida do arquivo PGM (apenas o raster, um valor por coluna) */
/* ------------------------------------------------------------------ */

/*
 * Tabela de conversao inteiro -> texto. Montada uma unica vez, evita
 * qualquer chamada de formatacao (sprintf/fprintf) durante a escrita.
 */
static char g_lut[256][4];
static uint8_t g_lut_len[256];
static int g_lut_ready = 0;

static void lut_init(void)
{
    int v;

    if (g_lut_ready) {
        return;
    }

    for (v = 0; v < 256; v++) {
        int n = 0;

        if (v >= 100) {
            g_lut[v][n++] = (char)('0' + v / 100);
        }
        if (v >= 10) {
            g_lut[v][n++] = (char)('0' + (v / 10) % 10);
        }
        g_lut[v][n++] = (char)('0' + v % 10);
        g_lut_len[v] = (uint8_t)n;
    }

    g_lut_ready = 1;
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
    char *iobuf = NULL;
    size_t cap;
    int y;

    if (filename == NULL || img == NULL || width <= 0 || height <= 0) {
        fprintf(stderr, "Erro interno: parametros invalidos em write_image\n");
        return 1;
    }

    lut_init();

    cap = (size_t)width * 4u + 2u;
    line = (char *)malloc(cap);
    if (line == NULL) {
        fprintf(stderr, "Erro: falha ao alocar buffer de linha (%zu bytes)\n",
                cap);
        return 1;
    }

    iobuf = (char *)malloc(IO_BUF_SIZE);
    if (iobuf == NULL) {
        fprintf(stderr, "Erro: falha ao alocar buffer de E/S (%d bytes)\n",
                IO_BUF_SIZE);
        free(line);
        return 1;
    }

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir \"%s\" para escrita: %s\n",
                filename, strerror(errno));
        free(iobuf);
        free(line);
        return 1;
    }

    if (setvbuf(fp, iobuf, _IOFBF, IO_BUF_SIZE) != 0) {
        fprintf(stderr, "Erro: falha ao configurar buffer de E/S para \"%s\"\n",
                filename);
        fclose(fp);
        free(iobuf);
        free(line);
        return 1;
    }

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
            fclose(fp);
            free(iobuf);
            free(line);
            return 1;
        }
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "Erro: falha ao fechar \"%s\": %s\n",
                filename, strerror(errno));
        free(iobuf);
        free(line);
        return 1;
    }

    free(iobuf);
    free(line);
    return 0;
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

typedef struct {
    const char *name;
    double seconds;
} Timing;

#define MAX_TIMINGS 4

static Timing g_timings[MAX_TIMINGS];
static int g_num_timings = 0;

static void record_time(const char *name, double seconds)
{
    if (g_num_timings >= MAX_TIMINGS) {
        return;
    }
    g_timings[g_num_timings].name = name;
    g_timings[g_num_timings].seconds = seconds;
    g_num_timings++;
}

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

    for (i = 0; i < g_num_timings; i++) {
        if (fprintf(fp, "%-12s%.6f\n", g_timings[i].name,
                    g_timings[i].seconds) < 0) {
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
 * Monta "mandelbrot_<LOGIN>_<sufixo>.pgm" em dst, detectando truncamento.
 * Retorna 0 em sucesso; != 0 em erro.
 */
static int build_filename(char *dst, size_t cap, const char *suffix)
{
    int n = snprintf(dst, cap, "mandelbrot_%s_%s.pgm", LOGIN, suffix);

    if (n < 0) {
        fprintf(stderr, "Erro: falha ao montar o nome do arquivo\n");
        return 1;
    }
    if ((size_t)n >= cap) {
        fprintf(stderr, "Erro: nome de arquivo truncado para a versao \"%s\"\n",
                suffix);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    Config cfg;
    unsigned char *img_ref = NULL;
    unsigned char *img_work = NULL;
    char filename[FILENAME_CAP];
    size_t npixels;
    double t0, t1;

    if (parse_args(argc, argv, &cfg) != 0) {
        return 1;
    }

    npixels = (size_t)cfg.width * (size_t)cfg.height;

    img_ref = (unsigned char *)malloc(npixels);
    if (img_ref == NULL) {
        fprintf(stderr, "Erro: falha ao alocar %zu bytes para a imagem de "
                        "referencia\n", npixels);
        return 1;
    }

    img_work = (unsigned char *)malloc(npixels);
    if (img_work == NULL) {
        fprintf(stderr, "Erro: falha ao alocar %zu bytes para a imagem de "
                        "trabalho\n", npixels);
        free(img_ref);
        return 1;
    }

    /* Serial: apenas o calculo entra na medicao; a escrita fica de fora. */
    t0 = now_seconds();
    run_serial(&cfg, img_ref);
    t1 = now_seconds();

    if (t0 < 0.0 || t1 < 0.0) {
        free(img_work);
        free(img_ref);
        return 1;
    }
    record_time("serial", t1 - t0);

    if (build_filename(filename, sizeof(filename), "serial") != 0) {
        free(img_work);
        free(img_ref);
        return 1;
    }
    if (write_image(filename, img_ref, cfg.width, cfg.height) != 0) {
        free(img_work);
        free(img_ref);
        return 1;
    }

    if (write_times("times.txt") != 0) {
        free(img_work);
        free(img_ref);
        return 1;
    }

    free(img_work);
    free(img_ref);
    return 0;
}
