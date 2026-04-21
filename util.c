#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

/* stan początkowy */
state_t stan = REST;

pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sendMut = PTHREAD_MUTEX_INITIALIZER;

/* zegar Lamporta */
int lamport_clock = 0;

/* kolejki żądań */
queue_entry_t grabki_queue[MAX_PROC];
int grabki_queue_size = 0;
queue_entry_t lopatki_queue[MAX_PROC];
int lopatki_queue_size = 0;
queue_entry_t nawoz_queue[MAX_PROC];
int nawoz_queue_size = 0;

/* wektor ostatnich timestampów */
int last_ts[MAX_PROC];

/* bufor UZUPELNIENIE */
int resupply_queue[MAX_RESUPPLY];
int resupply_head = 0, resupply_tail = 0;

/* nawóz i flagi */
int nawoz_remaining = 0; /* ustawiane w read_config */
int needs_shopping = 0;

/* zmienna warunkowa */
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/* nazwy tagów do debugowania */
struct tagNames_t {
    const char *name;
    int tag;
} tagNames[] = {
    { "ACK", ACK },
    { "REQ_GRABKI", REQ_GRABKI },
    { "REL_GRABKI", REL_GRABKI },
    { "REQ_LOPATKI", REQ_LOPATKI },
    { "REL_LOPATKI", REL_LOPATKI },
    { "REQ_NAWOZ", REQ_NAWOZ },
    { "REL_NAWOZ", REL_NAWOZ },
    { "BRAK_NAWOZU", BRAK_NAWOZU },
    { "UZUPELNIENIE", UZUPELNIENIE }
};

const char *const tag2string(int tag)
{
    for (int i = 0; i < sizeof(tagNames) / sizeof(struct tagNames_t); i++) {
        if (tagNames[i].tag == tag) return tagNames[i].name;
    }
    return "<unknown>";
}

/* tworzy typ MPI_PAKIET_T */
void inicjuj_typ_pakietu()
{
    int blocklengths[NITEMS] = {1, 1, 1};
    MPI_Datatype typy[NITEMS] = {MPI_INT, MPI_INT, MPI_INT};
    MPI_Aint offsets[NITEMS];
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);
}

/* wysyłanie pakietu — sendMut serializuje MPI_Send, wymuszając FIFO per nadawca */
void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt = 0;
    if (pkt == 0) { pkt = malloc(sizeof(packet_t)); freepkt = 1; }
    pkt->src = rank;

    /* lock order: sendMut → stateMut — nigdy odwrotnie */
    pthread_mutex_lock(&sendMut);
    pthread_mutex_lock(&stateMut);
    lamport_clock++;
    pkt->ts = lamport_clock;
    pthread_mutex_unlock(&stateMut);

    MPI_Send(pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    pthread_mutex_unlock(&sendMut);

    //debug("Wysyłam %s do %d", tag2string(tag), destination);
    if (freepkt) free(pkt);
}

/* zmiana stanu */
void changeState(state_t newState)
{
    pthread_mutex_lock(&stateMut);
    stan = newState;
    pthread_mutex_unlock(&stateMut);
}

/* operacje na kolejkach */
void queue_add(queue_entry_t *queue, int *qsize, int ts, int pid)
{
    int i = *qsize - 1;
    while (i >= 0 && (queue[i].ts > ts || (queue[i].ts == ts && queue[i].pid > pid))) {
        queue[i + 1] = queue[i];
        i--;
    }
    queue[i + 1].ts = ts;
    queue[i + 1].pid = pid;
    (*qsize)++;
}

void queue_remove(queue_entry_t *queue, int *qsize, int pid)
{
    for (int i = 0; i < *qsize; i++) {
        if (queue[i].pid == pid) {
            for (int j = i; j < *qsize - 1; j++)
                queue[j] = queue[j + 1];
            (*qsize)--;
            return;
        }
    }
}

int queue_position(queue_entry_t *queue, int qsize, int pid)
{
    for (int i = 0; i < qsize; i++)
        if (queue[i].pid == pid) return i;
    return -1;
}

int can_access(queue_entry_t *queue, int qsize, int capacity, int my_req_ts, int my_pid)
{
    int pos = queue_position(queue, qsize, my_pid);
    if (pos < 0 || pos >= capacity) return 0;
    for (int j = 0; j < size; j++) {
        if (j == my_pid) continue;
        if (last_ts[j] <= my_req_ts) return 0;
    }
    return 1;
}

/* resupply queue (circular FIFO) */
void resupply_enqueue(int amount)
{
    resupply_queue[resupply_tail] = amount;
    resupply_tail = (resupply_tail + 1) % MAX_RESUPPLY;
}

int resupply_dequeue(void)
{
    if (resupply_head == resupply_tail) return -1;
    int val = resupply_queue[resupply_head];
    resupply_head = (resupply_head + 1) % MAX_RESUPPLY;
    return val;
}

int resupply_is_empty(void)
{
    return resupply_head == resupply_tail;
}

/* czytanie konfiguracji z pliku */
void read_config(const char *filename)
{
    if (rank == 0) {
        FILE *f = fopen(filename, "r");
        if (f == NULL) {
            fprintf(stderr, "Nie mogę otworzyć pliku konfiguracyjnego %s\n", filename);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        fscanf(f, "G=%d\n", &G);
        fscanf(f, "L=%d\n", &L);
        fscanf(f, "D=%d\n", &D);
        fclose(f);
        printf("Konfiguracja: G=%d, L=%d, D=%d\n", G, L, D);
    }
    MPI_Bcast(&G, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&L, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&D, 1, MPI_INT, 0, MPI_COMM_WORLD);
    nawoz_remaining = D;
}

int max(int a, int b)
{
    return (a > b) ? a : b;
}
