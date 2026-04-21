#include "main.h"
#include "watek_glowny.h"

void mainLoop()
{
    srandom(rank);
    int my_grabki_ts = 0, my_lopatki_ts = 0, my_nawoz_ts = 0;
    int zakupy_from_wait = 0; /* czy Kupiec wszedł w ZAKUPY z WAIT_NAWOZ */
    int nawoz_was_zero = 0;   /* pomocnicza do wysłania BRAK_NAWOZU po odblokowaniu */

    while (1) {
        switch (stan) {

        case REST: {
            /* losowe opóźnienie 1-3s */
            sleep(1 + random() % 3);

            pthread_mutex_lock(&stateMut);

            /* Kupiec: sprawdź czy trzeba na zakupy */
            if (rank == 0 && needs_shopping) {
                stan = ZAKUPY;
                zakupy_from_wait = 0;
                println("Idę na zakupy (z REST)");
                pthread_mutex_unlock(&stateMut);
                break;
            }

            /* Rozpocznij ubieganie się o grabki */
            lamport_clock++;
            my_grabki_ts = lamport_clock;
            queue_add(grabki_queue, &grabki_queue_size, my_grabki_ts, rank);
            stan = WAIT_GRABKI;
            pthread_mutex_unlock(&stateMut);

            println("Ubiegam się o grabki");
            /* Wyślij REQ_GRABKI do wszystkich */
            packet_t pkt;
            pkt.data = my_grabki_ts;
            for (int i = 0; i < size; i++)
                if (i != rank)
                    sendPacket(&pkt, i, REQ_GRABKI);
            debug("Rozsyłam REQ_GRABKI do wszystkich");
                    
            break;
        }

        case WAIT_GRABKI: {
            pthread_mutex_lock(&stateMut);
            while (!can_access(grabki_queue, grabki_queue_size, G, my_grabki_ts, rank)) {
                /* Kupiec: sprawdź czy trzeba na zakupy */
                if (rank == 0 && needs_shopping) {
                    stan = ZAKUPY;
                    zakupy_from_wait = 0;
                    println("Idę na zakupy (z REST)");
                    pthread_mutex_unlock(&stateMut);
                    break;
                }
                pthread_cond_wait(&cond, &stateMut);
            }
            println("Mam grabki! Ubiegam się o łopatki");

            /* Przejście do WAIT_LOPATKI */
            lamport_clock++;
            my_lopatki_ts = lamport_clock;
            queue_add(lopatki_queue, &lopatki_queue_size, my_lopatki_ts, rank);
            stan = WAIT_LOPATKI;
            pthread_mutex_unlock(&stateMut);

            /* Wyślij REQ_LOPATKI do wszystkich */
            packet_t pkt;
            pkt.data = my_lopatki_ts;
            for (int i = 0; i < size; i++)
                if (i != rank)
                    sendPacket(&pkt, i, REQ_LOPATKI);
            debug("Rozsyłam REQ_LOPATKI do wszystkich");
            break;
        }

        case WAIT_LOPATKI: {
            pthread_mutex_lock(&stateMut);
            while (!can_access(lopatki_queue, lopatki_queue_size, L, my_lopatki_ts, rank)) {
                /* Kupiec: sprawdź czy trzeba na zakupy */
                if (rank == 0 && needs_shopping) {
                    stan = ZAKUPY;
                    zakupy_from_wait = 0;
                    println("Idę na zakupy (z REST)");
                    pthread_mutex_unlock(&stateMut);
                    break;
                }
                pthread_cond_wait(&cond, &stateMut);
            }
            println("Mam łopatki! Ubiegam się o nawóz");

            /* Przejście do WAIT_NAWOZ */
            lamport_clock++;
            my_nawoz_ts = lamport_clock;
            queue_add(nawoz_queue, &nawoz_queue_size, my_nawoz_ts, rank);
            stan = WAIT_NAWOZ;
            pthread_mutex_unlock(&stateMut);

            /* Wyślij REQ_NAWOZ do wszystkich */
            packet_t pkt;
            pkt.data = my_nawoz_ts;
            for (int i = 0; i < size; i++)
                if (i != rank)
                    sendPacket(&pkt, i, REQ_NAWOZ);
            debug("Rozsyłam REQ_NAWOZ do wszystkich");
            break;
        }

        case WAIT_NAWOZ: {
            pthread_mutex_lock(&stateMut);
            while (1) {
                /* Kupiec: jeśli nawóz się skończył, idź na zakupy */
                if (nawoz_remaining == 0 && rank == 0) {
                    stan = ZAKUPY;
                    zakupy_from_wait = 1;
                    println("Nawóz się skończył, idę na zakupy (z WAIT_NAWOZ)");
                    pthread_mutex_unlock(&stateMut);
                    goto next_iteration;
                }

                /* Sprawdź warunek dostępu do nawozu */
                if (nawoz_remaining > 0 &&
                    can_access(nawoz_queue, nawoz_queue_size, nawoz_remaining, my_nawoz_ts, rank)) {
                    break; /* dostęp przyznany */
                }

                pthread_cond_wait(&cond, &stateMut);
            }

            println("Mam nawóz! Pielę ogródek");
            stan = PIELENIE;
            pthread_mutex_unlock(&stateMut);
            break;
        }

        case PIELENIE: {
            println("Pielę w szklarni...");
            sleep(2 + random() % 3);

            pthread_mutex_lock(&stateMut);
            /* Zużyj nawóz */
            nawoz_remaining--;
            if (nawoz_remaining < 0) nawoz_remaining = 0;

            /* Sprawdź resupply_queue */
            if (nawoz_remaining == 0 && !resupply_is_empty()) {
                nawoz_remaining = resupply_dequeue();
                println("Uzupełniono nawóz z bufora: %d", nawoz_remaining);
            }

            nawoz_was_zero = (nawoz_remaining == 0);

            if (nawoz_was_zero && rank == 0) {
                needs_shopping = 1;
            }

            /* Zwolnij zasoby */
            queue_remove(nawoz_queue, &nawoz_queue_size, rank);
            queue_remove(lopatki_queue, &lopatki_queue_size, rank);
            queue_remove(grabki_queue, &grabki_queue_size, rank);
            stan = REST;
            println("Skończyłem pielenie, odpoczywam");
            pthread_mutex_unlock(&stateMut);

            /* Wyślij REL w odwrotnej kolejności */
            for (int i = 0; i < size; i++) {
                if (i != rank) {
                    sendPacket(0, i, REL_NAWOZ);
                    sendPacket(0, i, REL_LOPATKI);
                    sendPacket(0, i, REL_GRABKI);
                }
            }
            debug("Rozsyłam REL_NAWOZ, REL_LOPATKI, REL_GRABKI do wszystkich");

            /* Powiadom kupca o braku nawozu */
            if (nawoz_was_zero && rank != 0) {
                sendPacket(0, 0, BRAK_NAWOZU);
            }
            pthread_cond_broadcast(&cond);
            break;
        }

        case ZAKUPY: {
            /* Tylko proces 0 (Kupiec) */
            pthread_mutex_lock(&stateMut);
            needs_shopping = 0;
            pthread_mutex_unlock(&stateMut);

            println("Kupiec robi zakupy...");
            sleep(3 + random() % 3);

            /* Uzupełnij nawóz */
            pthread_mutex_lock(&stateMut);
            if (nawoz_remaining == 0) {
                nawoz_remaining = D;
            } else {
                resupply_enqueue(D);
            }
            pthread_mutex_unlock(&stateMut);

            /* Wyślij UZUPELNIENIE do wszystkich */
            packet_t pkt;
            pkt.data = D;
            for (int i = 0; i < size; i++)
                if (i != rank)
                    sendPacket(&pkt, i, UZUPELNIENIE);

            println("Kupiec wrócił z zakupów, nawóz uzupełniony do %d", D);

            /* Wróć do odpowiedniego stanu */
            pthread_mutex_lock(&stateMut);
            if (zakupy_from_wait) {
                stan = WAIT_NAWOZ;
                println("Kupiec wraca do czekania na nawóz");
            } else {
                stan = REST;
            }
            pthread_mutex_unlock(&stateMut);

            pthread_cond_broadcast(&cond);
            break;
        }

        default:
            break;
        }

        next_iteration:
        ; /* etykieta wymaga instrukcji */
    }
}
