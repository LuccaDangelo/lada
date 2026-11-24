# Gerenciador de Memória Virtual

Implementação 6 de Infraestrutura de Software: simulador de gerenciador de memória virtual em C,
baseado no projeto "Designing a Virtual Memory Manager" (Silberschatz, 10ª ed.).

- Endereços lógicos de 16 bits (página = bits 8–15, offset = bits 0–7)
- Páginas/frames de 256 bytes, tabela de páginas com 256 entradas, TLB com 16 entradas
- Valores lidos de `BACKING_STORE.bin`; saída gravada em `correct.txt`

## Compilar

```sh
make          # gera o executável ./vm
make clean    # remove o executável, os .o e o correct.txt
```

### Número de frames

A memória física tem **128 frames por padrão** (o que o enunciado pede), então `make` sozinho
já gera a versão de entrega. O número de frames pode ser trocado na compilação com a variável
`FRAMES`, por exemplo para testar com memória física igual ao espaço virtual (sem substituição):

```sh
make clean && make FRAMES=256   # 256 frames (fluxo de teste)
make clean && make              # volta para o padrão de 128 frames
```

Sempre rode `make clean` ao trocar `FRAMES`: o `make` não detecta a mudança da variável e
reaproveitaria o `vm.o` antigo.

### TLB

A TLB (16 entradas, substituição `fifo` ou `lru` conforme o 3º argumento) vem **ligada por
padrão**. Para demonstrar o fluxo sem TLB (só tabela de páginas), compile com `TLB=0`:

```sh
make clean && make TLB=0   # sem TLB: coluna "TLB" = -1 e TLB Hits = 0
make clean && make         # volta para o padrão, com TLB
```

Com `TLB=0` o 3º argumento continua obrigatório e validado, mas não tem efeito. Assim como em
`FRAMES`, rode `make clean` ao trocar `TLB`.

## Executar

```sh
./vm <arquivo_enderecos> <fifo|lru> <fifo|lru>
```

- 2º argumento: algoritmo de substituição de página (`fifo` ou `lru`)
- 3º argumento: algoritmo de substituição da TLB (`fifo` ou `lru`)

Exemplo: `./vm addresses.txt lru fifo`

O arquivo `BACKING_STORE.bin` deve estar no diretório atual. O arquivo de endereços deve ter um
número inteiro entre 0 e 65535 por linha (linhas vazias são ignoradas).
