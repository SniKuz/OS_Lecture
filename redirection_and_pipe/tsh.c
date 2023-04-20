/*
 * Copyright(c) 2023 All rights reserved by Heekuck Oh.
 * 이 프로그램은 한양대학교 ERICA 컴퓨터학부 학생을 위한 교육용으로 제작되었다.
 * 한양대학교 ERICA 학생이 아닌 이는 프로그램을 수정하거나 배포할 수 없다.
 * 프로그램을 수정할 경우 날짜, 학과, 학번, 이름, 수정 내용을 기록한다.
 * 
 * [Header("CODE_FIX_n")]
 * CODE_FIX_1
 * 2023.03.16 , ICT융합학부, 2019043118, 김건하
 * 수정내용 :
 * 1. 리다이렉션 기능 추가: redirectIn(char **fileName), redirectOut(char **fileName)
 * 2. 파이프 기능 추가 : createPipe(char **argv)
 * 3. 명령어 부분 실행 기능 추가 : runPartCmd(char **argv)
 * 4. 명령어 인자 분리 기능 수정 : cmdexec(char *cmd)
 * ㄴ 수정 내용 : 명령어 실행 부분 수정 (before : 명령어 전체 수행 | After : <,>,|에 따라 명령어 부분실행)
 * 
 * CODE_FIX_@
 * 2023.03.18, ICT융합학부, 2019043118, 김건하
 * 수정내용 : 
 * 1. redirectIn, redirectOut, createPipe 주석 추가
 * 
 * 참고 자료 출처
 * https://gist.github.com/tam5/be8e818d4c77dc480451 > redirectIn, redirectOut 코드 인용, createPipe, main 참조
 * https://velog.io/@hidaehyunlee/minishell-5.-%ED%8C%8C%EC%9D%B4%ED%94%84Pipe-%EC%B2%98%EB%A6%AC > Dup2(), Pipe 지식 참조
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80             /* 명령어의 최대 길이 */
static void cmdexec(char *cmd);
void redirectIn(char **fileName);
void redirectOut(char **fileName);
void runPartOfCmd(char **argv);
void createPipe(char **argv);

/*
* redirectIn - 파일을 오픈해서 파일 내용을 표준입력으로 받는다.
*/
void redirectIn(char **fileName)
{
	int in = open(*fileName, O_RDONLY); // 파일 열기 - 옵션 : 읽기전용
	dup2(in, STDIN_FILENO); //표준 입력 받기
	close(in); // 파일 닫기
}

/*
* redirectOut - 파일을 오픈해서 파일에 표준출력을 기록한다.
* 파일이 이미 존재할 시 파일 내용을 초기화 한다.
* 파일이 존재하지 않을 시 파일을 새로 생성하고 권한을 부여한다.
*/
void redirectOut(char **fileName)
{
	int out = open(*fileName, O_WRONLY | O_TRUNC | O_CREAT, 0600); // 파일 오픈 - 옵션 : 쓰기전용 | 파일 존재시 내부 내용 초기화 | 파일 미존재시 생성, 생성시 파일 권한
	dup2(out, STDOUT_FILENO); //표준 출력 받기
	close(out); // 파일 닫기
}

/*
* runPartCmd - 인수로 받은 명령어를 실행한다.
*/
void runPartCmd(char **argv)
{
	execvp(argv[0], argv);
}

/*
* createPipe - 파이프를 생성하여, 인수로 받은 명령어를 표준 입력으로 바꿔준다.
*/
void createPipe(char **argv)
{
	int fd[2];
	pid_t pid;

	if(pipe(fd) == -1) // 파이프 호풀. 2개의 fd 배열을 채운다.
	{
		exit(1); //error일 시 종료한다.
	}
	pid = fork(); //fork()
	if(pid == -1) //error일 시 종료한다.
	{
		exit(1);
	}
	else if(pid == 0) //자식프로세스 
	{
		dup2(fd[1], STDOUT_FILENO); // 표준 출력을 준비한다.
		close(fd[0]);

		runPartCmd(argv); // 자식 프로세스 - 현재 파이프를 기준으로 좌측 명령어들 실행
	}
	else // 부모프로세스
	{
		wait(NULL); //아무 자식 프로세스가 종료하기를 기다린다.

		dup2(fd[0], STDIN_FILENO); // 부모 프로세스 - 현재 파이프를 기준으로 자식 프로세스가 실행한 명령어들을 파이프 우측 명령어에 입력
		close(fd[1]);
	}

}

/*
 * cmdexec - 명령어를 파싱해서 실행한다.
 * 스페이스와 탭을 공백문자로 간주하고, 연속된 공백문자는 하나의 공백문자로 축소한다.
 * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
 * 기호 '<' 또는 '>'를 사용하여 표준 입출력을 파일로 바꾸거나,
 * 기호 '|'를 사용하여 파이프 명령을 실행하는 것도 여기에서 처리한다.
 */
static void cmdexec(char *cmd)
{
    char *argv[MAX_LINE/2+1];   /* 명령어 인자를 저장하기 위한 배열 */
    int argc = 0;               /* 인자의 개수 */
    char *p, *q;                /* 명령어를 파싱하기 위한 변수 */

    /*
     * 명령어 앞부분 공백문자를 제거하고 인자를 하나씩 꺼내서 argv에 차례로 저장한다.
     * 작은 따옴표나 큰 따옴표로 이루어진 문자열을 하나의 인자로 처리한다.
     */
    p = cmd; p += strspn(p, " \t");
    do {
        /*
         * 공백문자, 큰 따옴표, 작은 따옴표가 있는지 검사한다.
         */ 
        q = strpbrk(p, " \t\'\"");
        /*
         * 공백문자가 있거나 아무 것도 없으면 공백문자까지 또는 전체를 하나의 인자로 처리한다.
         */
        if (q == NULL || *q == ' ' || *q == '\t') {
            q = strsep(&p, " \t");
            if (*q) argv[argc++] = q;
        }
        /*
         * 작은 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 작은 따옴표 위치에서 두 번째 작은 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 작은 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else if (*q == '\'') {
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\'");
            if (*q) argv[argc++] = q;
        }
        /*
         * 큰 따옴표가 있으면 그 위치까지 하나의 인자로 처리하고, 
         * 큰 따옴표 위치에서 두 번째 큰 따옴표 위치까지 다음 인자로 처리한다.
         * 두 번째 큰 따옴표가 없으면 나머지 전체를 인자로 처리한다.
         */
        else {
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
            q = strsep(&p, "\"");
            if (*q) argv[argc++] = q;
        }        
    } while (p);
    argv[argc] = NULL;


    /*
    * argv에 저장된 명령어 인자를 돌며 인자에 따라 명령을 실행, 기록 한다.
    * 리다이렉션의 경우 다음 인자에 대해 명령어를 실행한다.
    * 파이프의 경우 파이프 이전의 명령어를 실행 후 이후 명령어에 표준 입력한다.
    * 나머지 명령어들은 추후 명령 실행을 위해 기록한다.
    */
    char **arg = argv;
    int i = 0;
    while(*arg)
    {
	    if(**arg == '<')
	    {
		    redirectIn(++arg);
	    }
	    else if(**arg == '>')
	    {
		    redirectOut(++arg);
	    }
	    else if(**arg == '|')
	    {
		    argv[i] = NULL;
		    createPipe(argv);
		    i = 0;
	    }
	    else
	    {
		    argv[i] = *arg;
		    i++;
	    }
	    ++arg;
    }
    argv[i] = NULL;
    runPartCmd(argv);

}

/*
 * 기능이 간단한 유닉스 셸인 tsh (tiny shell)의 메인 함수이다.
 * tsh은 프로세스 생성과 파이프를 통한 프로세스간 통신을 학습하기 위한 것으로
 * 백그라운드 실행, 파이프 명령, 표준 입출력 리다이렉션 일부만 지원한다.
 */
int main(void)
{
    char cmd[MAX_LINE+1];       /* 명령어를 저장하기 위한 버퍼 */
    int len;                    /* 입력된 명령어의 길이 */
    pid_t pid;                  /* 자식 프로세스 아이디 */
    int background;             /* 백그라운드 실행 유무 */
    
    /*
     * 종료 명령인 "exit"이 입력될 때까지 루프를 무한 반복한다.
     */
    while (true) {
        /*
         * 좀비 (자식)프로세스가 있으면 제거한다.
	 * waitpid(anychild = -1, NULL, WNOHANG = Non-Blocking. who dont wait me)
         */
        pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0)
            printf("[%d] + done\n", pid);
        /*
         * 셸 프롬프트를 출력한다. 지연 출력을 방지하기 위해 출력버퍼를 강제로 비운다.
         */
        printf("tsh> "); fflush(stdout);
        /*
         * 표준 입력장치로부터 최대 MAX_LINE까지 명령어를 입력 받는다.
         * 입력된 명령어 끝에 있는 새줄문자를 널문자로 바꿔 C 문자열로 만든다.
         * 입력된 값이 없으면 새 명령어를 받기 위해 루프의 처음으로 간다.
         */
        len = read(STDIN_FILENO, cmd, MAX_LINE);
        if (len == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        cmd[--len] = '\0';
        if (len == 0)
            continue;
        /*
         * 종료 명령이면 루프를 빠져나간다.
         */
        if(!strcasecmp(cmd, "exit"))
            break;
        /*
         * 백그라운드 명령인지 확인하고, '&' 기호를 삭제한다.
         */
        char *p = strchr(cmd, '&');
        if (p != NULL) {
            background = 1;
            *p = '\0';
        }
        else
            background = 0;
        /*
         * 자식 프로세스를 생성하여 입력된 명령어를 실행하게 한다.
         */
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        /*
         * 자식 프로세스는 명령어를 실행하고 종료한다.
         */
        else if (pid == 0) {
            cmdexec(cmd);
            exit(EXIT_SUCCESS);
        }
        /*
         * 포그라운드 실행이면 부모 프로세스는 자식이 끝날 때까지 기다린다.
         * 백그라운드 실행이면 기다리지 않고 다음 명령어를 입력받기 위해 루프의 처음으로 간다.
         */
        else if (!background)
            waitpid(pid, NULL, 0);
    }
    return 0;
}
