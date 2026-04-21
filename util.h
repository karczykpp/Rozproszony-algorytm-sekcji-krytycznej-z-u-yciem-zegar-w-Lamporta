#ifndef UTILH
#define UTILH
#include "main.h"

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta) */
    int src;
    int data;     /* niesie timestamp żądania w REQ, ilość nawozu w UZUPELNIENIE */
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=3 */
#define NITEMS 3

/* Typy wiadomości */
#define ACK             1
#define REQ_GRABKI      2
#define REL_GRABKI      3
#define REQ_LOPATKI     4
#define REL_LOPATKI     5
#define REQ_NAWOZ       6
#define REL_NAWOZ       7
#define BRAK_NAWOZU     8
#define UZUPELNIENIE    9

extern MPI_Datatype MPI_PAKIET_T;
void inicjuj_typ_pakietu();

/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

/* wpis w kolejce żądań */
typedef struct {
    int ts;   /* timestamp żądania */
    int pid;  /* id procesu */
} queue_entry_t;

/* stany procesów */
typedef enum {
    REST,           /* odpoczynek */
    WAIT_GRABKI,    /* czeka na grabki */
    WAIT_LOPATKI,   /* czeka na łopatki */
    WAIT_NAWOZ,     /* czeka na nawóz */
    PIELENIE,       /* pieli ogródek */
    ZAKUPY          /* kupiec robi zakupy (tylko proces 0) */
} state_t;

extern state_t stan;
extern pthread_mutex_t stateMut;
extern pthread_mutex_t sendMut; /* serializacja MPI_Send — wymusza FIFO per nadawca */

/* zegar Lamporta */
extern int lamport_clock;

/* kolejki żądań */
extern queue_entry_t grabki_queue[];
extern int grabki_queue_size;
extern queue_entry_t lopatki_queue[];
extern int lopatki_queue_size;
extern queue_entry_t nawoz_queue[];
extern int nawoz_queue_size;

/* wektor ostatnich timestampów od każdego procesu */
extern int last_ts[];

/* bufor UZUPELNIENIE (kolejka FIFO z ilością nawozu) */
#define MAX_RESUPPLY 100
extern int resupply_queue[];
extern int resupply_head, resupply_tail;

/* nawóz i flagi */
extern int nawoz_remaining;
extern int needs_shopping; /* tylko proces 0 */

/* zmienna warunkowa */
extern pthread_cond_t cond;

/* zmiana stanu */
void changeState(state_t);

/* operacje na kolejkach */
void queue_add(queue_entry_t *queue, int *qsize, int ts, int pid);
void queue_remove(queue_entry_t *queue, int *qsize, int pid);
int queue_position(queue_entry_t *queue, int qsize, int pid);
int can_access(queue_entry_t *queue, int qsize, int capacity, int my_req_ts, int my_pid);

/* resupply queue */
void resupply_enqueue(int amount);
int resupply_dequeue(void);
int resupply_is_empty(void);

/* konfiguracja */
void read_config(const char *filename);

int max(int a, int b);

#endif
