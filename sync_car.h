#ifndef SYNC_CAR_H
#define SYNC_CAR_H

#include <pthread.h>

/*
 * Estado compartilhado de uma estação de metrô.
 * Precisa ser um tipo completo: o teste aloca `struct station` na stack.
 */
struct station {
	/* TODO: substituir pelos campos reais (mutex, variáveis de condição,
	 * contadores de passageiros e assentos) no próximo commit. */
	int placeholder;
};

void station_init(struct station *station);
void station_load_car(struct station *station, int count);
void station_wait_for_car(struct station *station);
void station_on_board(struct station *station);

#endif /* SYNC_CAR_H */
