Przetwarzanie Rozproszone - MPI + watki + zegary Lamporta
==========================================================

Projekt prezentuje symulacje rozproszona oparta o MPI i watki POSIX.
Kazdy proces reprezentuje ogrodnika, ktory cyklicznie ubiega sie o wspoldzielone zasoby:

- grabki (pojemnosc G)
- lopatki (pojemnosc L)
- nawoz (zmienna liczba dostepnych jednostek)

Dodatkowo proces o randze 0 pelni role Kupca i uzupelnia nawoz, gdy ten sie skonczy.

Glowne elementy rozwiazania
===========================

1. Zegary Lamporta

- kazdy komunikat przenosi znacznik czasu ts
- przy wysylce lokalny zegar jest inkrementowany
- przy odbiorze stosowana jest regula:

  ``lamport_clock = max(lamport_clock, pakiet.ts) + 1``

- dostep do zegara i wspolnego stanu jest chroniony muteksem

2. Sekcje krytyczne i kolejkowanie

- dla kazdego zasobu prowadzona jest osobna kolejka zadan uporzadkowana po (timestamp, pid)
- wejscie do sekcji krytycznej wymaga:

  - odpowiedniej pozycji w kolejce (zaleznej od pojemnosci zasobu)
  - potwierdzenia postepu logicznego od pozostalych procesow (ACK/Lamport)

3. Komunikacja MPI

Wykorzystywane typy komunikatow:

- ``REQ_GRABKI`` / ``REL_GRABKI``
- ``REQ_LOPATKI`` / ``REL_LOPATKI``
- ``REQ_NAWOZ`` / ``REL_NAWOZ``
- ``ACK``
- ``BRAK_NAWOZU``
- ``UZUPELNIENIE``

Architektura plikow
===================

- ``main.c`` - inicjalizacja MPI, tworzenie watkow, start programu
- ``main.h`` - makra logowania, deklaracje wspoldzielonych zmiennych
- ``util.c`` / ``util.h`` - typy wiadomosci, kolejki, zegar Lamporta, wysylka pakietow
- ``watek_glowny.c`` / ``watek_glowny.h`` - logika stanow procesu (REST, WAIT_*, PIELENIE, ZAKUPY)
- ``watek_komunikacyjny.c`` / ``watek_komunikacyjny.h`` - odbior wiadomosci i aktualizacja stanu
- ``config.txt`` - parametry symulacji (G, L, D)

Konfiguracja
============

Plik ``config.txt``:

.. code-block:: text

    G=4
    L=3
    D=2

Gdzie:

- G - liczba jednoczesnie dostepnych grabek
- L - liczba jednoczesnie dostepnych lopatek
- D - poczatkowa porcja nawozu (oraz wielkosc jednego uzupelnienia)

Wymagania
=========

- kompilator C (gcc/mpicc)
- MPI (np. OpenMPI)
- make
- pthread (standardowo dostepny z libc na Linux/macOS)

Budowanie i uruchamianie
========================

.. code-block:: bash

    make clean
    make
    make run

Domyslnie ``make run`` uruchamia 8 procesow:

.. code-block:: bash

    mpirun --mca mpi_yield_when_idle 1 -oversubscribe -np 8 ./main

Aby uruchomic inna liczbe procesow, zmien parametr ``-np`` w Makefile
lub uruchom polecenie recznie.

Logi i debug
============

- ``println(...)`` wyswietla komunikaty zawsze
- ``debug(...)`` dziala po kompilacji z flaga ``-DDEBUG``
- logi zawieraja m.in. identyfikator procesu oraz aktualny zegar Lamporta

Uwagi
=====

- Projekt jest dobrym punktem wyjscia do testowania algorytmow wzajemnego wykluczania w systemach rozproszonych.
- Kod zaklada wspoldzielony model komunikacji oparty o komunikaty i porzadek Lamporta.
- W razie problemow z ``ctags`` mozna usunac regule ``tags`` z Makefile lub nie wywolywac tego celu.
