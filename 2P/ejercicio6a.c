/**
 * @brief Ejercicio 6a
 *
 * Este fichero contiene el código fuente del ejercicio 6a de la entrega.
 * @file ejercicio6a.c
 * @author Rafael Sánchez & Sergio Galán
 * @version 1.0
 * @date 06-04-2018
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include "mysignal.h"

#define NUM_PROC 5 /*!< Numero de iteraciones del contador*/
#define SECS 40 /*!< Numero de segundos*/

int main (void){
    sigset_t set, unset, oset;
    int pid, counter;
    sigemptyset(&set);
    sigemptyset(&unset);
    sigaddset_var(&set, SIGUSR1, SIGUSR2, SIGALRM, -1);
    sigaddset_var(&unset, SIGALRM, SIGUSR1, -1);
    pid = fork();
    if (pid == 0){
        if(alarm(SECS)){
            printf("Error al crear la alarma: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        while(1){
            if(sigprocmask(SIG_BLOCK, &set, &oset)){
                printf("Error al aplicar la máscara: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
            for (counter = 0; counter < NUM_PROC; counter++){
                printf("%d\n", counter);
                sleep(1);
            }
            if(sigprocmask(SIG_UNBLOCK, &unset, &oset)){
                printf("Error al aplicar la máscara: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
        sleep(3);
        }
    }
    while(wait(NULL)!=-1);
}
