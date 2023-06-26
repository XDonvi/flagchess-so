#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <unistd.h>
#include "util.h"

/*Data una stringa ne estrae il numero*/

void load_info(int* parametri){
	FILE* file = fopen("./config/config.txt", "r");
	char line[30];
	char* temp;
	char separatore[2] = ";";
	int counter=0;
	while(fgets(line,30,file)!=NULL){
		temp = strtok(line,separatore);
		parametri[counter] = atoi(strtok(NULL,separatore));
		counter++;
	}
	fclose(file);
}


/*Stampa a video lo stato del gioco*/

void stampaStatoDelGioco(int idShm,int idSharedMosse, int* punteggioGiocatori){


	/*Variabili per i parametri di gioco*/
	
	int parametri[10];
	int SO_NUM_G; /*numero di processi giocatore*/
	int SO_NUM_P; /*processi pedina per ogni giocatore*/
	int SO_MAX_TIME; /*durata massima di un round*/
	int SO_BASE; /*dimensioni base matrice*/
	int SO_ALTEZZA; /*dimensione altezza matrice*/
	int SO_FLAG_MIN; /*numero minimo di bandierine posizionate per round*/
	int SO_FLAG_MAX; /*numero massimo di bandierine posizionate per round*/
	int SO_ROUND_SCORE; /*punteggio totale assegnato per round alle varie bandierine*/
	int SO_N_MOVES; /*numero totale di mosse a disposizione delle pedine (vale per tutto il gioco, non per ogni round)*/
	int SO_MIN_HOLD_NSEC; /*numero minimo di nanosecondi di occupazione di una cella da parte di una pedina*/
	int i,j,temp;


	/*Variabili per shared memory della scacchiera*/
	
	int idSharedMemoryScacchiera;
	strutturaCellaScacchiera* myScacchiera;
	
	
	/*Variabili per la shared memory delle mosse residue*/
	
	int idSharedMemoryMosse;
	int* mosseGiocatori;


	/*Recupero i parametri di configurazione*/
	
	load_info(parametri);
	SO_NUM_G = parametri[0];
	SO_NUM_P = parametri[1];
	SO_MAX_TIME = parametri[2];
	SO_BASE = parametri[3];
	SO_ALTEZZA = parametri[4];
	SO_FLAG_MIN = parametri[5];
	SO_FLAG_MAX = parametri[6];
	SO_ROUND_SCORE = parametri[7];
	SO_N_MOVES = parametri[8];
	SO_MIN_HOLD_NSEC = parametri[9];
	
	
	/*Attach delle shared*/
	
	idSharedMemoryScacchiera = idShm;
	myScacchiera = shmat(idSharedMemoryScacchiera,NULL,0);
	idSharedMemoryMosse = idSharedMosse;
	mosseGiocatori = shmat(idSharedMemoryMosse,NULL,0);
	

	printf("\n");
	printf("######### STATO DEL GIOCO #########\n");
	printf("N = Pedina generata dal giocatore N\n");
	printf("x = bandierina\n\n");

	for(i=0;i<SO_NUM_G;i++){
		temp=0;
  		for(j=0;j<SO_NUM_P;j++){
  			temp =  temp + mosseGiocatori[i*SO_NUM_P+j];
  		}
  		printf("Mosse residue giocatore %d: %d\n",i,temp);
  	}
  	printf("\n");
  	for(i=0;i<SO_NUM_G;i++){
  		printf("Punteggio giocatore %d: %d\n",i,punteggioGiocatori[i]);
  	}

	for(i=0;i<SO_ALTEZZA;i++){
		printf("\n");
  		for(j=0;j<SO_BASE;j++){
  			if(myScacchiera[i*SO_BASE+j].pidPedina!=-1 && myScacchiera[i*SO_BASE+j].padrePedina!=-1){
  				printf("[%d]",myScacchiera[i*SO_BASE+j].padrePedina);
  			}else if(myScacchiera[i*SO_BASE+j].bandierina!=-1){
  				printf("[x]");
  			}else{
  				printf("[ ]");
  			}
  		}
  	}
  	printf("\n\n");
}





void stampaMetriche(int contatore, int idSharedMemoryMosse, int SO_N_MOVES, int SO_ROUND_SCORE, int* punteggioGiocatori, double time_spent, int SO_NUM_G,int SO_NUM_P){

	/*Variabili per la shared memory delle mosse residue*/
	
	int* mosseGiocatori;
	int i,j,k,temp;
	double mosseUtilizzate, mosseDisponibili,puntiOttenuti,rapporto;

	mosseGiocatori = shmat(idSharedMemoryMosse,NULL,0);

	printf("######### METRICHE #########\n\n");
	printf("Numero di round giocati: %d\n", contatore);
	
	
	/*rapporto di mosse utilizzate e mosse totali per ciascun giocatore*/
	
	printf("Rapporto di mosse utilizzate e mosse totali per ciascun giocatore\n");
	for(i=0;i<SO_NUM_G;i++){
		temp=0;
  		for(j=0;j<SO_NUM_P;j++){
  			temp =  temp + mosseGiocatori[i*SO_NUM_P+j];

  		}
  		mosseDisponibili=SO_N_MOVES*SO_NUM_P;
  		mosseUtilizzate=(SO_N_MOVES*SO_NUM_P-temp);
  		rapporto=(double)(SO_N_MOVES*SO_NUM_P-temp)/ (double)(SO_N_MOVES*SO_NUM_P);
  		printf("  Giocatore %d -> Mosse totali a disposizione: %f, Mosse utilizzate: %f rapporto = %f\n",i,mosseDisponibili,mosseUtilizzate,rapporto);
  	}
	
	
  	/*rapporto di punti ottenuti e mosse utilizzate per ogni giocatore*/
	
  	printf("Rapporto di punti ottenuti e mosse utilizzate\n");
  	for(i=0;i<SO_NUM_G;i++){
  		temp=0;
  		for(j=0;j<SO_NUM_P;j++){
  			temp =  temp + mosseGiocatori[i*SO_NUM_P+j];
  		}
  		puntiOttenuti=punteggioGiocatori[i];
  		mosseUtilizzate=(SO_N_MOVES*SO_NUM_P-temp);
  		rapporto=(double)punteggioGiocatori[i]/ (double)(SO_N_MOVES*SO_NUM_P-temp);
  		printf("  Giocatore %d -> Punti ottenuti: %f Mosse utilizzate: %f rapporto = %f\n", i,puntiOttenuti,mosseUtilizzate,rapporto);
  	}
	
	
  	/*rapporto di punti totali (sommati fra tutti i giocatori) e tempo totale di gioco*/
	
  	printf("Rapporto di punti totali (sommati fra tutti i giocatori) e tempo totale di gioco\n");
  	k=0;
  	for(i=0;i<SO_NUM_G;i++){
  		k=k+punteggioGiocatori[i];
  	}

  	printf("  Punti totali: %d Tempo di gioco: %f rapporto = %f\n",k,time_spent,(double)k/time_spent);

  	printf("\n\n");

}


void stampaStatoDeiSemaforiScacchiera(int idShm){


	/*Variabili per i parametri di gioco*/
	
	int parametri[10];
	int SO_NUM_G; /*numero di processi giocatore*/
	int SO_NUM_P; /*processi pedina per ogni giocatore*/
	int SO_MAX_TIME; /*durata massima di un round*/
	int SO_BASE; /*dimensioni base matrice*/
	int SO_ALTEZZA; /*dimensione altezza matrice*/
	int SO_FLAG_MIN; /*numero minimo di bandierine posizionate per round*/
	int SO_FLAG_MAX; /*numero massimo di bandierine posizionate per round*/
	int SO_ROUND_SCORE; /*punteggio totale assegnato per round alle varie bandierine*/
	int SO_N_MOVES; /*numero totale di mosse a disposizione delle pedine (vale per tutto il gioco, non per ogni round)*/
	int SO_MIN_HOLD_NSEC; /*numero minimo di nanosecondi di occupazione di una cella da parte di una pedina*/
	int i,j;
	
	
	/*Variabili per shared memory dei semafori*/
	
	int idSharedMemorySemafori;
	sharedSemaphores* mySemaphores;


	/*### Recupero i parametri di configurazione*/
	
	load_info(parametri);
	SO_NUM_G = parametri[0];
	SO_NUM_P = parametri[1];
	SO_MAX_TIME = parametri[2];
	SO_BASE = parametri[3];
	SO_ALTEZZA = parametri[4];
	SO_FLAG_MIN = parametri[5];
	SO_FLAG_MAX = parametri[6];
	SO_ROUND_SCORE = parametri[7];
	SO_N_MOVES = parametri[8];
	SO_MIN_HOLD_NSEC = parametri[9];
	
	
	/*Attach della shared*/
	
	idSharedMemorySemafori= idShm;
	mySemaphores = (sharedSemaphores*)shmat(idSharedMemorySemafori,NULL,0);
	

	printf("\n");
	printf("######### STATO DEI SEMAFORI #########\n");
	printf("S = SEMAFORO BLOCCATO\n");

	for(i=0;i<SO_ALTEZZA;i++){
		printf("\n");
  		for(j=0;j<SO_BASE;j++){
  			if( semctl(mySemaphores->setSemaforiScacchiera, (i*SO_BASE+j), GETVAL) != 0)  {
  				printf("[S]");
  			}else{
  				printf("[ ]");
  			}	
  		}
  	}
  	printf("\n");

}


/*Restituisce un numero random nel range base-max*/

int getRandomNumber(int base ,int max){
	return (rand() % (max + 1 - base) + base);
}

