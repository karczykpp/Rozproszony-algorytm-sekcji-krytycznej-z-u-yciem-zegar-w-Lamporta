#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void *ptr)
{
    MPI_Status status;
    packet_t pakiet;

    while (1) {
        /* Blokujący odbiór wiadomości */
        MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        /* Aktualizacja zegara Lamporta PO odebraniu (fix buga z oryginalnego kodu) */
        pthread_mutex_lock(&stateMut);
        lamport_clock = max(lamport_clock, pakiet.ts) + 1;
        last_ts[pakiet.src] = pakiet.ts;
        pthread_mutex_unlock(&stateMut);

        switch (status.MPI_TAG) {

        case REQ_GRABKI:
            //debug("Otrzymałem REQ_GRABKI od %d (req_ts=%d)", pakiet.src, pakiet.data);
            pthread_mutex_lock(&stateMut);
            queue_add(grabki_queue, &grabki_queue_size, pakiet.data, pakiet.src);
            pthread_mutex_unlock(&stateMut);
            sendPacket(0, pakiet.src, ACK);
            pthread_cond_broadcast(&cond);
            break;

        case REL_GRABKI:
            //debug("Otrzymałem REL_GRABKI od %d", pakiet.src);
            pthread_mutex_lock(&stateMut);
            queue_remove(grabki_queue, &grabki_queue_size, pakiet.src);
            pthread_mutex_unlock(&stateMut);
            pthread_cond_broadcast(&cond);
            break;

        case REQ_LOPATKI:
            //debug("Otrzymałem REQ_LOPATKI od %d (req_ts=%d)", pakiet.src, pakiet.data);
            pthread_mutex_lock(&stateMut);
            queue_add(lopatki_queue, &lopatki_queue_size, pakiet.data, pakiet.src);
            pthread_mutex_unlock(&stateMut);
            sendPacket(0, pakiet.src, ACK);
            pthread_cond_broadcast(&cond);
            break;

        case REL_LOPATKI:
            //debug("Otrzymałem REL_LOPATKI od %d", pakiet.src);
            pthread_mutex_lock(&stateMut);
            queue_remove(lopatki_queue, &lopatki_queue_size, pakiet.src);
            pthread_mutex_unlock(&stateMut);
            pthread_cond_broadcast(&cond);
            break;

        case REQ_NAWOZ:
            //debug("Otrzymałem REQ_NAWOZ od %d (req_ts=%d)", pakiet.src, pakiet.data);
            pthread_mutex_lock(&stateMut);
            queue_add(nawoz_queue, &nawoz_queue_size, pakiet.data, pakiet.src);
            pthread_mutex_unlock(&stateMut);
            sendPacket(0, pakiet.src, ACK);
            pthread_cond_broadcast(&cond);
            break;

        case REL_NAWOZ: {
            //debug("Otrzymałem REL_NAWOZ od %d", pakiet.src);
            int nawoz_is_zero = 0;
            pthread_mutex_lock(&stateMut);
            nawoz_remaining--;
            if (nawoz_remaining < 0) nawoz_remaining = 0;
            queue_remove(nawoz_queue, &nawoz_queue_size, pakiet.src);

            /* Sprawdź bufor uzupełnień */
            if (nawoz_remaining == 0 && !resupply_is_empty()) {
                nawoz_remaining = resupply_dequeue();
                debug("Uzupełniono nawóz z bufora: %d", nawoz_remaining);
            }

            nawoz_is_zero = (nawoz_remaining == 0);

            if (nawoz_is_zero && rank == 0) {
                needs_shopping = 1;
            }
            pthread_mutex_unlock(&stateMut);

            // /* Powiadom kupca */
            // if (nawoz_is_zero && rank != 0) {
            //     sendPacket(0, 0, BRAK_NAWOZU);
            // }
            pthread_cond_broadcast(&cond);
            break;
        }

        case BRAK_NAWOZU:
            debug("Otrzymałem BRAK_NAWOZU od %d", pakiet.src);
            pthread_mutex_lock(&stateMut);
            if (nawoz_remaining == 0) {
                needs_shopping = 1;
            }
            pthread_mutex_unlock(&stateMut);
            pthread_cond_broadcast(&cond);
            break;

        case UZUPELNIENIE:
            debug("Otrzymałem UZUPELNIENIE od %d (ilość=%d)", pakiet.src, pakiet.data);
            pthread_mutex_lock(&stateMut);
            if (nawoz_remaining == 0) {
                nawoz_remaining = pakiet.data;
            } else {
                resupply_enqueue(pakiet.data);
            }
            pthread_mutex_unlock(&stateMut);
            pthread_cond_broadcast(&cond);
            break;

        case ACK:
            //debug("Otrzymałem ACK od %d", pakiet.src);
            /* last_ts i clock już zaktualizowane powyżej */
            pthread_cond_broadcast(&cond);
            break;

        default:
            break;
        }
    }
    return NULL;
}
