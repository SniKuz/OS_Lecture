/*
 * Copyright(c) 2021-2023 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 */
#include "pthread_pool.h"
#include <stdio.h>
/*
 * 풀에 있는 일꾼(일벌) 스레드가 수행할 함수이다.
 * FIFO 대기열에서 기다리고 있는 작업을 하나씩 꺼내서 실행한다.
 * 대기열에 작업이 없으면 새 작업이 들어올 때까지 기다린다.
 * 이 과정을 스레드풀이 종료될 때까지 반복한다.
 */
void foo(void *i)
{
	//do nothing
}

static void *worker(void *param)
{
	pthread_pool_t *pool = (pthread_pool_t *)param;
	task_t *task;
	while(pool->running)
	{
		pthread_mutex_lock(&pool->mutex);
		while(pool->q_len == 0 && pool->running)//no element
		{
			pthread_cond_wait(&pool->full, &pool->mutex);
			printf("isEnter? and len : %d\n", pool->q_len);
		}
		//pthread_mutex_unlock(&pool->mutex);

		//pthread_mutex_lock(&pool->mutex);
		task = &pool->q[pool->q_front];
		pool->q_front = (pool->q_front + 1) % pool->q_size;
		pool->q_len -= 1;
		printf("qlen : %d\n", pool->q_len);
		//task->function(task->param);
		pthread_mutex_unlock(&pool->mutex);

		task->function(task->param);// must put in mutex?

		pthread_mutex_lock(&pool->mutex);
		pthread_cond_signal(&pool->empty);
		pthread_mutex_unlock(&pool->mutex);

		//free(task);
	}
	pthread_exit(NULL);
}

/*
 * 스레드풀을 생성한다. bee_size는 일꾼(일벌) 스레드의 개수이고, queue_size는 대기열의 용량이다.
 * bee_size는 POOL_MAXBSIZE를, queue_size는 POOL_MAXQSIZE를 넘을 수 없다.
 * 일꾼 스레드와 대기열에 필요한 공간을 할당하고 변수를 초기화한다.
 * 일꾼 스레드의 동기화를 위해 사용할 상호배타 락과 조건변수도 초기화한다.
 * 마지막 단계에서는 일꾼 스레드를 생성하여 각 스레드가 worker() 함수를 실행하게 한다.
 * 대기열로 사용할 원형 버퍼의 용량이 일꾼 스레드의 수보다 작으면 효율을 극대화할 수 없다.
 * 이런 경우 사용자가 요청한 queue_size를 bee_size로 상향 조정한다.
 * 성공하면 POOL_SUCCESS를, 실패하면 POOL_FAIL을 리턴한다.
 */
int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size)
{
	int success = 0;
	if(bee_size > POOL_MAXBSIZE) return POOL_FAIL;
	if(queue_size > POOL_MAXQSIZE) return POOL_FAIL;
	if(bee_size > queue_size) queue_size = bee_size;

	pool->running = true;

	pool->q = (task_t*)calloc(queue_size,sizeof(task_t));
	if(pool->q == NULL) success = 1;
	pool->q_size = queue_size;
	pool->q_front = 0;
	pool->q_len = 0;

	for(int i = 0; i < pool->q_size; i++)
	{
		//base task init
		task_t *t = (task_t*)malloc(sizeof(task_t));
		t->function = foo;
		t->param = NULL;
		pool->q[i] = *t;
	}


	pool->bee = (pthread_t*)calloc(bee_size,sizeof(pthread_t));
	if(pool->bee == NULL) success = 1;
	pool->bee_size = bee_size;

	pthread_mutex_init(&pool->mutex, NULL);
	pthread_cond_init(&pool->full, NULL);
	pthread_cond_init(&pool->empty, NULL);
	pthread_cond_init(&pool->shutdown, NULL);


	for(int i = 0; i < pool->bee_size; i++)
	{
	       if(pthread_create(&pool->bee[i], NULL, worker, (void *)pool) != 0)
		       success = 1;
	}

	if(success != 0) return POOL_FAIL;
	else return POOL_SUCCESS;
}

/*
 * 스레드풀에서 실행시킬 함수와 인자의 주소를 넘겨주며 작업을 요청한다.
 * 스레드풀의 대기열이 꽉 찬 상황에서 flag이 POOL_NOWAIT이면 즉시 POOL_FULL을 리턴한다.
 * POOL_WAIT이면 대기열에 빈 자리가 나올 때까지 기다렸다가 넣고 나온다.
 * 작업 요청이 성공하면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_submit(pthread_pool_t *pool, void (*f)(void *p), void *p, int flag)
{
	if((pool->q_len == pool->q_size) && (flag == POOL_NOWAIT)) return POOL_FULL;
	pthread_mutex_lock(&pool->mutex);
	while(pool->q_len == pool->q_size)
		pthread_cond_wait(&pool->empty, &pool->mutex);
	//pthread_mutex_unlock(&pool->mutex);

	//pthread_mutex_lock(&pool->mutex);
	task_t *task = (task_t*)malloc(sizeof(task_t));
	task->function = f;
	task->param = p;
	pool->q[(pool->q_front + pool->q_len) % pool->q_size] = *task;
	pool->q_len += 1;
	pthread_mutex_unlock(&pool->mutex);

	pthread_mutex_lock(&pool->mutex);
	pthread_cond_signal(&pool->full);
	pthread_mutex_unlock(&pool->mutex);


	return POOL_SUCCESS;
}

/*
 * 스레드풀을 종료한다. 일꾼 스레드가 현재 작업 중이면 그 작업을 마치게 한다.
 * how의 값이 POOL_COMPLETE이면 대기열에 남아 있는 모든 작업을 마치고 종료한다.
 * POOL_DISCARD이면 대기열에 새 작업이 남아 있어도 더 이상 수행하지 않고 종료한다.
 * 부모 스레드는 종료된 일꾼 스레드와 조인한 후에 스레드풀에 할당된 자원을 반납한다.
 * 스레드를 종료시키기 위해 철회를 생각할 수 있으나 바람직하지 않다.
 * 락을 소유한 스레드를 중간에 철회하면 교착상태가 발생하기 쉽기 때문이다.
 * 종료가 완료되면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_shutdown(pthread_pool_t *pool, int how)
{
	if(how == POOL_COMPLETE)
	{
		printf("len: %d, size: %d \n", pool->q_len, pool->q_size);
		//현재 문제점 아마 큐렌이 스레드 개수만큼만 줄어들고 0이 안되는중으로 보임
		while(true)//pool->q_len != 0
		{
			pthread_mutex_lock(&pool->mutex);
			if(pool->q_len == 0)
			{
				pthread_mutex_unlock(&pool->mutex);
				break;
			}
		}

		//printf("qlen : %d ", pool->q_len); // spin lock
	}
	printf("Enter SpinLock\n");

	pool->running = false;
	pthread_cond_broadcast(&pool->full);
	for(int i = 0; i < pool->bee_size; i++)
	{
		//printf("%d : %ld\n", i, pool->bee[i]);
		pthread_join(pool->bee[i], NULL);
	}

	free(pool->q);
	free(pool->bee);
	pthread_mutex_destroy(&pool->mutex);
	pthread_cond_destroy(&pool->full);
	pthread_cond_destroy(&pool->empty);
	pthread_cond_destroy(&pool->shutdown);

	return POOL_SUCCESS;
}
