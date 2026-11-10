# lada — Algoritmo do Banqueiro

Implementação em C do algoritmo do banqueiro para evitar deadlock.
Atividade de Infraestrutura de Software — Implementação 5.

## Compilar

```sh
make        # gera o executável banker
make clean  # remove o executável e os arquivos gerados
```

## Executar

```sh
./banker 10 5 7
```

Cada argumento é o número de instâncias de um tipo de recurso. O programa lê
`customer.txt` e `commands.txt` do diretório atual e gera `result.txt`.
