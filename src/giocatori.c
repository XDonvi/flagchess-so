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
#include <fcntl.h>
#include <sys/stat.h>
#include "util.h"

#define MAX_TEXT 512 /*Definisco una struttura per il messaggio in modo da mettere più di un carattere, volendo si può spostare in util.h*/
struct my_msg_st 
{
        long int my_msg_type;
        char some_text[120];
};

int run=1; /*Variabile per effettuare la roundazione*/
int semId;
void handle(int signum);


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
	struct sembuf abbassaSemaforo;
	struct sembuf alzaSemaforo;
	struct sembuf waitForZeroOperation;
	/*Variabili per la shared memory delle mosse*/
	int idSharedMemoryMosse;
	int* mosseGiocatori;
	char indicePedinaMatriceMosseStr[1000];
	/*Variabili utilizzate per la gestione dei processi*/
	pid_t* pidPedine; /*Vettore in cui sono contenuti i pid delle mie pedine*/
	pid_t wpid;
	/*Variabili di appoggio*/
	int i,j,k, i_index, j_index, exit_cond;
	/*Utilizzo queste stringhe per passare parametri nella execve*/
	char idSharedMemoryScacchieraStr[1000]; 
	char idSharedMemorySemaforiStr[1000];
	char idSharedMemoryMosseStr[1000];
	char id_queque[1000];
	char i_indexStr[10000];
	char j_indexStr[10000];
	/*Altre*/
	char verificaCoda;
	int indiceNelSemaforoTurnazione; /*Indice del giocatore nel semaforo turnazione, identifica anche il "numero" del giocatore*/
	char* args[9]; /*NomeProgramma, shmScacchiera,shmSemafori, iPosizPedina, jPosizPedina, idGenitore, shmMosse, indiceJ_MatriceMosse*/
	/*Variabili per i messaggi alle pedine*/
	int q_id;
	struct my_msg_st my_msg;
	/*Variabili per dare indicazioni alle pedine*/
	char* mossePossibili[] = {"0/-1","+1/0","0/+1","-1/0"}; /*Le altre mosse come -1/+1 non ci sono perchè sono permesse solo nelle 4 direzioni*/
	int contatore;


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
	idSharedMemoryScacchiera = atoi(argv[1]);
	idSharedMemorySemafori = atoi(argv[2]);
	semId = idSharedMemorySemafori;
	indiceNelSemaforoTurnazione = atoi(argv[3]);


	/*Questo giocatore può accedere solo alla riga indiceNelSemaforoTurnazione della matrice mosse*/
	idSharedMemoryMosse = atoi(argv[4]);

	mosseGiocatori = shmat(idSharedMemoryMosse,NULL,0);
	
	/*Attach shared memory Scacchiera e Semafori*/
	myScacchiera = shmat(idSharedMemoryScacchiera,NULL,0);
	mySemaphores = (sharedSemaphores*)shmat(idSharedMemorySemafori,NULL,0);

	/*Piazzo le pedine*/
	pidPedine=malloc(sizeof(int)*SO_NUM_P); /*Alloco il vettore dei pid delle pedine con la dimensione del numero di pedine che ho a disposizione*/
	sprintf(idSharedMemoryScacchieraStr, "%d", idSharedMemoryScacchiera);
	sprintf(idSharedMemorySemaforiStr, "%d", idSharedMemorySemafori);
	sprintf(idSharedMemoryMosseStr, "%d", idSharedMemoryMosse);
	for (k=0;k<SO_NUM_P;k++) {

		/*Aspetto che sia il mio turno di piazzare*/
  		semaphoreID = indiceNelSemaforoTurnazione;/*indice del semaforo che voglio aspettare*/
  		waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
  		waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
  		waitForZeroOperation.sem_flg = 0;
  		semop(mySemaphores->setSemaforiPosizionamentoPedine, &waitForZeroOperation, 1);

  		/*Entro in sezione critica*/

  		/*Scelgo in quale cella della scacchiera mettere la pedina in modo randomico*/
  		i_index = getRandomNumber(0,SO_ALTEZZA);
  		j_index = getRandomNumber(0,SO_BASE);
  		exit_cond=0;
  		while(exit_cond==0){
	  		if(semctl(mySemaphores->setSemaforiScacchiera, (i_index*SO_BASE+j_index), GETVAL) == 0){ /*se il valore del semaforo in quella cella è 0 posso accedere alla cella in scacchiera*/
	  			semctl(mySemaphores->setSemaforiScacchiera, (i_index*SO_BASE+j_index), SETVAL, 1); /*blocco il semaforo su quella cella e lo lascio bloccato perchè c'è una pedina ora*/
	  			exit_cond=1;
	  		}else{
	  			i_index = getRandomNumber(0,SO_ALTEZZA);
  				j_index = getRandomNumber(0,SO_BASE);
	  		}	
  		}
  		/*Piazzo la pedina*/
  		myScacchiera[i_index*SO_BASE+j_index].pidPedina = 1; /*Se e' presente una pedina su questa cella qua c'è il PID di quella pedina, altrimenti vale -1*/
  		myScacchiera[i_index*SO_BASE+j_index].bandierina=-1;  /*Se e' presente una bandierina in questa posizione c'è il suo pid, altrimenti vale -1*/
  		myScacchiera[i_index*SO_BASE+j_index].punteggioBandierina=-1; /*Se e' presente una bandierina in posizione [i][j] qua c'è il punteggio di quella bandierina*/
  		myScacchiera[i_index*SO_BASE+j_index].padrePedina = indiceNelSemaforoTurnazione;

  		sprintf(i_indexStr,"%d", i_index);
  		sprintf(j_indexStr,"%d", j_index);
  		sprintf(id_queque , "%d", indiceNelSemaforoTurnazione);
  		sprintf(indicePedinaMatriceMosseStr,"%d", k);

  		args[0] = "pedine";
		args[1] = idSharedMemoryScacchieraStr; 
		args[2] = idSharedMemorySemaforiStr; 
		args[3] = i_indexStr;/*Indice i in scacchiera su dove è la pedina*/
		args[4] = j_indexStr;/*Indice j in scacchiera su dove è la pedina*/
		args[5] = id_queque;
		args[6] = idSharedMemoryMosseStr;
		args[7] = indicePedinaMatriceMosseStr;
		args[8] = NULL;

		/*genero il figlio che starà nella posizione i_index, j_index e gli passo la sua posizione in scacchiera (appunto i_index j_index)*/
		pidPedine[k] = fork();

		switch(pidPedine[k]){
			case -1:
				printf("Error in fork\n");
			break;
			case 0:
				execve("./pedine",args,NULL);
				exit(EXIT_SUCCESS);
			break;
			default:
				
			break;
		}
	
		
		/*Ho piazzato una pedina quindi alzo il mio semaforo per permettere agli altri giocatori di piazzare le pedine e poi aspetto nuovamente il mio turno*/
		semaphoreID = indiceNelSemaforoTurnazione; /*indice del semaforo che voglio aspettare*/
  		alzaSemaforo.sem_num = (unsigned short)semaphoreID;
  		alzaSemaforo.sem_op  = 1; /*Se metto 1 qua alzo il semaforo di 1*/
  		alzaSemaforo.sem_flg = 0;
  		semop(mySemaphores->setSemaforiPosizionamentoPedine, &alzaSemaforo, 1);

  		/*Fine sezione critica*/

		/*Voglio passare il turno al giocatore con indice successivo al mio*/
  		semaphoreID = (indiceNelSemaforoTurnazione+1)%SO_NUM_G; /*indice del semaforo che voglio aspettare*/
  		abbassaSemaforo.sem_num = (unsigned short)semaphoreID;
  		abbassaSemaforo.sem_op  = -1; /*Se metto 1 qua alzo il semaforo di 1*/
  		abbassaSemaforo.sem_flg = 0;
  		semop(mySemaphores->setSemaforiPosizionamentoPedine, &abbassaSemaforo, 1);
  	}
  	
  	printf("[Giocatore %d] Ho finito di piazzare tutte le mie %d pedine...\n", indiceNelSemaforoTurnazione,SO_NUM_P);

  	/*Comunico al master che ho finito di piazzare le mie pedine*/
  	semaphoreID = 0; /*indice del semaforo*/
  	abbassaSemaforo.sem_num = (unsigned short)semaphoreID;
  	abbassaSemaforo.sem_op  = -1; /*Se metto -1 qua abbasso il semaforo di 1*/
  	abbassaSemaforo.sem_flg = 0;
  	semop(mySemaphores->semaforoFineTurnazionePedine, &abbassaSemaforo, 1);



  	/*### ROUND ###*/

  	/*Creo una coda di messaggi per comunicare alle pedine lo spostamento da fare*/
	q_id = msgget(indiceNelSemaforoTurnazione+1, IPC_CREAT | 0600); /*il primo parametro è l'id della coda di messaggi, ho aggiunto 1 perchè deve essere maggiore di 0 e il primo giocatore aveva id 0*/
	my_msg.my_msg_type = 1; /*lasciare a 1*/
	srand(getpid());

	signal(SIGUSR1, handle);/*Creo il segnale per la fine del gioco*/






	mySemaphores->runGiocatori=1;

	while(mySemaphores->runGiocatori!=0){ /*Ripeto il round finchè non avvengono una delle due condizioni di fine round*/

		/*Rimango in attesa che il master mi dia il permesso di dare informazioni di movimento alle mie pedine*/
	  	semaphoreID = 0;/*indice del semaforo che voglio aspettare*/
	  	waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
	  	waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
	  	waitForZeroOperation.sem_flg = 0;
	  	semop(mySemaphores->semaforoIndicazioniPedine, &waitForZeroOperation, 1);

	  	if(mySemaphores->runGiocatori!=0){
		  	printf("[Giocatore %d]Posso dare info alle mie pedine...\n",indiceNelSemaforoTurnazione);
			/*mando SO_NUM_P messaggi*/
			for(i=0;i<SO_NUM_P;i++){
				strcpy(my_msg.some_text, mossePossibili[rand()%4]); /*Scelgo tra le possibili mosse*/
				if(msgsnd(q_id, &my_msg, 120, IPC_NOWAIT) == -1){
					if(errno == EAGAIN){
						/*printf("Buffer Pieno\n");*/
						/*break;*/
					}
				}
				
			}
		  	
		  	/*Comunico al master che ho finito di dare indicazioni alle mie pedine*/
		  	semaphoreID = 1; /*indice del semaforo*/
		  	abbassaSemaforo.sem_num = (unsigned short)semaphoreID;
		  	abbassaSemaforo.sem_op  = -1; /*Se metto -1 qua abbasso il semaforo di 1*/
		  	abbassaSemaforo.sem_flg = 0;
		  	semop(mySemaphores->semaforoIndicazioniPedine, &abbassaSemaforo, 1);
		  	
		  	printf("[Giocatore %d]Ho finito di dare info alle mie pedine...\n",indiceNelSemaforoTurnazione);
		  	
		  	/*I giocatori dopo aver finito di dare le info alle pedine devono aspettare che il master dia il via a tutti quanti di giocare*/
			/*Rimango in attesa che il master mi dia il permesso di iniziare il gioco*/
		  	semaphoreID = 0;/*indice del semaforo che voglio aspettare*/
		  	waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
		  	waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
		  	waitForZeroOperation.sem_flg = 0;
		  	semop(mySemaphores->gameStart, &waitForZeroOperation, 1);

		  	
	  		printf("[Giocatore %s] Inizia il gioco!!\n",id_queque);	
	  	}
	  	
	}

	/*### FINE ROUND ###*/

	/*Il round è concluso, lo segnalo alle pedine*/
	printf("[Giocatore %d] Segnalo alle mie pedine che il gioco si e' concluso...\n",indiceNelSemaforoTurnazione);


	for(contatore=0;contatore<SO_NUM_P;contatore++){
		kill(pidPedine[contatore],SIGUSR1);
	}

	
	/*Aspetto che i miei figli muoiano*/
	/*while ((wpid = wait(NULL)) > 0);*/


	printf("[Giocatore %d] Tutti i miei processi pedina terminati ed eliminata la coda messaggi...\n",indiceNelSemaforoTurnazione);
	free(pidPedine);
	/*Rimuovo la coda di messaggi*/
	msgctl(q_id, IPC_RMID, NULL);


	exit(0);

}
void handle(int signum){
	/*printf("[Giocatore] Ricevuto il segnale di fine gioco...\n");*/
	sharedSemaphores* mySemaphores;
	mySemaphores = (sharedSemaphores*)shmat(semId,NULL,0);
	semctl(mySemaphores->semaforoIndicazioniPedine, 0, SETVAL, 0);
	mySemaphores->runPedine=0;
}