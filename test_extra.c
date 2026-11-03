/*
 * Testes extras (não fazem parte do teste oficial do professor):
 * station_load_car com count <= 0 não pode liberar nenhum passageiro.
 *
 *   a) 3 passageiros esperando, load_car(-5): retorna em < 1 s, 0 liberados;
 *   b) os mesmos 3 esperando, load_car(0):    retorna em < 1 s, 0 liberados;
 *   c) load_car(3): libera os 3, on_board dos 3, vagão parte com exatamente 3.
 */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "sync_car.h"

#define NPASS 3

static struct station st;

/* estado do próprio teste (separado do lock da station) */
static pthread_mutex_t t_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  t_cond = PTHREAD_COND_INITIALIZER;
static int released;      /* passageiros que retornaram de wait_for_car */
static int load_returned; /* a chamada atual de load_car já retornou? */

static void alarm_handler(int sig)
{
	(void)sig;
	static const char msg[] = "FAIL: timeout global (alarm) estourou\n";
	write(STDERR_FILENO, msg, sizeof msg - 1);
	_exit(1);
}

static void *passenger(void *arg)
{
	(void)arg;
	station_wait_for_car(&st);
	pthread_mutex_lock(&t_lock);
	released++;
	pthread_cond_broadcast(&t_cond);
	pthread_mutex_unlock(&t_lock);
	return NULL;
}

static void *loader(void *arg)
{
	station_load_car(&st, *(int *)arg);
	pthread_mutex_lock(&t_lock);
	load_returned = 1;
	pthread_cond_broadcast(&t_cond);
	pthread_mutex_unlock(&t_lock);
	return NULL;
}

static struct timespec deadline_ms(long ms)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	long long ns = (long long)now.tv_usec * 1000 + ms * 1000000LL;
	struct timespec ts;
	ts.tv_sec = now.tv_sec + ns / 1000000000LL;
	ts.tv_nsec = ns % 1000000000LL;
	return ts;
}

/* espera até *flag >= target ou estourar ms; devolve 1 se atingiu. Chamar com t_lock. */
static int wait_until(int *flag, int target, long ms)
{
	struct timespec ts = deadline_ms(ms);
	while (*flag < target)
		if (pthread_cond_timedwait(&t_cond, &t_lock, &ts) == ETIMEDOUT)
			break;
	return *flag >= target;
}

/* inicia load_car(count) em outra thread; devolve se ela retornou em < 1 s */
static int load_returns_quickly(int *count, pthread_t *tid)
{
	pthread_mutex_lock(&t_lock);
	load_returned = 0;
	pthread_mutex_unlock(&t_lock);

	pthread_create(tid, NULL, loader, count);

	pthread_mutex_lock(&t_lock);
	int ok = wait_until(&load_returned, 1, 1000);
	pthread_mutex_unlock(&t_lock);
	return ok;
}

/* caso (a)/(b): load_car(count) com count <= 0 e NPASS esperando */
static int case_no_seats(const char *name, int count)
{
	pthread_t tid;
	int returned = load_returns_quickly(&count, &tid);

	usleep(100 * 1000); /* dá tempo de um passageiro liberado indevidamente aparecer */
	pthread_mutex_lock(&t_lock);
	int r = released;
	pthread_mutex_unlock(&t_lock);

	int ok = returned && r == 0;
	printf("%s: %s (load_car(%d) %s em < 1 s; liberados = %d, esperado 0)\n",
	       ok ? "PASS" : "FAIL", name, count,
	       returned ? "retornou" : "NAO retornou", r);
	if (returned)
		pthread_join(tid, NULL);
	return ok;
}

int main(void)
{
	pthread_t pass[NPASS];
	int failures = 0;

	signal(SIGALRM, alarm_handler);
	alarm(10);

	station_init(&st);
	for (int i = 0; i < NPASS; i++)
		pthread_create(&pass[i], NULL, passenger, NULL);
	usleep(100 * 1000); /* deixa os 3 chegarem na plataforma */

	if (!case_no_seats("(a) count = -5", -5))
		failures++;

	if (failures) {
		/* a station ficou inconsistente (passageiros liberados / vagão preso):
		 * os próximos casos não têm como ser avaliados */
		printf("FAIL: (b) count = 0 (não executado: estado corrompido pelo caso a)\n");
		printf("FAIL: (c) count = 3 (não executado: estado corrompido pelo caso a)\n");
		return 1;
	}

	if (!case_no_seats("(b) count = 0", 0))
		failures++;

	/* caso (c) */
	{
		int count = NPASS;
		pthread_t tid;

		pthread_mutex_lock(&t_lock);
		load_returned = 0;
		pthread_mutex_unlock(&t_lock);
		pthread_create(&tid, NULL, loader, &count);

		pthread_mutex_lock(&t_lock);
		int all_released = wait_until(&released, NPASS, 1000);
		int early = load_returned;
		pthread_mutex_unlock(&t_lock);

		int returned = 0;
		if (all_released) {
			for (int i = 0; i < NPASS; i++)
				station_on_board(&st);
			pthread_mutex_lock(&t_lock);
			returned = wait_until(&load_returned, 1, 1000);
			pthread_mutex_unlock(&t_lock);
		}

		pthread_mutex_lock(&t_lock);
		int r = released;
		pthread_mutex_unlock(&t_lock);

		int ok = all_released && !early && returned && r == NPASS;
		printf("%s: (c) count = 3 (liberados = %d, esperado 3; load_car %s)\n",
		       ok ? "PASS" : "FAIL", r,
		       early ? "retornou antes dos on_board" :
		       returned ? "partiu após os on_board" : "NAO partiu");
		if (!ok)
			failures++;
		else {
			pthread_join(tid, NULL);
			for (int i = 0; i < NPASS; i++)
				pthread_join(pass[i], NULL);
		}
	}

	return failures ? 1 : 0;
}
