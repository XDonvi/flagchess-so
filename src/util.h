typedef struct{
	int pidPedina; 	/*se è presente una pedina sulla casella questo intero contiene 1, altrimenti 0*/
	int bandierina;	/*se è presente una bandierina su questa cella vale 1, altrimenti -1*/
	int punteggioBandierina; /*Se è presente una bandierina su questa casella qua è contenuto il suo punteggio*/
	int padrePedina; /*Se in questa casella c'è una bandierina, qui c'è il "numero" del giocatore che l'ha piazzata*/
}strutturaCellaScacchiera;


typedef struct{
	long setSemaforiScacchiera;		/*Set di semafori usato per l'accesso in mutua esclusione alle celle della scacchiera*/
	int setSemaforiPosizionamentoPedine;	/*Set di semafori utilizzato per la turnazione del posizionamento delle pedine da parte dei giocatori*/
	int semaforoFineTurnazionePedine;	/*Utilizzato per sbloccare il master dopo che i giocatori hanno piazzato le pedine*/
	int semaforoIndicazioniPedine; 		/*utilizzato per permettere ai giocatori di dare le info sul movimento alle pedine*/
	int gameStart; 				/*Quando tutti i giocatori hanno piazzato le pedine, dato le indicazioni sugli spostamenti e comunicato cio al master, il gioco puo iniziare*/
	int endRound; 				/*Quando tutte le pedine hanno fatto le mosse volute nel round si può rimuovere l'alarm (TODO: al posto del semaforo ci sarà un numBandiereConquistate)*/
	int runGiocatori;
	int runPedine;
}sharedSemaphores;


struct msgbuf {
	long mtype;         /*message type, must be > 0*/
	char mtext[120];    /*message data*/
};


/*Load dei parametri da file di testo*/
void load_info(int*);


/*Stampa lo stato del gioco*/
void stampaStatoDelGioco(int,int,int*);


/*Stampa metriche 1.8*/
void stampaMetriche(int, int, int, int,int*,double,int,int);


/*stampa una scacchiera e mi dice dove sono i semafori bloccati*/
void stampaStatoDeiSemaforiScacchiera(int); 


/*Restituisce un numero random tra il range dato MIN,MAX*/
int getRandomNumber(int,int);

