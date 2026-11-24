# Gerenciador de Memória Virtual

Implementação 6 de Infraestrutura de Software: simulador de gerenciador de memória virtual em C,
baseado no projeto "Designing a Virtual Memory Manager" (Silberschatz, 10ª ed.).

- Endereços lógicos de 16 bits (página = bits 8–15, offset = bits 0–7)
- Páginas/frames de 256 bytes, tabela de páginas com 256 entradas, 128 frames
- TLB de 16 entradas; a busca na TLB usa uma thread por entrada (`pthread`)
- Substituição de página e da TLB: `fifo` ou `lru`

Relatório: [lada.pdf](lada.pdf)

## Arquivos

- `vm.c` — código-fonte completo
- `Makefile` — compilação e opções de teste

Arquivos necessários na pasta para executar (não versionados):

- `BACKING_STORE.bin` — armazenamento secundário
- o arquivo de endereços (ex.: `addresses.txt`)

## Compilar e executar

```sh
make                              # gera ./vm
./vm addresses.txt lru fifo       # <endereços> <subst. de página> <subst. da TLB>
make clean                        # remove vm, vm_tsan, *.o e correct.txt
```

A saída é gravada em `correct.txt`.

## Opções de teste

| Comando | Efeito |
|---|---|
| `make FRAMES=256` | memória física com 256 frames (sem substituição de página) |
| `make TLB=0` | sem TLB: coluna `TLB` = -1 e `TLB Hits = 0` |
| `make THREADS=0` | busca sequencial na TLB, sem threads (só faz sentido com `TLB=1`) |
| `make tsan` | compila `vm_tsan` com ThreadSanitizer e roda `./vm_tsan addresses.txt lru fifo` |

Rode sempre `make clean` antes de trocar uma opção: o `make` não detecta a mudança das variáveis
e reaproveitaria o `vm.o` antigo. Ex.: `make clean && make THREADS=0`.

## Formato do correct.txt

Uma linha por endereço, seguida das estatísticas:

```
Virtual address: 16916 TLB: 0 Physical address: 20 Value: 0
...
Number of Translated Addresses = 1000
Page Faults = 539
Page Fault Rate = 0.539
TLB Hits = 54
TLB Hit Rate = 0.054
```

`TLB` é o índice da entrada da TLB usada (encontrada no hit ou onde a tradução foi inserida no
miss).

## Resultados

Com `./vm addresses.txt lru fifo` (128 frames): **539 page faults e 54 TLB hits**, saída
idêntica à de referência (`diff` vazio), tanto com threads quanto com `THREADS=0`.

## Tratamento de erros

O programa imprime uma mensagem em `stderr` e termina com código diferente de 0 quando:

- o número de argumentos é diferente de 3;
- o algoritmo de página ou da TLB não é `fifo` nem `lru` (somente minúsculas);
- o arquivo de endereços não existe ou não pode ser lido;
- uma linha do arquivo não é um inteiro não negativo, ou o endereço está fora de 0..65535
  (a mensagem informa a linha);
- o arquivo de endereços não contém nenhum endereço;
- `BACKING_STORE.bin` não existe, não pode ser lido ou está truncado;
- `correct.txt` não pode ser criado, escrito ou fechado;
- falta memória ao ler os endereços;
- `pthread_create` ou `pthread_join` falha na busca da TLB.

Todo o arquivo de endereços é validado antes do processamento.

## Sistema usado

macOS (Darwin 25.5.0, arm64), compilador `gcc` → Apple clang version 21.0.0
(clang-2100.1.1.101), conforme `gcc --version`.
