#!/bin/sh
#
# test.sh - bateria de testes do projeto Mandelbrot.
#
# Blocos:
#   A) casos de erro       - exit code != 0, mensagem em stderr, sem sinal
#   B) casos validos       - exit code 0 e os 4 .pgm identicos
#   C) formato da saida    - stdout vazio, dimensoes e faixa de valores
#   D) benchmark           - tabela de tempos, speedup e eficiencia
#
# Uso: ./test.sh [caminho-do-binario]
# Retorna 0 se todos os testes passarem; != 0 caso contrario.

BIN=${1:-./mandelbrot}
LOGIN=lada

if [ ! -x "$BIN" ]; then
    echo "Erro: binario \"$BIN\" nao encontrado. Rode 'make' antes." >&2
    exit 1
fi
BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")

PASS=0
FAIL=0
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

ok()   { PASS=$((PASS + 1)); printf '  [ OK ] %s\n' "$1"; }
bad()  { FAIL=$((FAIL + 1)); printf '  [FALHA] %s\n' "$1"; }

# md5 de um arquivo, portavel entre Linux e macOS.
hash_of() {
    if command -v md5sum > /dev/null 2>&1; then
        md5sum "$1" | awk '{print $1}'
    else
        md5 -q "$1"
    fi
}

# Numero de hashes distintos entre os 4 arquivos de saida.
distinct_hashes() {
    for f in mandelbrot_${LOGIN}_*.pgm; do hash_of "$f"; done | sort -u | wc -l | tr -d ' '
}

cd "$WORK" || exit 1

# ------------------------------------------------------------------
printf '\n=== BLOCO A: casos de erro ===\n'
printf 'Esperado: exit code != 0, mensagem em stderr, sem morte por sinal.\n\n'
# ------------------------------------------------------------------

check_error() {
    desc="mandelbrot $*"
    rm -f *.pgm times.txt out.txt err.txt
    "$BIN" "$@" > out.txt 2> err.txt
    rc=$?

    if [ "$rc" -eq 0 ]; then
        bad "$desc  (exit code 0, esperava != 0)"
    elif [ "$rc" -ge 128 ]; then
        bad "$desc  (morto por sinal: exit code $rc)"
    elif [ ! -s err.txt ]; then
        bad "$desc  (exit $rc, mas stderr vazio)"
    elif [ -s out.txt ]; then
        bad "$desc  (escreveu em stdout)"
    else
        ok "$desc  -> exit $rc: $(head -n 1 err.txt)"
    fi
}

check_error
check_error 100 100 100
check_error 100 100 100 4 5
check_error 0 100 100 4
check_error -5 100 100 4
check_error abc 100 100 4
check_error 100 100 0 4
check_error 100 100 100 0
check_error 100 100 100 -2
check_error 100 100 100 abc
check_error 12abc 100 100 4
check_error 999999999 999999999 100 4
check_error 100 100 100 99999

# ------------------------------------------------------------------
printf '\n=== BLOCO B: casos validos de borda ===\n'
printf 'Esperado: exit code 0 e os 4 arquivos byte-a-byte identicos.\n\n'
# ------------------------------------------------------------------

check_valid() {
    desc="mandelbrot $*"
    rm -f *.pgm times.txt out.txt err.txt
    "$BIN" "$@" > out.txt 2> err.txt
    rc=$?

    if [ "$rc" -ne 0 ]; then
        bad "$desc  (exit code $rc: $(head -n 1 err.txt))"
        return
    fi
    if [ -s err.txt ]; then
        bad "$desc  (stderr nao vazio: $(head -n 1 err.txt))"
        return
    fi

    n=$(ls mandelbrot_${LOGIN}_*.pgm 2> /dev/null | wc -l | tr -d ' ')
    if [ "$n" -ne 4 ]; then
        bad "$desc  (gerou $n arquivos, esperava 4)"
        return
    fi

    h=$(distinct_hashes)
    if [ "$h" -ne 1 ]; then
        bad "$desc  ($h hashes distintos, as implementacoes divergiram)"
    else
        ok "$desc  -> 4 arquivos identicos ($(hash_of mandelbrot_${LOGIN}_serial.pgm))"
    fi
}

check_valid 1 1 1 1
check_valid 100 10 100 64
check_valid 10 100 100 3
check_valid 800 800 1000 1
check_valid 800 800 1000 4

# ------------------------------------------------------------------
printf '\n=== BLOCO C: formato da saida ===\n'
printf 'Base: mandelbrot 800 800 1000 4 (largura 800, altura 800).\n\n'
# ------------------------------------------------------------------

W=800
H=800
rm -f *.pgm times.txt out.txt err.txt
"$BIN" $W $H 1000 4 > out.txt 2> err.txt
rc=$?

if [ "$rc" -ne 0 ]; then
    bad "execucao base falhou (exit $rc)"
else
    ok "execucao base terminou com exit 0"
fi

if [ -s out.txt ]; then
    bad "stdout deveria ficar vazio (tem $(wc -c < out.txt) bytes)"
else
    ok "stdout vazio"
fi

if [ -s err.txt ]; then
    bad "stderr deveria ficar vazio (tem $(wc -c < err.txt) bytes)"
else
    ok "stderr vazio"
fi

for f in mandelbrot_${LOGIN}_*.pgm; do
    campos=$(awk '{print NF}' "$f" | sort -u | tr '\n' ' ' | sed 's/ $//')
    if [ "$campos" = "$W" ]; then
        ok "$f: toda linha tem exatamente $W campos"
    else
        bad "$f: campos por linha = [$campos], esperava $W"
    fi

    linhas=$(wc -l < "$f" | tr -d ' ')
    if [ "$linhas" -eq "$H" ]; then
        ok "$f: $H linhas"
    else
        bad "$f: $linhas linhas, esperava $H"
    fi

    faixa=$(tr ' ' '\n' < "$f" | awk '
        NF { if ($1 < min || NR == 1) min = $1; if ($1 > max) max = $1 }
        END { print (min >= 0 && max <= 255) ? "ok" : "fora"; }')
    if [ "$faixa" = "ok" ]; then
        ok "$f: todos os valores em [0, 255]"
    else
        bad "$f: ha valores fora de [0, 255]"
    fi
done

# ------------------------------------------------------------------
printf '\n=== BLOCO D: benchmark (1000 x 1000, 5000 iteracoes) ===\n\n'
# ------------------------------------------------------------------

get_time() { awk -v k="$1" '$1 == k { print $2 }' times.txt; }

printf '%-10s %10s %10s %10s %10s\n' threads serial openmp pthreads1 pthreads2
printf '%.0s-' 1 2 3 4 5 6 7 8 9 10; printf '%.0s-' 1 2 3 4 5 6 7 8 9 10
printf '%.0s-' 1 2 3 4 5 6 7 8 9 10; printf '%.0s-' 1 2 3 4 5 6 7 8 9 10
printf '%.0s-' 1 2 3 4 5 6 7 8 9 10; printf '\n'

BENCH_FAIL=0
for n in 1 2 4 8; do
    rm -f *.pgm times.txt
    if ! "$BIN" 1000 1000 5000 "$n" 2> err.txt; then
        bad "benchmark com $n threads falhou: $(head -n 1 err.txt)"
        BENCH_FAIL=1
        continue
    fi

    ts=$(get_time serial)
    to=$(get_time openmp)
    t1=$(get_time pthreads1)
    t2=$(get_time pthreads2)

    printf '%-10s %10s %10s %10s %10s\n' "$n" "$ts" "$to" "$t1" "$t2"

    printf '%-10s ' "  speedup"
    awk -v s="$ts" -v a="$to" -v b="$t1" -v c="$t2" \
        'BEGIN { printf "%9.2fx %9.2fx %9.2fx %9.2fx\n", 1, s/a, s/b, s/c }'
    printf '%-10s ' "  eficien."
    awk -v s="$ts" -v a="$to" -v b="$t1" -v c="$t2" -v n="$n" \
        'BEGIN { printf "%9s %9.0f%% %9.0f%% %9.0f%%\n", "-", 100*s/a/n, 100*s/b/n, 100*s/c/n }'
done

if [ "$BENCH_FAIL" -eq 0 ]; then
    ok "benchmark completo para 1, 2, 4 e 8 threads"
else
    bad "benchmark teve execucoes com falha"
fi

printf '\nspeedup = tempo_serial / tempo_versao    eficiencia = speedup / num_threads\n'

# ------------------------------------------------------------------
TOTAL=$((PASS + FAIL))
printf '\n===============================================\n'
printf '%d/%d testes passaram\n' "$PASS" "$TOTAL"
printf '===============================================\n'

[ "$FAIL" -eq 0 ] || exit 1
exit 0
