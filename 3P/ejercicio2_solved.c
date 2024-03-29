/**
 * @brief Ejercicio 2 Solved
 *
 * Este fichero contiene el código fuente del ejercicio 2 de la entrega.
 * @file ejercicio2_solved.c
 * @author Rafael Sánchez & Sergio Galán
 * @version 1.0
 * @date 14-04-2018
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/sem.h>
#include <ctype.h>

#include "mylib.h"
#include "semaforos.h"
#include "ejercicio2_lib.h"

/**
 * @brief Manejador de SIGUSR1
 *
 * Imprime el nombre y el id del usuario en la memoria compartida a la recepcion
 * de SIGUSR1. Levanta el semaforo.
 */
void handle_SIGUSR1(int sig);

/**
 * @brief Rutina que sigue el proceso hijo
 *
 * Duerme al comienzo, reserva el recurso del input y la memoria, lee de teclado
 * y envía SIGUSR1 al padre.
 */
void rutina_hijo();

Info *buff = NULL; /*!< Variable global donde se guarda la informacion de usuario*/
int semshm; /*!< Semaforo para proteger el recurso*/

int main(int argc, char const *argv[]) {
    int mem, n, i;
    pid_t pid;
    key_t key1, key2, key3;
    unsigned short val[1];

    if(argc != 2){
        usage();
        exit(EXIT_FAILURE);
    }

    if(!aredigits(argv[1])){
        usage();
        exit(EXIT_FAILURE);
    }

    n = atoi(argv[1]);

    if((key1 = ftok(PATH, KEY1)) == -1){
        perror("Fallo ftok");
        exit(EXIT_FAILURE);
    }

    if((key2 = ftok(PATH, KEY2)) == -1){
        perror("Fallo ftok");
        exit(EXIT_FAILURE);
    }

    if((key3 = ftok(PATH, KEY3)) == -1){
        perror("Fallo ftok");
        exit(EXIT_FAILURE);
    }

    if(ERROR == crear_semaforo(key2, 1, &semshm)){
        perror("Error creando semaforo");
        exit(EXIT_FAILURE);
    }

    mem = shmget(key3, sizeof(Info), SHM_W|SHM_R|IPC_CREAT);
    if(mem == -1){
        perror("Error generando memoria compartida");
        borrar_semaforo(semshm);
        exit(EXIT_FAILURE);
    }

    buff = shmat(mem, NULL, 0);
    if(buff == (void *) -1){
        perror("Error añadiendo la memoria");
        borrar_semaforo(semshm);
        shmctl(mem, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    buff->id = 0;

    val[0] = 1;
    if(ERROR == inicializar_semaforo(semshm, val)){
        perror("Error al inicializar el semaforo");
    }

    if(signal(SIGUSR1, handle_SIGUSR1) == SIG_ERR){
        perror("Error asignando el manejador");
        borrar_semaforo(semshm);
        shmdt(buff);
        shmctl(mem, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    for(i = 0; i < n; i++){
        pid = fork();
        if(!pid){
            rutina_hijo();
            exit(EXIT_FAILURE);
        }
    }
    while(1){
        if(buff->id == n){
            break;
        }
        pause();
    }
    while(wait(NULL) != -1);
    shmdt(buff);
    shmctl(mem, IPC_RMID, NULL);
    borrar_semaforo(semshm);
    exit(EXIT_SUCCESS);
}


void handle_SIGUSR1(int sig){
    printf("Nombre: %s - Id: %d\n", buff->nombre, buff->id);
    if(ERROR == up_semaforo(semshm, 0, 0)){
        perror("Error al hacer up a semshm");
    }
}

void rutina_hijo(){
    sleep((int) randNum(1, 6));
    if (ERROR == down_semaforo(semshm, 0, 0)){
        perror("Error al bajar el semaforo semshm");
    }
    printf("Soy %d y voy a leer tu nombre:\n", getpid());
    fgets(buff->nombre, MAX_NOMBRE, stdin);
    buff->nombre[strcspn(buff->nombre, "\n")] = 0;
    buff->id++;
    kill(getppid(), SIGUSR1);
    shmdt(buff);
    exit(EXIT_SUCCESS);
}
