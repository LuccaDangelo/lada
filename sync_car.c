#include "sync_car.h"

void station_init(struct station *station)
{
	pthread_mutex_init(&station->lock, NULL);
	pthread_cond_init(&station->car_arrived, NULL);
	pthread_cond_init(&station->all_boarded, NULL);
	station->waiting_passengers = 0;
	station->free_seats = 0;
	station->boarding_passengers = 0;
}

void station_load_car(struct station *station, int count)
{
	/* TODO: chamada quando um vagão chega com `count` assentos livres.
	 * Plano:
	 *   - travar o lock;
	 *   - free_seats = count;
	 *   - broadcast em car_arrived (acorda os passageiros na plataforma);
	 *   - while ((free_seats > 0 && waiting_passengers > 0) ||
	 *            boarding_passengers > 0)
	 *         esperar em all_boarded;
	 *   - free_seats = 0 (fecha o vagão);
	 *   - destravar o lock. */
	(void)station;
	(void)count;
}

void station_wait_for_car(struct station *station)
{
	/* TODO: chamada por um passageiro ao chegar na estação.
	 * Plano:
	 *   - travar o lock;
	 *   - waiting_passengers++;
	 *   - while (free_seats == 0)
	 *         esperar em car_arrived;
	 *   - waiting_passengers--; free_seats--; boarding_passengers++;
	 *   - destravar o lock. */
	(void)station;
}

void station_on_board(struct station *station)
{
	/* TODO: chamada pelo passageiro depois de sentar no vagão.
	 * Plano:
	 *   - travar o lock;
	 *   - boarding_passengers--;
	 *   - se boarding_passengers == 0 &&
	 *        (free_seats == 0 || waiting_passengers == 0)
	 *         signal em all_boarded;
	 *   - destravar o lock. */
	(void)station;
}
