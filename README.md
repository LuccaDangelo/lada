# Implementação 5 — Sincronização do Metrô (Infraestrutura de Software, CESAR School)

Autor: Lucca Dangelo (lada@cesar.school)

## Problema

Passageiros (threads) chegam a uma estação e esperam um vagão; cada vagão chega com
um número de assentos livres e só parte quando estiver cheio ou quando não houver
mais passageiros esperando, depois que todos os liberados embarcarem. A
sincronização usa apenas `pthread_mutex_*` e `pthread_cond_*`.

## Arquivos

| Arquivo             | Responsabilidade |
|---------------------|------------------|
| `sync_car.h`        | Definição da `struct station` (completa no header porque o teste a aloca na stack) e protótipos. |
| `sync_car.c`        | Implementação de `station_init`, `station_load_car`, `station_wait_for_car` e `station_on_board`. |
| `sync_car-runner.c` | Teste oficial fornecido pelo professor, incluído sem alterações (contém o `main`). |
| `test_extra.c`      | Testes próprios de `station_load_car` com `count <= 0`. |
| `Makefile`          | Compilação e alvos de teste (`all`, `run`, `stress`, `tsan`, `test-extra`, `clean`). |
| `evidencias.log`    | Log da sessão de terminal gerado com `script`. |

## Como compilar e executar

```sh
make clean && make
./sync_car
```

ou

```sh
make run
```

## Como testar

```sh
make stress            # roda ./sync_car N vezes (padrão N=100) e resume acertos/falhas
make stress N=500      # mesmo teste com N definido
make tsan              # compila sync_car_tsan com -fsanitize=thread e roda 10 vezes
make test-extra        # testes próprios: load_car com count -5, 0 e 3
```

`make tsan` termina com erro mesmo com o teste passando: no macOS o TSan aborta o
processo quando há avisos, e o runner do professor gera avisos (ver abaixo).

## Resultados obtidos

- Teste oficial (`./sync_car`): "Looks good!".
- `make stress N=500`: 500/500 ok, 0 falhas.
- `make test-extra`: 3/3 PASS.
- `make tsan`: nenhum aviso em `sync_car.c`. Todos os avisos estão em
  `sync_car-runner.c`: data races em `threads_completed` e `load_car_returned`
  (variáveis globais do teste acessadas sem sincronização) e thread leaks por
  falta de `pthread_join` nas threads criadas pelo `main`.

## Restrições atendidas

- Apenas `pthread_mutex_*` e `pthread_cond_*`; nenhum semáforo.
- Um único mutex por estação (`station->lock`).
- Sem busy-wait: toda espera é `pthread_cond_wait` dentro de `while`.
- Embarque simultâneo: `pthread_cond_broadcast(&car_arrived)` libera vários
  passageiros de `station_wait_for_car` antes de qualquer `station_on_board`.

## Sistema

macOS 26.5.1 (arm64), Apple clang 21.0.0 (invocado como `gcc`), GNU Make 3.81.

## Relatório

[lada.pdf](lada.pdf) (na raiz do repositório).
