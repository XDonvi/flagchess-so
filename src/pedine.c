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
#include <sys/signal.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include "util.h"


#define MAX_TEXT 512 /*Definisco una struttura per il messaggio in modo da mettere più di un carattere, volendo si può spostare in util.h*/

struct my_msg_st {
        long int my_msg_type;
        char some_text[MAX_TEXT];
};

int run=1;
int id_sh_sem=0;

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
	int indicePedinaMatriceMosse; /*è la j della pedina nella riga del suo giocatore*/
	
	/*Variabili di appoggio*/
	int i_index, j_index, x, y, i_futura, j_futura, count;
	
	/*Variabili per la coda di messaggi*/
	int q_id;
	int num_bytes;
	long rcv_type;
	int id_queque;
	struct my_msg_st my_msg;
	
	/*Altro*/
	int conquista_bandierina_exit;
	char delimeter[] = "/"; /*Delimitatore per strtok in modo da splittare la mossa consigliata*/

	struct timespec tim;
	struct timespec tim2;


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
	id_sh_sem = atoi(argv[2]);


	/*Attach shared memory*/
	
	myScacchiera = shmat(idSharedMemoryScacchiera,NULL,0);
	mySemaphores = (sharedSemaphores*)shmat(idSharedMemorySemafori,NULL,0);


	/*Posizione iniziale della pedina*/
	
	i_index=atoi(argv[3]); /*i_index di piazzamento pedina iniziale*/
  	j_index=atoi(argv[4]); /*j_index di piazzamento pedina iniziale*/
	id_queque=atoi(argv[5]); /*Indice del giocatore che ha generato la pedina*/


	/*Questa pedina puo accedere solo alla cella id_queque*SO_NUM_P+indicePedinaMatriceMosse della matrice delle mosse*/
	
	idSharedMemoryMosse = atoi(argv[6]);
	mosseGiocatori = shmat(idSharedMemoryMosse,NULL,0);
	indicePedinaMatriceMosse = atoi(argv[7]);
	
	/*IO PEDINA POSSO ACCEDERE SOLO A QUESTA CELLA*/
  	mosseGiocatori[id_queque*SO_NUM_P+indicePedinaMatriceMosse];
  	


	q_id = msgget(id_queque+1, IPC_CREAT | 0600);
	rcv_type = 1;
	signal(SIGUSR1, handle);
	count=0;
	conquista_bandierina_exit=1;
	mySemaphores->runPedine=1;

	/*### ROUND ###*/

	while((mySemaphores->runPedine!=0)){

		while ((mySemaphores->runPedine!=0)) {
			/* now receiving the message */
			num_bytes = msgrcv(q_id, &my_msg, 120, rcv_type, 0);
			if (num_bytes >= 0) {
				/* received a good message (possibly of zero length) */
				break;
			}
		}

		/*printf("[Pedina del giocatore %d] Q_id=%d: msg type=%ld \"%s\" RECEIVED\n",id_queque, q_id, my_msg.mtype, my_msg.mtext);*/
		x = atoi(strtok(my_msg.some_text,delimeter)); /*Conversione del primo parametro splittato*/
		y = atoi(strtok(NULL,delimeter));

		srand(getpid());
		
		/*printf("[pedina %d del giocatore %d] vado in %d/%d\n", getpid(),id_queque,x,y);*/

		/*Ora le pedine hanno le informazioni sul primo spostamento da fare aspettano che il master dia il via a giocare*/
		semaphoreID = 0;/*indice del semaforo che voglio aspettare*/
	  	waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
	  	waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
	  	waitForZeroOperation.sem_flg = 0;
	  	semop(mySemaphores->gameStart, &waitForZeroOperation, 1);

	  	/*############MOVIMENTO PEDINA ###########*/

	  	/*Mi muovo finchè ho mosse / scade il timer / conquisto una bandierina*/
	  	while(mosseGiocatori[id_queque*SO_NUM_P+indicePedinaMatriceMosse]>0 && (mySemaphores->runPedine!=0) && conquista_bandierina_exit){

			/*Movimento che voglio fare*/
			if(count==0 && run){
				i_futura = i_index+x;
			  	j_futura = j_index+y;				
			}
			else{
				i_futura = i_index + rand()%3-1;
				if(i_futura==i_index)
					j_futura = j_index + rand()%3-1; 
				else
					j_futura = j_index;
			}

		  	if(i_futura>=0 && i_futura<=SO_ALTEZZA && j_futura>=0 && j_futura<=SO_BASE){
		  		if(semctl(mySemaphores->setSemaforiScacchiera, (i_futura*SO_BASE+j_futura), GETVAL) == 0){
		  			semctl(mySemaphores->setSemaforiScacchiera, (i_futura*SO_BASE+j_futura), SETVAL, 1);
					myScacchiera[i_futura*SO_BASE+j_futura].pidPedina=1;
		  			myScacchiera[i_futura*SO_BASE+j_futura].padrePedina = myScacchiera[i_index*SO_BASE+j_index].padrePedina;	
		  			myScacchiera[i_index*SO_BASE+j_index].pidPedina=-1;/*Rendo libera la cella in cui mi trovo cambiando valore della cella*/
		  			myScacchiera[i_index*SO_BASE+j_index].padrePedina = -1;
		  			semctl(mySemaphores->setSemaforiScacchiera, (i_index*SO_BASE+j_index), SETVAL, 0); /*Rendo libera la cella in cui mi trovo cambiando valore del semaforo*/	
		  			/*nanosleep*/
		  			/*usleep(SO_MIN_HOLD_NSEC/1000);*/

		  			tim.tv_sec = 0;
					tim.tv_nsec= SO_MIN_HOLD_NSEC;
					nanosleep(&tim,&tim2);


  					i_index = i_futura;		/*Aggiorno le coordinate attuali con la nouva mossa*/
  					j_index = j_futura;
  					mosseGiocatori[id_queque*SO_NUM_P+indicePedinaMatriceMosse] = mosseGiocatori[id_queque*SO_NUM_P+indicePedinaMatriceMosse]-1;
  					count++;
  					if(myScacchiera[i_index*SO_BASE+j_index].bandierina == 1){
  						/*Quando conquisto una bandierina mi fermo nel punto in cui sono, il master e i giocatori ora lo sanno grazie alla shared memory*/
  						conquista_bandierina_exit=0;

  						printf("[Pedina del giocatore %d] E' stata conquistata la bandierina in posizione i:%d j:%d...\n",id_queque, i_index,j_index);
  						/*Dato che una bandierina è stata mangiata abbasso di 1 il semaforo endRound cosi da avere il conto di quante ne sono state mangiate*/
  						semaphoreID = 0; /*indice del semaforo che voglio aspettare*/
	  					abbassaSemaforo.sem_num = (unsigned short)semaphoreID;
	  					abbassaSemaforo.sem_op  = -1;
	  					abbassaSemaforo.sem_flg = 0;
	  					semop(mySemaphores->endRound, &abbassaSemaforo, 1);
  					}
		  		}
		  	}
	  	}
	  	

	  	/*Se finisco le mosse aspetto la fine del round*/
	  	if(mySemaphores->runPedine!=0){ /*Se esco dal while sopra perchè è conclusa la partita mi bloccherei qua, ma io metto run!=0 cosi so che la partita è conclusa e non mi blocco qua*/
	  		semaphoreID = 0;/*indice del semaforo che voglio aspettare*/
			waitForZeroOperation.sem_num = (unsigned short) semaphoreID;
  			waitForZeroOperation.sem_op  = 0; /* L'operazione 0 significa "aspetto che il semaforo raggiunga valore 0" */
			waitForZeroOperation.sem_flg = 0;
  			semop(mySemaphores->endRound, &waitForZeroOperation, 1);
	  	}
	}

	/*printf("Pedina figlia %d, mi sono mossa %d volte\n", myScacchiera[i_index*SO_BASE+j_index].padrePedina ,count);*/
	exit(0);
}

void handle(int signum){
	sharedSemaphores* mySemaphores;
	mySemaphores = (sharedSemaphores*)shmat(id_sh_sem,NULL,0);
	semctl(mySemaphores->endRound, 0, SETVAL, 0);	
}
