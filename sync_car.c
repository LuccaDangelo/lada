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
	pthread_mutex_lock(&station->lock);

	station->free_seats = count;
	/* acorda todos os passageiros da plataforma: vários podem ser liberados
	 * de uma vez (até `count`), antes de qualquer on_board */
	pthread_cond_broadcast(&station->car_arrived);

	/* while, e não if: pthread_cond_wait pode ter despertar espúrio, e quando
	 * acordamos a condição pode já não valer (ex.: chegou outro passageiro
	 * enquanto ainda há assentos). Só seguimos quando ela for de fato falsa:
	 * vagão cheio ou plataforma vazia, e nenhum liberado ainda embarcando. */
	while ((station->free_seats > 0 && station->waiting_passengers > 0) ||
	       station->boarding_passengers > 0)
		pthread_cond_wait(&station->all_boarded, &station->lock);

	/* fecha o vagão: se sobraram assentos (plataforma vazia), um passageiro
	 * que chegasse depois da partida veria free_seats > 0 e "embarcaria" num
	 * vagão que já foi embora. Zerar garante que ele espere o próximo. */
	station->free_seats = 0;

	pthread_mutex_unlock(&station->lock);
}

void station_wait_for_car(struct station *station)
{
	pthread_mutex_lock(&station->lock);

	station->waiting_passengers++;
	/* while, e não if: despertar espúrio, e o broadcast acorda todos, mas
	 * só há free_seats lugares; quem acordar tarde reencontra 0 e volta
	 * a esperar o próximo vagão */
	while (station->free_seats == 0)
		pthread_cond_wait(&station->car_arrived, &station->lock);

	station->waiting_passengers--;
	station->free_seats--;
	station->boarding_passengers++;

	pthread_mutex_unlock(&station->lock);
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
