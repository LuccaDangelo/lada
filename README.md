# Sincronização do Metrô

Trabalho de Infraestrutura de Software (CESAR School).

## Problema

Passageiros chegam a uma estação e esperam por vagões. Cada vagão chega com um
número de assentos livres e só pode partir quando estiver cheio ou quando não
houver mais passageiros esperando. A sincronização entre a thread do vagão e as
threads dos passageiros é feita apenas com `pthread_mutex_*` e `pthread_cond_*`
(no máximo 1 mutex por estação, sem semáforos e sem busy-wait).

## Arquivos

| Arquivo              | Responsabilidade                                                                 |
|----------------------|----------------------------------------------------------------------------------|
| `sync_car.h`         | Definição da `struct station` e protótipos das funções da estação.               |
| `sync_car.c`         | Implementação de `station_init`, `station_load_car`, `station_wait_for_car` e `station_on_board`. |
| `sync_car-runner.c`  | Teste oficial do professor (contém o `main()`). Não deve ser alterado.           |
| `Makefile`           | Compila tudo no executável `sync_car` (alvos `all`, `run`, `clean`).             |

## Como compilar

```sh
make clean && make
```

## Como executar

```sh
./sync_car
# ou
make run
```

## Testes

TODO

## Ambiente de desenvolvimento

macOS 26.5.1 (Darwin 25.5.0, arm64), Apple clang 21.0.0 (invocado como `gcc`).
