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

## Executar

```sh
./vm <arquivo_enderecos> <fifo|lru> <fifo|lru>
```

- 2º argumento: algoritmo de substituição de página (`fifo` ou `lru`)
- 3º argumento: algoritmo de substituição da TLB (`fifo` ou `lru`)

Exemplo: `./vm addresses.txt lru fifo`

O arquivo `BACKING_STORE.bin` deve estar no diretório atual. O arquivo de endereços deve ter um
número inteiro entre 0 e 65535 por linha (linhas vazias são ignoradas).
