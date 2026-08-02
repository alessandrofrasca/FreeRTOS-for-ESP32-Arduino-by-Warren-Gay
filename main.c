#define nPiloti 10
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct pilota{
    char nome[50];
    double tempo;
}PILOTA;

void generaDati(PILOTA V[], char* nome[]){
    for (int i = 0; i < nPiloti; i++) {
        strcpy(V[i].nome, nome[i]);
        do {
            V[i].tempo = (double)(rand() % 120) + (double)(rand() % 1000) / 1000.0;
        } while (V[i].tempo < 31);
    }
}

void selectionSort(PILOTA V[]){
    for (int i = 0; i<nPiloti-1; i++){
        int k = i;
        for (int j = i+1; j<nPiloti; j++){
            if (V[j].tempo<V[k].tempo){
                k = j;
            }
        }
        PILOTA tmp = V[i];
        V[i] = V[k];
        V[k] = tmp;
    }
}

int generaClassifica(PILOTA V[]){
    // V[] dev'essere già ordinato.
    FILE* fp = fopen("qualifiche.txt", "w+");
    if (fp == NULL){
        printf("Errore nella generazione del file.\n");
        return 1;
    }
    fprintf(fp, "Classifica\nPilota\t\t\tTempo\t\tDistacco\n");
    for (int i=0; i<nPiloti; i++){
        fprintf(fp, V[i].nome);
        fprintf(fp, "\t\t%.3f\t\t%.3f\n", V[i].tempo, (V[i].tempo - V[0].tempo));
    }
    fclose(fp);
    return 0;
}

int main(){
    srand(time(NULL));
    char* nome[] = {"Pilota 1","Pilota 2","Pilota 3","Pilota 4","Pilota 5","Pilota 6","Pilota 7","Pilota 8","Pilota 9","Pilota 10"};
    PILOTA V[nPiloti];
    generaDati(V, nome);
    selectionSort(V);
    generaClassifica(V);
    return 0;
}