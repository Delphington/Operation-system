#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define SLEEP_DURATION 10
#define PROGRAM_NAME "[myfork]"
static pid_t child_pid = 0;

void print_pid_at_exit(void){
    printf("%s: process %d exits\n", PROGRAM_NAME, getpid());
}

// Обработчик сигнала SIGINT
void catch_sigint(int sig) {
    (void)sig; // Подавляем предупреждение о неиспользуемом параметре
    printf("%s: process %d interrupted with SIGINT signal! Abort\n", 
           PROGRAM_NAME, getpid()); 
    
    if (child_pid > 0) {
        if (kill(child_pid, SIGINT) == -1) {
            perror("kill");
        }
    }
    exit(EXIT_FAILURE);
}

// Обработчик сигнала SIGTERM
void catch_sigterm(int sig){
    (void)sig; // Подавляем предупреждение о неиспользуемом параметре
    printf("%s: process %d interrupted with SIGTERM signal! Abort\n", 
           PROGRAM_NAME, getpid()); 
    
    if (child_pid > 0) {
        if (kill(child_pid, SIGTERM) == -1) {
            perror("kill");
        }
    }
    exit(EXIT_FAILURE);
}

// Установка обработчика сигнала
int setup_signal_handler(int sig, void (*handler)(int)){
    struct sigaction sa;
    
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(sig, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    return 0;
}


int main(int argc, char **argv){
    (void)argc;
    (void)argv;
    
    int wstatus;
    pid_t fork_result;

    if (atexit(print_pid_at_exit) != 0) {
        perror("atexit");
        exit(EXIT_FAILURE);
    }

    // Настраиваем обработчики сигналов
    if (setup_signal_handler(SIGTERM, catch_sigterm) == -1) {
        exit(EXIT_FAILURE);
    }
    
    if (setup_signal_handler(SIGINT, catch_sigint) == -1) {
        exit(EXIT_FAILURE);
    }

    // Создаем дочерний процесс
    fork_result = fork();
    if (fork_result == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (fork_result == 0)  {
        printf("%s: child process id - %d\n", PROGRAM_NAME, getpid());
        printf("%s: parent process id - %d\n", PROGRAM_NAME, getppid());
        printf("%s: child will be sleeping for %d seconds\n", 
               PROGRAM_NAME, SLEEP_DURATION);
        
        if (sleep(SLEEP_DURATION) > 0)  {
            // sleep был прерван сигналом
            printf("%s: child sleep was interrupted\n", PROGRAM_NAME);
            exit(EXIT_FAILURE);
        }
        
        printf("%s: child finished sleeping\n", PROGRAM_NAME);
        exit(EXIT_SUCCESS);
    }
    else {
        child_pid = fork_result;
        
        printf("%s: parent process id - %d\n", PROGRAM_NAME, getpid());
        printf("%s: child process id - %d\n", PROGRAM_NAME, child_pid);

        // Ждем завершения дочернего процесса
        if (wait(&wstatus) == -1) {
            perror("wait");
            exit(EXIT_FAILURE);
        }
        
        if (WIFEXITED(wstatus)) {
            printf("%s: child exited normally with code %d\n", 
                   PROGRAM_NAME, WEXITSTATUS(wstatus));
        }
        else if (WIFSIGNALED(wstatus)) {
            printf("%s: child was terminated by signal %d\n", 
                   PROGRAM_NAME, WTERMSIG(wstatus));
        }
        else {
            printf("%s: child terminated abnormally\n", PROGRAM_NAME);
        }
    }
    return EXIT_SUCCESS;
}
