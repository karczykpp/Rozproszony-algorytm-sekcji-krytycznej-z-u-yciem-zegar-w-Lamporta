DEMONSTRACJA DZIAŁANIA WĄTKÓW W MPI
===================================
Repozytorium zawiera prosty przykład projektu z użyciem wątków. Można wykorzystać ten projekt
jako szkielet dla własnych rozwiązań.

Struktura:

- main.h <- makra debug oraz println, zmienne współdzielone przez wątki
- util.h <- wysyłanie wiadomości, inicjalizacja, zmienne współdzielone używane przez funkcje z util.c
- watek_glowny.h <- główna pętla programu. 
- watek_komunikacyjny.h <- wątek odpowiedzialny za odbieranie wiadomości.
 
ZADANIE:
=======
Zaimplementować zegary lamporta oraz dowolny algorytm dostępu do sekcji krytycznej.
Zaimplementować wyświetlanie zegarów w makrach println oraz debug.
1) stworzyć zmienną globalną w jakimś pliku .c oraz zapowiedź (extern) w jakimś pliku .h
2) zmodyfikować makra println i debug w pliku main.h by wyświetlały zegar lamporta
3) przy wysyłaniu (sendPacket) zwiększać lokalny zegary lamporta, dołączać jako pole ts do wiadomości
4) przy odbieraniu (wątek komunikacyjny) max( ts, lokalny zegar ) +1
5) pamiętać o obwarowaniu dostępu do zegara lamporta muteksami

UWAGA! TO JUŻ JEST:
w struct packet_t jest już pole "ts" - to będzie timestamp.
Trzeba dodać zmienną clock i obwarować ją muteksami.

KOMPILACJA, URUCHOMIENIE
========================

Najprościej kompilować używając załączonego Makefile.

    make clean # usuwa pliki wykonywalne

    make # kompiluje

    make run # uruchamia

Jeżeli nie działa make, może to być spowodowane brakiem polecenia ctags. W takim wypadku należy
wykomentować regułę ctags z Makefile, albo wyrzucić ją z zależności dla reguły all
