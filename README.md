# Mandelbrot — serial, OpenMP e pthreads

Gera o conjunto de Mandelbrot na regiao `[-2, 1] x [-1.5, 1.5]` do plano complexo
em quatro implementacoes e mede o tempo de calculo de cada uma:

| Implementacao | Estrategia |
| --- | --- |
| `serial` | laco unico sobre todas as linhas; referencia de corretude |
| `openmp` | `#pragma omp parallel for schedule(dynamic, 1)` sobre as linhas |
| `pthreads1` | pthreads com divisao estatica: blocos contiguos de linhas por thread |
| `pthreads2` | pthreads com fila dinamica: contador compartilhado protegido por mutex |

As quatro chamam exatamente a mesma funcao `compute_row()`, o que garante saidas
byte-a-byte identicas.

## Requisitos

- Compilador C com suporte a **OpenMP** e **pthreads** (C11).
- No macOS, o `clang` do sistema **nao** aceita `-fopenmp`. Instale o GCC:
  ```
  brew install gcc
  ```
  O `Makefile` detecta sozinho o primeiro `gcc-<versao>` do Homebrew
  (`/opt/homebrew/bin` ou `/usr/local/bin`) e cai para `gcc` se nao achar nenhum.
- No Linux, o `gcc` padrao ja atende.

## Compilar

```
make clean
make
```

## Executar

```
./mandelbrot <largura> <altura> <max_iteracoes> <num_threads>
```

Exemplo:

```
./mandelbrot 800 800 1000 4
```

Atalho equivalente: `make run`.

Todos os argumentos sao obrigatorios e inteiros. Faixas aceitas:
`largura`, `altura` em `[1, 100000]`; `max_iteracoes` em `[1, INT_MAX]`;
`num_threads` em `[1, 1024]`.

## Saida

Em caso de sucesso o programa nao escreve nada em `stdout` nem em `stderr`, e gera:

- `mandelbrot_lada_serial.pgm`
- `mandelbrot_lada_openmp.pgm`
- `mandelbrot_lada_pthreads1.pgm`
- `mandelbrot_lada_pthreads2.pgm`
- `times.txt` — uma linha por implementacao: nome e tempo de calculo em segundos.

Cada `.pgm` contem apenas o raster: uma linha de texto por linha da imagem, com os
valores de 0 a 255 separados por um unico espaco. Para visualizar, basta antepor o
cabecalho PGM:

```
{ printf "P2\n800 800\n255\n"; cat mandelbrot_lada_serial.pgm; } > preview.pgm
open preview.pgm
```

O tempo medido cobre **somente o calculo**; a escrita dos arquivos fica fora da
medicao. O relogio e o `CLOCK_MONOTONIC` (tempo de parede) — `clock()` devolveria a
soma do tempo de CPU de todas as threads e inverteria o speedup.

## Codigos de saida

| Codigo | Significado |
| --- | --- |
| 0 | sucesso |
| 2 | linha de comando invalida |
| 3 | falha de alocacao de memoria |
| 4 | falha ao abrir, escrever ou fechar arquivo |
| 5 | falha na criacao, no join ou no mutex das threads |
| 6 | falha na leitura do relogio monotonico |

Toda falha imprime uma mensagem especifica em `stderr`.

## Testes

```
./test.sh
```

A bateria tem quatro blocos:

- **A — casos de erro:** 13 linhas de comando invalidas; exige exit code diferente
  de 0, mensagem em `stderr`, `stdout` vazio e nenhuma morte por sinal.
- **B — casos validos de borda:** exige exit 0 e os 4 `.pgm` com o mesmo md5.
- **C — formato da saida:** numero de linhas, campos por linha e faixa `[0, 255]`.
- **D — benchmark:** roda 1000x1000 com 5000 iteracoes para 1, 2, 4 e 8 threads e
  imprime tempo, speedup e eficiencia.

O script retorna 0 somente se todos os testes passarem.

## Arquivos

| Arquivo | Responsabilidade |
| --- | --- |
| `mandelbrot.c` | programa inteiro: validacao, as 4 implementacoes, escrita e medicao |
| `Makefile` | deteccao do compilador, alvos `mandelbrot`, `run` e `clean` |
| `test.sh` | bateria de testes e benchmark |
| `evidencias.log` | log de sessao gerado por `script -a evidencias.log` |
