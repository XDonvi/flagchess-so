#define _GNU_SOURCE
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
#include <time.h>
#include <sys/signal.h>
#include <unistd.h> /*per alarm*/
#include "util.h"


void alarmEndGame(int); /*Allarme timer*/
int continua=1;
int semId;

int main(int argc, char const *argv[]){
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
	/*Variabili per shared memory della scacchiera*/
	int idSharedMemoryScacchiera;
	strutturaCellaScacchiera* myScacchiera;
	/*Variabili per la shared memory dei semafori*/
	int idSharedMemorySemafori;
	sharedSemaphores* mySemaphores;
	int semaphoreID;
	struct sembuf waitForZeroOperation;
	struct sembuf abbassaSemaforo;
	struct sembuf alzaSemaforo;
	/*Variabili della shared memory delle mosse residue*/
	int idSharedMemoryMosse;
	int* mosseGiocatori;
	/*Variabili per la gestione dei processi*/
	pid_t* pidGiocatori; /*Vettore che contiene i pid dei giocatori*/
	char* args[6]; /*vettore per passare parametri ai giocatori*/
	pid_t wpid; /*Per l'attesa finale che muoiano tutti i figli*/
	/*Variabili di appoggio*/
	int i,j,k,temp,i_index,j_index,exit_cond,punteggio,numBandierineUtilizzate;
	/*Utilizzo queste stringhe per passare parametri nella execve*/
	char idSharedMemoryScacchieraStr[1000];
	char idSharedMemorySemaforiStr[1000];
	char idSharedMemoryMosseStr[1000];
	char indiceNelSemaforoTurnazioneStr[1000];
	/*Altre*/
	int contatore,numRoundFinale=0;
	int* punteggioGiocatori; /*Vettore che contiene i punteggi dei giocatori, l'indice N del vettore è il punteggio del giocatore N*/

	/*Variabili per misurare il tempo totale di gioco*/
	struct timeval begin;
	struct timeval end;
	double time_spent;

	int idRun;
	int* runGiocatori;

	setvbuf(stdout,NULL,_IONBF,0); /*Viene tolta la bufferizzazione per stdout in modo da visualizzare gli eventi in ordine di esecuzione*/

	
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


  	/*Inizializzo il vettore con i punteggi dei giocatori*/
  	punteggioGiocatori = malloc(sizeof(int)*SO_NUM_G);
  	for(i=0;i<SO_NUM_G;i++){punteggioGiocatori[i]=0;}
	
	/*Creazione shared memory scacchiera, usage: myScacchiera[i][j] --> myScacchiera[i*SO_BASE+j]*/
	idSharedMemoryScacchiera = shmget(IPC_PRIVATE, (sizeof(strutturaCellaScacchiera)*SO_BASE)*SO_ALTEZZA, IPC_CREAT | IPC_EXCL | 0666); /*0666 sono i permessi per la lettura e scrittura in shared memory a TUTTI gli utenti del sistema*/
	myScacchiera = shmat(idSharedMemoryScacchiera,NULL,0); /*d'ora in poi uso ptrScacchiera come se fosse una memoria locale*/
	if (idSharedMemoryScacchiera == -1) {printf("Errore in shmget\n");}
  	/*Creazione shared memory semafori*/
  	idSharedMemorySemafori = shmget(IPC_PRIVATE, sizeof(sharedSemaphores), IPC_CREAT | IPC_EXCL | 0666); /*0666 sono i permessi per la lettura e scrittura in shared memory a TUTTI gli utenti del sistema*/
	semId = idSharedMemorySemafori;
	if (idSharedMemorySemafori == -1) {printf("Errore in shmget\n");}
  	mySemaphores = (sharedSemaphores*)shmat(idSharedMemorySemafori,NULL,0); /*d'ora in poi uso ptrShsem come se fosse una memoria locale*/
	/*Creazione shared memory delle mosse residue*/
	idSharedMemoryMosse = shmget(IPC_PRIVATE, (sizeof(int)*SO_NUM_G)*SO_NUM_P, IPC_CREAT | IPC_EXCL | 0666);
	mosseGiocatori = shmat(idSharedMemoryMosse,NULL,0);

	

	/*Inizializzazione della shared memory delle mosse residue*/
	k=0;
	for(i=0;i<SO_NUM_G;i++){
  		for(j=0;j<SO_NUM_P;j++){
  			mosseGiocatori[i*SO_NUM_P+j]=SO_N_MOVES;
  			k++;
  		}
  	}
	/*Inizializzazione scacchiera*/
	for(i=0;i<SO_ALTEZZA;i++){
  		for(j=0;j<SO_BASE;j++){
  			myScacchiera[i*SO_BASE+j].pidPedina=-1; /*Se e'presente una pedina su questa cella qua c'è il PID di quella pedina, altrimenti vale -1*/
  			myScacchiera[i*SO_BASE+j].bandierina=-1;  /*Se e' presente una bandierina in questa posizione c'è 1, altrimenti vale -1*/
  			myScacchiera[i*SO_BASE+j].punteggioBandierina=-1; /*Se e' presente una bandierina in posizione [i][j] qua c'è il punteggio di quella bandierina*/
  		}
  	}

  	/*Inizializzazione semafori*/
  	/*semget mi permette di creare un set di semafori con (,N,) N numero di semafori*/
  	/*semctl mi permette di impostare e leggere il valore del semaforo, l'ultimo parametro è il valore che voglio assegnare, restituisce -1 in caso di errore*/
	/*Semaforo per il piazzamento a turno delle pedine*/
	mySemaphores->setSemaforiPosizionamentoPedine = semget(IPC_PRIVATE, SO_NUM_G, IPC_CREAT | 0600);
	if(semctl(mySemaphores->setSemaforiPosizionamentoPedine, 0, SETVAL, 0) == -1){ /*0 nel semaforo dell'indice "assegnato" al giocatore sbloccato, 1 bloccato*/
		printf("Error in semctl\n");
	}
	for(k=1;k<SO_NUM_G;k++){
		if(semctl(mySemaphores->setSemaforiPosizionamentoPedine, k, SETVAL, 1) == -1){
			printf("Error in semctl\n");
		}   
	}
	/*Semaforo che aspetta che i giocatori abbiano piazzato le loro pedine, si fa una wait for zero su questo semaforo, quando un giocatore ha piazzato tutte le pedine abbassa di 1 il semaforo*/
	mySemaphores->semaforoFineTurnazionePedine = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
	if(semctl(mySemaphores->semaforoFineTurnazionePedine, 0, SETVAL, SO_NUM_G) == -1){
		printf("Error in semctl\n");
	}
	/*Semafori delle celle della scacchiera per accedere al semaforo della cella [i][j] ==> (i*SO_BASE+j) è direttamente l'indice da inserire nella getval*/
	mySemaphores->setSemaforiScacchiera = semget(IPC_PRIVATE, SO_BASE*SO_ALTEZZA , IPC_CREAT | 0600);
	for(k=0;k<SO_BASE*SO_ALTEZZA;k++){
		if(semctl(mySemaphores->setSemaforiScacchiera, k, SETVAL, 0) == -1){ /*0 sbloccato, 1 bloccato*/
			printf("Error in semctl\n");		
		}
	}
	/*Semaforo per dare il via ai giocatori del momento in cui possono piazzare le pedine*/
	mySemaphores->semaforoIndicazioniPedine = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
	if(semctl(mySemaphores->semaforoIndicazioniPedine, 0, SETVAL, 1) == -1){ /*lo metto a uno, i giocatori rimarranno in waitForZero finchè il master non darà il via al round*/
		printf("Error in semctl\n");
	}
	/*L'indice 1 lo metto a SO_NUM_G cosi tutti i giocatori lo abbassano quando hanno finito di dare indicazioni alle pedine*/
	if(semctl(mySemaphores->semaforoIndicazioniPedine, 1, SETVAL, SO_NUM_G) == -1){ 
		printf("Error in semctl\n");
	}
	/*Semaforo che indica l'inizio del gioco, dopo che tutti i giocatori hanno dato le indicazioni alle pedine sblocco il semaforo e lo metto a 0*/
	mySemaphores->gameStart = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
	if(semctl(mySemaphores->gameStart, 0, SETVAL, 1) == -1){
		printf("Error in semctl\n");
	}
	/*Semaforo che indica la fine del round, ogni pedina quando finisce le mosse da fare nel round decrementa di uno, quando il sem vale 0 il master toglie alarm e rimoncia round*/
	mySemaphores->endRound = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
	
	













/*INIZIO DEL GIOCO*/




	printf("[Master] Inizia il gioco...\n");
	printf("[Master] Genero %d processi giocatori e attendo che piazzino le pedine...\n", SO_NUM_G);
	


	/*Generazione dei processi giocatore*/
	pidGiocatori=malloc(sizeof(int)*SO_NUM_G);/*alloco la dimensione del vettore pidGiocatori così che possa contenere il numero di giocatori che ho*/
  	sprintf(idSharedMemoryScacchieraStr, "%d", idSharedMemoryScacchiera);
	sprintf(idSharedMemorySemaforiStr, "%d", idSharedMemorySemafori);
	sprintf(idSharedMemoryMosseStr, "%d", idSharedMemoryMosse);

	for (k=0;k<SO_NUM_G;k++){ 
		/*parametri da passare al figlio*/
		sprintf(indiceNelSemaforoTurnazioneStr,"%d", k);
		args[0] = "giocatori";
		args[1] = idSharedMemoryScacchieraStr;
		args[2] = idSharedMemorySemaforiStr;
		args[3] = indiceNelSemaforoTurnazioneStr;
		args[4] = idSharedMemoryMosseStr;
		args[5] = NULL;

		pidGiocatori[k] = fork();
		switch(pidGiocatori[k]){
			case -1:
				printf("Error in fork\n");
			break;
			case 0:
				execve("./giocatori",args,NULL);
				exit(EXIT_SUCCESS);
			break;
			default:
			break;
		}
  	}



	/*ATTENDO CHE I GIOCATORI ABBIANO PIAZZATO TUTTE LE PEDINE*/
  	semaphoreID = 0;/*indice del semaforo che voglio aspettare*/
  	waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
  	waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
  	waitForZeroOperation.sem_flg = 0;
  	semop(mySemaphores->semaforoFineTurnazionePedine, &waitForZeroOperation, 1);

  	printf("[Master] I giocatori hanno finito di piazzare le pedine...\n\n\n\n\n\n");

  	/*Inizializzo l'alarm*/
  	signal(SIGALRM, alarmEndGame);
  	





  	/*INIZIO DEL ROUND*/


  	contatore=0;
  	continua=1;
  	/*for(contatore=0;contatore<4 && continua;contatore++){*/
  	while(continua!=0){ /*Finchè non scatta il timer o vengono mangiate tutte le bandierine*/
  		contatore++; /*Mi serve a contare a che round sono*/
		
		printf("\nROUND %d\n",contatore);

	  	/*Distribuisco le bandierine su celle libere della scacchiera, in un numero casuale tra SO_FLAG_MIN e MAX, con somma totale dei punti delle bandierine uguale a SO_ROUND_SCORE*/
		numBandierineUtilizzate = getRandomNumber(SO_FLAG_MIN,SO_FLAG_MAX);
		punteggio = (SO_ROUND_SCORE/numBandierineUtilizzate);
		
		printf("[Master] piazzo %d bandierine\n",numBandierineUtilizzate);

		/*Inizializzo il semaforo che mi permette di sbloccare il master che sta aspettando che le bandierine vengano mangiate
		il master al fondo fa una wait for 0 di questo semaforo, inizializzato a numBandierineUtilizzate, quando una bandierina viene
		mangiata abbasso di uno questo semaforo*/
		semctl(mySemaphores->endRound, 0, SETVAL, numBandierineUtilizzate);

		/*Piazzo numBandierineUtilizzate bandierine*/
		for(k=0;k<numBandierineUtilizzate;k++){ 
			/*Le piazzo in posizioni casuali*/
			i_index = getRandomNumber(0,SO_ALTEZZA);
		  	j_index = getRandomNumber(0,SO_BASE);
		  	exit_cond=0;
		  	while(exit_cond==0){
		  		/*se il valore del semaforo in quella cella è 0 posso accedere alla cella in scacchiera, ho trovato la mia posizione*/
				if(semctl(mySemaphores->setSemaforiScacchiera, (i_index*SO_BASE+j_index), GETVAL) == 0){ 
					/*semctl(mySemaphores->setSemaforiScacchiera, (i_index*SO_BASE+j_index), SETVAL, 1);*/ /*NB NON blocco il semaforo perchè le pedine devono poter accedere alle celle con le banierine*/
			  		exit_cond=1;
			  	}else{
			  		/*Se quella cella è occupata cerco un altra posizione*/
			  		i_index = getRandomNumber(0,SO_ALTEZZA);
		  			j_index = getRandomNumber(0,SO_BASE);
			  	}	
		  	}
		  	/*Metto in scacchiera la bandierina e aggiorno il suo punteggio*/
		  	myScacchiera[i_index*SO_BASE+j_index].bandierina=1;
		  	myScacchiera[i_index*SO_BASE+j_index].punteggioBandierina = punteggio;  
		}
		
		/*Stampo lo stato del gioco come in sezione 1.6*/
		stampaStatoDelGioco(idSharedMemoryScacchiera,idSharedMemoryMosse,punteggioGiocatori);
		




		/*Ora permetto ai giocatori di fornire indicazioni di inizio round alle pedine*/
		/*mySemaphores->semaforoIndicazioniPedine rimane a 1 in attesa che il master lo porti a 0 e dia l'ordine ai giocatori di dare indicazioni alle pedine sui movimenti*/
	 	semctl(mySemaphores->semaforoIndicazioniPedine, 0, SETVAL, 0); 



		/*Resto in attesa che i giocatori finiscano di dare indicazioni alle pedine*/
		semaphoreID = 1;/*indice del semaforo che voglio aspettare*/
	  	waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
	  	waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa wait for zero */
	  	waitForZeroOperation.sem_flg = 0;
	  	semop(mySemaphores->semaforoIndicazioniPedine, &waitForZeroOperation, 1);

	  	/*Formatto i semafori nel caso ci fosse un nuovo round*/
	  	semctl(mySemaphores->semaforoIndicazioniPedine, 0, SETVAL, 1);
	  	semctl(mySemaphores->semaforoIndicazioniPedine, 1, SETVAL, SO_NUM_G);
		
		
	  	printf("[Master] Tutti i giocatori hanno dato indicazioni alle pedine, avvio il round ed il timer...\n");
		
	 
	 	/*Avvio il timer*/
	  	alarm(SO_MAX_TIME);
	  	if(contatore==1){ /*Al primo round mi segno il clock cosi da calcolare il tempo totale di gioco*/
			gettimeofday(&begin, NULL);
	  	}
	  	
	  		  	
	  	/*Sblocco chi sta aspettando che il gioco inizi*/
	  	semaphoreID = 0; /*indice del semaforo che voglio aspettare*/
	  	abbassaSemaforo.sem_num = (unsigned short)semaphoreID;
	  	abbassaSemaforo.sem_op  = -1; /*Se metto -1 qua abbasso il semaforo di 1*/
	  	abbassaSemaforo.sem_flg = 0;
	  	semop(mySemaphores->gameStart, &abbassaSemaforo, 1);

	  


	  	if(continua!=0){ /*Se non è scaduto il timer entro qua  e aspetto che tutte le bandierine vengano conquistate
	  			  anche se so che se arrivo qua endRound vale 0 quindi passerò*/
	  		semaphoreID = 0;/*indice del semaforo che voglio aspettare*/
	  		waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
	  		waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
	  		waitForZeroOperation.sem_flg = 0;
	  		semop(mySemaphores->endRound, &waitForZeroOperation, 1);
	  		/*printf("CIAO MI SONO SBLOCCATO#########################################################\n");*/
	  	}
	  	
	  	/*Formatto i valori del semaforo gameStart nel caso ci fosse un nuovo round*/
	  	semctl(mySemaphores->gameStart, 0, SETVAL, 1);

	  	/*Azzero il timer, lo farò ripartire al prossimo round*/
		alarm(0);

	  	printf("ROUND %d TERMINATO\n\n\n\n\n\n\n",contatore);


	  	/*Aggiornamento punteggio*/
	  	for(i=0;i<SO_ALTEZZA;i++){
			for(j=0;j<SO_BASE;j++){
			if( (myScacchiera[i*SO_BASE+j].pidPedina==1) && (myScacchiera[i*SO_BASE+j].bandierina==1) ){
					myScacchiera[i*SO_BASE+j].bandierina=-1;
					temp = myScacchiera[i*SO_BASE+j].padrePedina;
					punteggioGiocatori[temp] = punteggioGiocatori[temp] + myScacchiera[i*SO_BASE+j].punteggioBandierina;	
				}
			}
		}
  	}


  	/*### FINE ROUND ###*/
	gettimeofday(&end,NULL);


  	
  	printf("\n\n[Master] Segnalo ai giocatori che il gioco si e' concluso...\n");
  	numRoundFinale = contatore;
  	for(contatore=0;contatore<SO_NUM_G;contatore++){ 
  		kill(pidGiocatori[contatore],SIGUSR1);
  	}

  	/*Attendo che terminino tutti i figli*/
	while ((wpid = wait(NULL)) > 0);
	printf("[Master] Tutti processi giocatori terminati...\n");
	
	/*Stampo lo stato del gioco come in 1.6*/
	stampaStatoDelGioco(idSharedMemoryScacchiera,idSharedMemoryMosse,punteggioGiocatori);
	
	/*Stampo le metriche come descritto in 1.8*/
	time_spent = (double)(((end.tv_sec-begin.tv_sec)*1000000+end.tv_usec)-(begin.tv_usec)) / 1000000;
	stampaMetriche(numRoundFinale, idSharedMemoryMosse, SO_N_MOVES, SO_ROUND_SCORE,punteggioGiocatori,time_spent,SO_NUM_G,SO_NUM_P);


	
	/*Cancellazione del set di semafori*/
	if(semctl(mySemaphores->setSemaforiPosizionamentoPedine, -1, IPC_RMID, 0) == -1){ /*id,posizione del semaforo(ignorata con IPC_RMID), IPC_RMID cancella tutto il set di semafori svegliando tutti i processi bloccati in semop*/
		printf("Error in semctl\n");
		exit(EXIT_FAILURE);
	} 
	if(semctl(mySemaphores->semaforoFineTurnazionePedine, -1, IPC_RMID, 0) == -1){
		printf("Error in semctl\n");
		exit(EXIT_FAILURE);
	}
	if(semctl(mySemaphores->setSemaforiScacchiera, -1, IPC_RMID, 0) == -1){
		printf("Error in semctl\n");
		exit(EXIT_FAILURE);
	}
	if(semctl(mySemaphores->semaforoIndicazioniPedine, -1, IPC_RMID, 0) == -1){
		printf("Error in semctl\n");
		exit(EXIT_FAILURE);
	}
	if(semctl(mySemaphores->gameStart, -1, IPC_RMID, 0) == -1){
		printf("Error in semctl\n");
		exit(EXIT_FAILURE);
	}
	if(semctl(mySemaphores->endRound, -1, IPC_RMID, 0) == -1){
		printf("Error in semctl\n");
		exit(EXIT_FAILURE);
	}
	printf("[Master] Eliminati tutti i set di semafori...\n");
  	/*Detach e cancellazione della memoria condivisa dei semafori*/
	if(shmdt(mySemaphores)==-1 || shmctl(idSharedMemorySemafori,IPC_RMID,0)==-1){
    		printf("Error nella shmdt/shmctl\n");
    		exit(EXIT_FAILURE);
  	}
  	/*Detach e cancellazione della memoria condivisa della scacchiera*/
	if(shmdt(myScacchiera)==-1 || shmctl(idSharedMemoryScacchiera,IPC_RMID,0)==-1){ /*Il parametro IPC_RMID rimuove il segmento solo dopo che tutti i processi hanno datto detach*/
    		printf("Error nella shmdt/shmctl\n");
    		exit(EXIT_FAILURE);
  	}
  	if(shmdt(mosseGiocatori)==-1 || shmctl(idSharedMemoryMosse,IPC_RMID,0)==-1){
    		printf("Error nella shmdt/shmctl\n");
    		exit(EXIT_FAILURE);
  	}
  	free(punteggioGiocatori);
	free(pidGiocatori);
  	printf("[Master] Rimosse tutte le memorie condivise...\n");
  	printf("[Master] Fine del gioco\n");
	return 0;
}


void alarmEndGame(int sig) {
	sharedSemaphores* mySemaphores;
	mySemaphores = shmat(semId,NULL,0);
	mySemaphores->runGiocatori=0;
  	printf("[Master] Il timer di gioco e' scaduto...\n");
  	continua=0;
}