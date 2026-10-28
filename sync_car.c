#include "sync_car.h"

void station_init(struct station *station)
{
	/* TODO: inicializar o mutex, as variáveis de condição e os contadores
	 * da estação. Chamada uma única vez antes de qualquer outra função. */
	(void)station;
}

void station_load_car(struct station *station, int count)
{
	/* TODO: chamada quando um vagão chega com `count` assentos livres.
	 * Deve bloquear até que o vagão esteja cheio (todos os assentos
	 * ocupados) ou até não haver mais passageiros esperando, e só então
	 * retornar para o vagão partir. Se count == 0, retorna imediatamente. */
	(void)station;
	(void)count;
}

void station_wait_for_car(struct station *station)
{
	/* TODO: chamada por um passageiro ao chegar na estação. Deve bloquear
	 * até haver um vagão na estação com assento livre, e então retornar
	 * (o passageiro passa a embarcar e depois chama station_on_board). */
	(void)station;
}

void station_on_board(struct station *station)
{
	/* TODO: chamada pelo passageiro depois de sentar no vagão. Informa
	 * à estação que o embarque terminou, permitindo que station_load_car
	 * retorne quando todos os passageiros liberados tiverem embarcado. */
	(void)station;
}
