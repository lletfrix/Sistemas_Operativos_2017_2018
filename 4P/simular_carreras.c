/**
 * @brief Rutina que simula la carrera
 *
 * Este fichero contiene el código fuente de la simulación de la carrera
 * @file simular_carreras.c
 * @author Rafael Sánchez & Sergio Galán
 * @version 1.0
 * @date 09-05-2018
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#include "sim_carr_lib.h"
#include "mylib.h"
#include "semaforos.h"
#include "caballo.h"
#include "apostador.h"
#include "apuesta.h"
#include "rutina_monitor.h"
#include "rutina_gestor.h"
#include "rutina_tirada.h"
#include "rutina_apostador.h"

#define NUM_ARGS 6 /*!< Numero de argumentos de entrada*/

/**
 * @brief Función que muestra el uso del programa y los parámetros de entrada
 *
 * Imprime por pantalla el uso y los parámetros de entrada de la función
 */
void usage();
/**
 * @brief Función que envía una señal a todos los hijos
 *
 * Envía la señal elegida a todos sus hijos
 *
 * @param sig Señal que queremos mandar a todos los hijos
 */
void _killall(int sig);
/**
 * @brief Manejador de señales de la simulación de la carrera
 *
 * Establece las acciones que deberá ejecutar el proceso cuando reciba ciertas señales
 *
 * @param sig Señal recibida
 */
void _sim_handler(int sig);

volatile bool running_principal = true; /*!< Bandera que indica si el proceso sigue ejecutando la lógica de tiradas*/
Caballo *caballos; /*!< Puntero al array de caballos*/
Apostador *apostadores; /*!< Puntero al array de apostadores*/
pid_t gestor; /*!< Id de proceso del gestor de apuestas*/
pid_t monitor; /*!< Id de proceso del monitor*/

/**
 * @brief main
 *
 * Programa principal del simulador de carreras.
 *
 * @param argc Número de argumentos de entrada
 * @param argv Argumentos de entrada
 * @return Valor de salida del programa
 */
int main(int argc, char* argv[]) {
    int i, longitud, pid_aux, max_pos, min_pos, pos_aux;
    int semid_mon, semid_turno, semid_cab, semid_gen;
    int qid_apues, qid_tir;
    int shmid_cab, shmid_apos;
    unsigned short sem_initial_val, *sem_cab_init;
    int fd[MAX_CAB][2] = {0};
    int *active;
    char tirada_type;
    struct msgtir mensaje_tirada;
    sigset_t mask;

    /* Comprobacion de parametros de entrada*/
    if(argc != NUM_ARGS){
        usage();
        exit(EXIT_FAILURE);
    }

    for(i = 1; i < NUM_ARGS-1; i++){
        if(!aredigits(argv[i])){
            usage();
            exit(EXIT_FAILURE);
        }
    }

    if(!isfloat(argv[NUM_ARGS-1])){
        usage();
        exit(EXIT_FAILURE);
    }

    n_cab = atoi(argv[1]);
    longitud = atoi(argv[2]);
    n_apos = atoi(argv[3]);
    n_vent = atoi(argv[4]);
    din = atof(argv[5]);

    if(n_cab > MAX_CAB || n_apos > MAX_APOS || n_cab <= 0 || n_apos <= 0 || longitud <= 0 || n_vent <= 0 || din <= 0.0){
        usage();
        exit(EXIT_FAILURE);
    }
    num_proc = n_cab+n_apos+2;

    /* Resets necessary file */
    fclose(fopen(RUTA_FICHERO_APUESTAS,"w"));
    /* Reserva de IPCS */
    //TODO: Añadir liberacion de recursos al control de errores
    if(crear_semaforo(ftok(PATH, KEY_MON_SEM), 1, &semid_mon) == ERROR){
        perror("Error al crear el semaforo monitor");
        exit(EXIT_FAILURE);
    }
    if(crear_semaforo(ftok(PATH, KEY_TUR_SEM), 1, &semid_turno) == ERROR){
        perror("Error al crear el semaforo turno");
        borrar_semaforo(semid_mon);
        exit(EXIT_FAILURE);
    }
    if(crear_semaforo(ftok(PATH, KEY_CAB_SEM), n_cab, &semid_cab) == ERROR){
        perror("Error al crear el semaforo caballo");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        exit(EXIT_FAILURE);
    }
    if(crear_semaforo(ftok(PATH, KEY_GEN_SEM), num_proc, &semid_gen) == ERROR){
        perror("Error al crear el semaforo general");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        exit(EXIT_FAILURE);
    }

    sem_initial_val = 0;
    if(inicializar_semaforo(semid_mon, &sem_initial_val) == ERROR){
        perror("Error al inicializar el semaforo monitor");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        exit(EXIT_FAILURE);
    }
    sem_initial_val = 1;
    if(inicializar_semaforo(semid_turno, &sem_initial_val) == ERROR){
        perror("Error al inicializar el semaforo turno");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        exit(EXIT_FAILURE);
    }
    sem_cab_init = calloc(n_cab, sizeof(unsigned short));
    if(!sem_cab_init){
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < n_cab; ++i) {
        sem_cab_init[i] = 1;
    }
    if(inicializar_semaforo(semid_cab, sem_cab_init) == ERROR){
        perror("Error al inicializar el semaforo caballo mutex");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        free(sem_cab_init);
        exit(EXIT_FAILURE);
    }
    free(sem_cab_init);

    if((qid_apues = msgget(ftok(PATH, KEY_APUES_Q), IPC_CREAT|0600)) == -1){
        perror("Error al crear la cola de mensajes APOSTADOR-GESTOR");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        exit(EXIT_FAILURE);
    }
    if((qid_tir = msgget(ftok(PATH, KEY_TIR_Q), IPC_CREAT|0600)) == -1){
        perror("Error al crear la cola de mensajes CABALLO-MAIN");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        msgctl(qid_apues, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    if((shmid_cab = shmget(ftok(PATH, KEY_CAB_SHM), n_cab*sizeof(Caballo), SHM_W|SHM_R|IPC_CREAT|0600)) == -1){
        perror("Error al crear la memoria compartida de los caballos");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        msgctl(qid_apues, IPC_RMID, NULL);
        msgctl(qid_tir, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    if((shmid_apos = shmget(ftok(PATH, KEY_APOS_SHM), n_apos*sizeof(Apostador), SHM_W|SHM_R|IPC_CREAT|0600)) == -1){
        perror("Error al crear la memoria compartida de los apostadores");
        borrar_semaforo(semid_mon);
        borrar_semaforo(semid_cab);
        borrar_semaforo(semid_gen);
        msgctl(qid_apues, IPC_RMID, NULL);
        msgctl(qid_tir, IPC_RMID, NULL);
        shmctl(shmid_cab, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    for(i = 0; i < n_cab; ++i){
        if(pipe(fd[i]) == -1){
            perror("Error al crear una pipe");
            borrar_semaforo(semid_mon);
            borrar_semaforo(semid_cab);
            borrar_semaforo(semid_gen);
            msgctl(qid_apues, IPC_RMID, NULL);
            msgctl(qid_tir, IPC_RMID, NULL);
            shmctl(shmid_cab, IPC_RMID, NULL);
            shmctl(shmid_apos, IPC_RMID, NULL);
            exit(EXIT_FAILURE);
        }
    }

    caballos = shmat(shmid_cab, NULL, 0);
    apostadores = shmat(shmid_apos, NULL, 0);

    /* Creacion de procesos hijo */
    /*
       A partir de aqui se deja de tener un control exhaustivo de errores.
       Habría que liberar recursos y enviar una señal a cada hijo, cosa
       que no consideramos que es el objetivo de la practica
    */

    /* Crea el monitor */
    if((monitor = fork()) == 0){
        proc_monitor();
        exit(EXIT_FAILURE);
    }
    /* Crea el gestor de apuestas */
    if((gestor = fork()) == 0){
        proc_gestor();
        exit(EXIT_FAILURE);
    }
    /* Crea los procesos apostadores */
    for(i = 0; i < n_apos; ++i){
        pid_aux = fork();
        if(!pid_aux){
            proc_apostador(i);
            exit(EXIT_FAILURE);
        }
        apos_set_pid(&apostadores[i], pid_aux);
    }
    /* Crea los procesos de tirada */
    for(i = 0; i < n_cab; ++i){
        pid_aux = fork();
        if(!pid_aux){
            proc_tirada(i, fd[i]);
            exit(EXIT_FAILURE);
        }
        cab_set_pid(&caballos[i], pid_aux);
        cab_set_id(&caballos[i], i);
    }

    /* Establece la máscara de señales */
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    signal(SIGINT, _sim_handler);

    active = calloc(num_proc, sizeof(int));
    for (i = 0; i < num_proc; i++) {
        active[i] = i;
    }
    /* Hace que los hijos comiencen su ejecucion */
    up_multiple_semaforo(semid_gen, num_proc, 0, active);
    free(active);

    /* Espera a que empiece la carrera */
    sleep(TIEMPO_PRE_CARR);

    _killall(SIGSTART);

    for (i = 0;  i < n_cab; ++i){
        tirada_type = NORMAL;
        write(fd[i][WRITE], &tirada_type, sizeof(char));
    }
    sleep(1);
    max_pos = 0;
    /* Ejecuta su parte de la logica de la carrera */
    while(max_pos < longitud && running_principal){
        min_pos = longitud;
        for(i = 0; i < n_cab; ++i){
            kill(cab_get_pid(&caballos[i]), SIGTHROW);
        }
        /* Actualiza la posicion de cada caballo junto a la posicion maxima y minima */
        down_semaforo(semid_turno, 0, 0);
        for(i = 0; i < n_cab; ++i){
            msgrcv(qid_tir, (struct msgbuf *) &mensaje_tirada, sizeof(struct msgtir) - sizeof(long),0, 0);
            pos_aux = cab_get_pos(&caballos[mensaje_tirada.mtype-1]) + mensaje_tirada.tirada;
            cab_set_last_tir(&caballos[mensaje_tirada.mtype-1], mensaje_tirada.tirada);
            cab_set_pos(&caballos[mensaje_tirada.mtype-1], pos_aux);
            if(pos_aux > max_pos){
                max_pos = pos_aux;
            }
            if(pos_aux < min_pos){
                min_pos = pos_aux;
            }
        }
        up_semaforo(semid_mon, 0, 0);
        /* Escribe el tipo de tiradas en las pipes */
        for (i = 0;  i < n_cab; ++i){
            if(min_pos == max_pos){
                tirada_type = NORMAL;
            }else if(cab_get_pos(&caballos[i]) == min_pos){
                tirada_type = REMONTAR;
            }else if(cab_get_pos(&caballos[i]) == max_pos){
                tirada_type = GANADORA;
            }else{
                tirada_type = NORMAL;
            }
            write(fd[i][WRITE], &tirada_type, sizeof(char));
        }
    }
    /* Notifica del final de la carrera */
    _killall(SIGINT);

    while(wait(NULL) != -1);

    msgctl(qid_apues, IPC_RMID, NULL);
    msgctl(qid_tir, IPC_RMID, NULL);

    borrar_semaforo(semid_cab);
    borrar_semaforo(semid_turno);
    borrar_semaforo(semid_mon);
    borrar_semaforo(semid_gen);

    shmctl(shmid_cab, IPC_RMID, NULL);
    shmctl(shmid_apos, IPC_RMID, NULL);

    exit(EXIT_SUCCESS);
}

void usage(){
    printf("Usage is ./simularCarrera <n_caballos> <longitudcarrera> <n_apostadores> <n_ventanillas> <dineroinicial>\n"\
    "n_caballos no puede ser mayor que 10 y n_apostadores no puede ser mayor que 100.\n");
}

void _sim_handler(int sig){
    switch (sig) {
        case SIGINT:
            _killall(SIGINT);
            running_principal = false;
            break;
        default:
            return;
    }
}

void _killall(int sig){
    int i;
    kill(gestor, sig);
    for(i = 0; i < n_apos; ++i){
        kill(apos_get_pid(&apostadores[i]), sig);
    }
    if(-1 == kill(monitor, sig)){
    }
    for(i = 0; i < n_cab; ++i){
        kill(cab_get_pid(&caballos[i]), sig);
    }
}
