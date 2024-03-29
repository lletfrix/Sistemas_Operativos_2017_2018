/**
 * @brief Implementación de las primitivas del TAD apuesta
 *
 * Este fichero contiene el código fuente del TAD apuesta
 * @file apuesta.c
 * @author Rafael Sánchez & Sergio Galán
 * @version 1.0
 * @date 09-05-2018
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>

#include "apostador.h"
#include "caballo.h"
#include "apuesta.h"
#include "semaforos.h"
#include "sim_carr_lib.h"

/**
 * @brief Datos de la apuesta
 *
 * Esta estructura guarda la información de una apuesta.
 */
struct _Apuesta{
    Apostador *apos; /*!< Apostador que realiza la apuesta*/
    Caballo* c; /*!< Caballo por el que se apuesta*/
    int cab_id; /*!< Identificador del caballo*/
    unsigned short ventanilla; /*!< Id de la ventanilla asignada a la apuesta*/
    double cantidad; /*!< Cantidad apostada*/
};

double apuesta_total = 0;/*!< Variable que contiene el total de todas las apuestas realizadas hasta el momento*/

/**
 * @brief Función privada que guarda los datos de una apuesta
 *
 * Imprime los datos de la apuesta en un fichero dado
 *
 * @param fp Fichero en el que escribimos los datos
 * @param a Apuesta de la que queremos guardar los datos
 * @param old_cot Cotización anterior del caballo
 */
void _apuesta_print(FILE *fp, Apuesta *a, double old_cot);

Apuesta *apuesta_new(){
    return calloc(1, sizeof(Apuesta));
}

void apuesta_destroy(Apuesta *a){
    free(a);
}

Apuesta *apuesta_init(Apuesta *a, Apostador *apos, Caballo *c, int cab_id, unsigned short ventanilla, double cantidad){
    if(!a || !apos || !c){
        printf("Voy a devolver apuesta nula\n");
        return NULL;
    }
    a->apos = apos;
    a->c = c;
    a->cab_id = cab_id;
    a->ventanilla = ventanilla;
    a->cantidad = cantidad;
    return a;
}

void apuesta_execute(Apuesta *a, char *path){
    int cab_mutex, i;
    //TODO: Control de errores, Proteger la memoria del caballo (y del apostador?)
    FILE *fp;
    double old_cot;
    int active[] = {0,1,2,3,4,5,6,7,8,9};
    fp = fopen(path, "a");
    crear_semaforo(ftok(PATH, KEY_CAB_SEM), n_cab, &cab_mutex);
    down_multiple_semaforo(cab_mutex, n_cab, 0, active);

    apuesta_total += a->cantidad; //TODO: Mirar si esto va aqui
    old_cot = cab_get_cot(&a->c[a->cab_id]);
    apos_set_ben(a->apos, apos_get_ben(a->apos, a->cab_id) + old_cot*a->cantidad, a->cab_id);
    cab_incr_apostado(&a->c[a->cab_id], a->cantidad);
    for(i = 0; i < n_cab; ++i){
        cab_set_cot(&a->c[i], apuesta_total/cab_get_apostado(&a->c[i]));
    }
    up_multiple_semaforo(cab_mutex, n_cab, 0, active);
    _apuesta_print(fp, a, old_cot);
    fclose(fp);
}

void _apuesta_print(FILE *fp, Apuesta *a, double old_cot){
    if(!fp || !a){
        return;
    }
    fprintf(fp, "Apostador: %s, Ventanilla: %hu, Caballo: %hu, Cotizacion: %lf, Cantidad %lf\n", apos_get_name(a->apos),a->ventanilla+1, cab_get_id(&a->c[a->cab_id])+1, old_cot, a->cantidad);
}
