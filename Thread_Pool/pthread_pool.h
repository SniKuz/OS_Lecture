/*
 * Copyright(c) 2021-2023 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * [Header("CODE_FIX_n")]
 * CODE_FIX_1
 * 2023.05.30, ICT융합학부 2019043118 김건하
 * 수정내용 :
 * 1. pthread_mutex_t operate 추가. 사유는 조건변수용 뮤텍스락을 따로 마련하는 방안을 생각
 * 
 * CODE_FIX_2
 * 2023.05.31
 * 1. pthread_mutex_t operate 삭제. 사용 효용성 없는 것 같으며 스레드풀을 관리할 때 조건 변수 대기와 상호배타 락이 혼란이 옴
 * 2. pthread_cond_t shutdown 추가. 스레드를 종료할 때 원형큐에 남은 일을 다 할 때 까지 대기용
 */
#ifndef _PTHREAD_POOL_H_
#define _PTHREAD_POOL_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h> //malloc

#define POOL_MAXBSIZE 128
#define POOL_MAXQSIZE 1024
#define POOL_WAIT 0
#define POOL_NOWAIT 1
#define POOL_SUCCESS 0
#define POOL_FAIL 1
#define POOL_FULL 2
#define POOL_DISCARD 0
#define POOL_COMPLETE 1

/*
 * 스레드를 통해 실행할 작업 함수와 함수의 인자정보 구조체 타입
 */
typedef struct {
    void (*function)(void *param);
    void *param;
} task_t;

/*
 * 스레드풀을 운영하는데 필요한 정보를 저장하는 스레드풀 제어블록 구조체 타입
 *
 * running은 스레드풀이 현재 실행 또는 종료 상태임을 나타낸다.
 * 스레드풀의 FIFO 작업 대기열인 배열 q는 원형 버퍼의 역할을 한다.
 * q_size는 원형버퍼로 사용하는 배열 q의 방의 갯수를 의미한다.
 * q_front는 대기열에서 다음에 실행될 작업의 위치를 나타낸다.
 * q_len은 대기열의 길이를 나타낸다. q_len이 0이면 현재 대기하고 있는 작업이 없다는 뜻이다.
 * q_len의 값이 q_size이면 대기열이 차서 새 작업을 더 넣을 수 없는 상황을 의미한다.
 * bee는 작업을 수행하는 일꾼 스레드의 ID를 저장하는 배열이다.
 * bee_size는 배열 bee의 크기를 나타내며 일꾼 스레드의 갯수를 의미한다.
 * mutex는 대기열을 조회하거나 변경하기 위해 사용하는 상호배타 락이다.
 * full과 empty는 대기열에 작업이 채워지기를 또는 빈 자리가 생기기를 기다리는 조건 변수이다.
 */
typedef struct {
    bool running;           /* 스레드풀의 실행 또는 종료 상태 */
    task_t *q;              /* FIFO 작업 대기열로 사용할 원형 버퍼 */
    int q_size;             /* 원형 버퍼 q 배열의 크기 */
    int q_front;            /* 대기열에서 다음에 실행될 작업의 위치 */
    int q_len;              /* 대기열의 길이, 0이면 현재 대기하고 있는 작업이 없다는 뜻 */
    pthread_t *bee;         /* 일꾼(일벌) 스레드의 ID를 저장하기 위한 배열 */
    int bee_size;           /* bee 배열의 크기로 일꾼 스레드의 수를 의미 */
    pthread_mutex_t mutex;  /* 대기열을 접근하기 위해 사용하는 상호배타 락 */
    pthread_cond_t full;    /* 빈 대기열에 새 작업이 들어올 때까지 기다리는 곳 */
    pthread_cond_t empty;   /* 대기열에 빈 자리가 발생할 때까지 기다리는 곳 */
    pthread_cond_t shutdown; /* pthread_pool_shutdown 중 how가 POOL_COMPLETE인 경우 모든 큐가 끝날때까지 기다리는 곳*/
} pthread_pool_t;

int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size);
int pthread_pool_submit(pthread_pool_t *pool, void (*f)(void *p), void *p, int flag);
int pthread_pool_shutdown(pthread_pool_t *pool, int how);

#endif
