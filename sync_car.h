#ifndef SYNC_CAR_H
#define SYNC_CAR_H

#include <pthread.h>

/*
 * Estado compartilhado de uma estação de metrô.
 * Precisa ser um tipo completo: o teste aloca `struct station` na stack.
 */
struct station {
	pthread_mutex_t lock;         /* único mutex da estação (o enunciado exige no máximo 1) */
	pthread_cond_t  car_arrived;  /* passageiros esperam aqui até haver vagão com assento livre */
	pthread_cond_t  all_boarded;  /* o vagão espera aqui até os liberados embarcarem */
	int waiting_passengers;       /* passageiros na plataforma que ainda não foram liberados */
	int free_seats;               /* assentos livres do vagão atual (0 = nenhum vagão na estação) */
	int boarding_passengers;      /* liberados por wait_for_car que ainda não chamaram on_board */
};

void station_init(struct station *station);
void station_load_car(struct station *station, int count);
void station_wait_for_car(struct station *station);
void station_on_board(struct station *station);

#endif /* SYNC_CAR_H */
