#include <iostream>
#include <vector> // vector<T> collection
#include <fstream> // file stream
#include <string>
#include <cstdlib> // rzutowanie stoi()
#include <signal.h> // sygnał stopu
#include <stdlib.h> // srand, rand
#include <unistd.h>
#include <ctime>
#include <chrono> // do mierzenia czasu
#include <vector>
#include <algorithm>
#include <random>

#define TASKS 8000
#define RCLLENGTH 40
#define FILENAME "test-SDSC-SP2.txt"

using namespace std;


class zadanie {
public:
    int number;
    int submit;
    int run;
    int processorNumber;

    vector<int> processorUsed;

    bool status;

    zadanie() {};

    zadanie(int number, int submit, int run, int proc) {
        this->number = number;
        this->submit = submit;
        this->run = run;
        this->processorNumber = proc;
        this->status = false;

        for (int i = 0; i < proc; i++)
            processorUsed.push_back(-1);
    };
};

class gotoweZadanie {
public:
    int id;
    int start;
    int stop;
    vector<int> procesory;

    gotoweZadanie() {};

    gotoweZadanie(int id, int start, int stop, vector<int> &tablica) {
        this->id = id;
        this->start = start;
        this->stop = stop;

        for (int i = 0; i < tablica.size(); i++)
            procesory.push_back(tablica[i]);
    }
};

vector<zadanie> lista_zapas; // zapasowa lista -> pierwszy Greed

int MaxJobs;
int MaxProcs;
int timee;
int endTime;
unsigned int sumy = 0;

bool quit = false;

/**
 * Funkcja wczytująca z pliku "filename" N_task kolejnych zadań.
 * @param filename
 * @param N_task
 * @return
 */
vector<zadanie> LoadFromFile(string filename, int &N_task) {
    ifstream plik(filename);
    vector<zadanie> lista;
    int valid = 0;

    if (!plik.good()) {
        std::cout << "Błąd wczytywania pliku.\n" << endl;
        throw 69;
    }

    zadanie z1(0, 0, 0, 0); // wymagane dla dodania zadania (w switchu)
    lista.push_back(z1);
    lista.erase(lista.begin());

    string linia;
    while (getline(plik, linia)) // wczytywanie
    {
        string praca = "MaxJobs:";
        string l_proc = "MaxProcs:";

        size_t pozycja = linia.find(praca);
        if (pozycja == string::npos) // MaxJobs - nie znaleziono
        {
            pozycja = linia.find(l_proc);
            if (pozycja == string::npos) {
                pozycja = linia.find(";");
                if (pozycja == string::npos) {
                    int indeks = 0;
                    string str = "";
                    for (int i = 0; i < linia.size(); i++) {
                        if ((linia[i] >= '0' && linia[i] <= '9') || int(linia[i]) == 45)
                            str += linia[i];
                        else if (str.size() > 0) {
                            switch (indeks) {
                                case 0:
                                    z1.number = atoi(str.c_str());
                                    break;
                                case 1:
                                    z1.submit = atoi(str.c_str());
                                    break;
                                case 2:
                                    break; // wait time
                                case 3:
                                    z1.run = atoi(str.c_str());
                                    break;
                                case 4:
                                    z1.processorNumber = atoi(str.c_str());
                                    break;
                                default:
                                    break;
                            }
                            indeks++;
                            str = "";
                        }
                    }

                    if ((z1.number > 0 || z1.run > 0 || z1.processorNumber > 0) && z1.run > 0) {
                        lista.push_back(z1);
                        valid++;
                    };
                }
            } else {
                string liczba;
                for (int i = static_cast<int>(pozycja + l_proc.size()); i < linia.size(); i++)
                    liczba += linia[i];
                MaxProcs = atoi(liczba.c_str());
            }
        } else {
            string liczba;
            for (int i = static_cast<int>(pozycja + praca.size()); i < linia.size(); i++)
                liczba += linia[i];
            MaxJobs = atoi(liczba.c_str());
            if (N_task == -1)
                N_task = MaxJobs;
        }

        if (valid == N_task || plik.eof()) {
            N_task = valid;
            break;
        }
    }
    perror("File open");
    plik.close();
    return lista;
}

/**
 * Funkcja zapisująca do pliku "wynik.txt" gotową listę zadań jako vector<gotoweZadanie>.
 * @param lista
 */
void SaveToFile(vector<gotoweZadanie> lista) {
    ofstream file("wynik.txt", ofstream::out);
    for (int i = 0; i < lista.size(); i++) {
        file << lista[i].id << " " << lista[i].start << " " << lista[i].stop << " ";
        for (int j = 0; j < lista[i].procesory.size(); j++)
            if (j != lista[i].procesory.size() - 1)
                file << lista[i].procesory[j] << " ";
            else if (i != lista.size() - 1)
                file << lista[i].procesory[j] << "\n";
            else file << lista[i].procesory[j];
    };

    file.close();
    cout << "Zapisano do pliku.\n";
}

/**
 * Funkcja szukająca dany element w vectorze.
 * @param lista
 * @param item
 * @return
 */
int find(vector<zadanie> lista, zadanie item) {
    for (int i = 0; i < lista.size(); i++) {
        if (lista[i].number == item.number) {
            return i;
        }
    }
}

/**
 * Funkcja dodająca element do vectora zadań w losowym miejscu.
 * @param lista
 * @param item
 */
void insertRandom(vector<zadanie> &lista, zadanie item) {
    int index = rand() % (int)lista.size();
    lista.insert(lista.begin() + index, item);
}

/**
 * Funkcja szukająca najmniejszego możliwego czasu przeskoku, dla podanego vector<int> procesorów i vector<zadanie> zadań.
 * @param procesory
 * @param lista
 * @return
 */
int closest(vector<int> procesory, vector<zadanie> lista) {
    int min = 0;

    // przyjęcie pierwszej liczby >0
    for (int i = 0; i < procesory.size(); i++) {
        if (procesory[i] > 0) {
            min = procesory[i];
            break;
        };
    }

    // wykonywanie poszczególnych procesów
    for (int i = 0; i < procesory.size(); i++) {
        if (procesory[i] < min && procesory[i] > 0) {
            min = procesory[i];
        }
    }

    // przyjęcie (submit) danego procesu
    for (int i = 0; i < lista.size(); i++) {
        if ((lista[i].submit > timee) && (lista[i].status == false) && (lista[i].submit - timee < min || min == 0)) {
            min = lista[i].submit - timee;
        };
    };

    return min;
}

/**
 * Algorytm zachłanny pobierający vector<zadania> na której działa kolejno układając elementy.
 * @param lista
 * @param lenght
 * @return
 */
vector<gotoweZadanie> GRASP(vector<zadanie> lista) {

    int lenght = RCLLENGTH;
    vector<zadanie> RCL;
    int pointer = 0;
    sumy = 0;

    while (pointer < lenght) {
        RCL.push_back(lista[pointer++]); // zwiększony licznik zadania.
    }
    random_shuffle(RCL.begin(), RCL.end()); // losowe przetasowanie
    random_shuffle(RCL.begin(), RCL.end());

    timee = 0;
    int timestep = 0;
    int Procs = MaxProcs;
    vector<int> procesory;

    for (int i = 0; i < Procs; i++) {
        procesory.push_back(0);
    }

    vector<gotoweZadanie> lista_gotowa;

    while (lista.size() > 0) // lista zadań NIE PUSTA
    {
        for (int i = 0; i < lista.size(); i++) // getIter po wszystkich
        {

            // sprawdzenie zakończenie procesu
            // TAK -> usuń proces z Vectora, zwolnij procesory
            if (lista[i].run == 0) {
                Procs += lista[i].processorNumber;
                for (int j = 0; j < lista[i].processorNumber; j++)
                    procesory[lista[i].processorUsed[j]] = 0;
                lista.erase(lista.begin() + i);
                pointer--;
                i--;
            };
        };

        for (int i = 0; i < RCL.size(); i++) {

            // sprawdzenie czy są wolne procesory
            // zmienna Procs
            // jeśli TAK -> kontynuuj działanie(*)
            // jeśli NIE -> break (for);
            if (Procs == 0) {
                break;
            }

            // sprawdzamy czy wystarczy procesorów
            // jeśli TAK -> sprawdzamy czy jest gotowy && status
            //              jeśli TAK -> dodajemy do gotowych
            //                           procesory--;
            if (Procs >= RCL[i].processorNumber
                && RCL[i].status == false
                && RCL[i].submit <= timee) {
                int temp = 0;
                int index = find(lista, RCL[i]);
                for (int j = 0; j < MaxProcs; j++) {
                    if (procesory[j] == 0 && temp < RCL[i].processorNumber) {
                        procesory[j] = RCL[i].run;
                        RCL[i].processorUsed[temp] = j;
                        lista[index].processorUsed[temp] = j;
                        temp++;
                    } else if (temp >= RCL[i].processorNumber)
                        break;
                };

                gotoweZadanie z1(
                        RCL[i].number,
                        timee,
                        timee + RCL[i].run,
                        RCL[i].processorUsed);

                lista_gotowa.push_back(z1);

                sumy += timee - RCL[i].submit + RCL[i].run;
                lista[index].status = true;
                Procs -= lista[index].processorNumber;

                RCL.erase(RCL.begin()+i);
                if (pointer != lista.size()) // indeks na ostatni element
                    insertRandom(RCL, lista[pointer++]);
            };
        };

        // krok przejścia czasu
        timestep = closest(procesory, RCL);

        // krok przejścia dla .run
        for (int i = 0; i < lista.size(); i++) {
            if (lista[i].status == true) {
                lista[i].run -= timestep;
            }
        }

        // krok przejścia przez czas zajęcia procesorów
        for (int i = 0; i < procesory.size(); i++) {
            if (procesory[i] > 0) {
                procesory[i] -= timestep;
            }
        }

        timee += timestep;
        if (timestep == 0) {
            break;
        }
    };

    return lista_gotowa;
};

vector<gotoweZadanie> zachlanny(vector<zadanie> lista);
void testGRASP(vector<zadanie> lista, vector<gotoweZadanie> &zachlannaLista) {

    int sumGreed = sumy;
    sumy = 0;

    int iteracja = 1;
    while(quit == false) {
        cout << ".\n";
        vector<gotoweZadanie> test = GRASP(lista);
        if (sumGreed > sumy) {
            cout << "Znaleziono rozwiązanie lepsze! :o\n\n";
            cout << "Iteracja " << iteracja <<"\n";
            cout << "Pomiary dla " << lista.size() << " elementów.\n";
            cout << "Suma jakościowa : " << sumy << "\n";
            cout << "Minimalny czas szeregowań : " << timee << "\n";

            SaveToFile(test);
            return;
        }
        iteracja++;
    }
    cout << "Nie znaleziono lepszego rozwiązania w zadanym czasie(" << iteracja << " iteracji)...\n";
    cout << "Zwracanie rozwiązania zachłannego.\n";
    SaveToFile(zachlannaLista);
    return;
}

/**
 * Algorytm zachłanny.
 */
vector<gotoweZadanie> zachlanny(vector<zadanie> lista) {

    sumy = 0;
    timee = 0;
    int timestep = 0;
    int Procs = MaxProcs;
    vector<int> procesory;

    for (int i = 0; i < Procs; i++) {
        procesory.push_back(0);
    }

    vector<gotoweZadanie> lista_gotowa;

    while (lista.size() > 0) // lista zadań NIE PUSTA
    {
        for (int i = 0; i < lista.size(); i++) // getIter po wszystkich
        {

            // sprawdzenie zakończenie procesu
            // TAK -> usuń proces z Vectora, zwolnij procesory
            if (lista[i].run == 0) {
                Procs += lista[i].processorNumber;
                for (int j = 0; j < lista[i].processorNumber; j++)
                    procesory[lista[i].processorUsed[j]] = 0;
                lista.erase(lista.begin() + i);
                i--;
            };
        };

        for (int i = 0; i < lista.size(); i++) {

            // sprawdzenie czy są wolne procesory
            // zmienna Procs
            // jeśli TAK -> kontynuuj działanie(*)
            // jeśli NIE -> break (for);
            if (Procs == 0) {
                break;
            }

            // sprawdzamy czy wystarczy procesorów
            // jeśli TAK -> sprawdzamy czy jest gotowy && status
            //              jeśli TAK -> dodajemy do gotowych
            //                           procesory--;
            if (Procs >= lista[i].processorNumber
                && lista[i].status == false
                && lista[i].submit <= timee) {
                int temp = 0;
                for (int j = 0; j < MaxProcs; j++) {
                    if (procesory[j] == 0 && temp < lista[i].processorNumber) {
                        procesory[j] = lista[i].run;
                        lista[i].processorUsed[temp] = j;
                        temp++;
                    } else if (temp >= lista[i].processorNumber)
                        break;
                };

                gotoweZadanie z1(
                        lista[i].number,
                        timee,
                        timee + lista[i].run,
                        lista[i].processorUsed);

                lista_gotowa.push_back(z1);

                sumy += timee - lista[i].submit + lista[i].run;
                lista[i].status = true;
                Procs -= lista[i].processorNumber;
            };
        };

        // krok przejścia czasu
        timestep = closest(procesory, lista);

        // krok przejścia dla .run
        for (int i = 0; i < lista.size(); i++) {
            if (lista[i].status == true) {
                lista[i].run -= timestep;
            }
        }

        // krok przejścia przez czas zajęcia procesorów
        for (int i = 0; i < procesory.size(); i++) {
            if (procesory[i] > 0) {
                procesory[i] -= timestep;
            }
        }

        timee += timestep;
        if (timestep == 0) {
            break;
        }
    };

    cout << "Sumy zachłannych (sumy) : " << sumy << "\n";
    cout << "Czas końca zachłanny (timee) : " << timee << "\n";

    return lista_gotowa;
};

/**
 * r -> run (najkrótsze)
 * s -> submit (najszybciej przyjęte)
 * @param arr
 * @param l
 * @param m
 * @param r
 * @param sorting
 */
void merge(std::vector<zadanie> &arr, int l, int m, int r, char sorting) {
    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;

    /* tymczasowa tablica */
    std::vector<zadanie> L, R;
    zadanie temp(0,0,0,0);
    for (i = 0; i < n1; i++)
        L.push_back(temp);
    for (j = 0; j < n2; j++)
        R.push_back(temp);


    /* kopiuje zawartość do tymczasowej tablicy */
    for (i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for (j = 0; j < n2; j++)
        R[j] = arr[m + 1 + j];

    i = 0;
    j = 0;
    k = l;
    while (i < n1 && j < n2) {
        // Porównanie jest tutaj
        // r -> run
        // s -> submit
        if (L[i].run <= R[j].run && sorting == 'r') {
            arr[k] = L[i];
            i++;
        } else if (L[i].submit <= R[j].submit && sorting == 's') {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    /* Kopiuje pozostałości listy */
    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
}

/**
 * Funkcja sortująca metodą MergeSort. Zmienna 'sorting' odpowiada za wybór parametru sortującego.
 * r -> run
 * s -> submit
 * @param arr
 * @param l
 * @param r
 * @param sorting
 */
void mergeSort(std::vector<zadanie> &arr, int l, int r, char sorting) {
    if (l < r) {
        int m = l + (r-l)/2;

        mergeSort(arr, l, m, sorting);
        mergeSort(arr, m+1, r, sorting);

        merge(arr, l, m, r, sorting);
    }
}


void stop(int signo) {
    cout << "Napotkano sygnał stopu, program trwa za długo\n";
    cout << "Numer sygnału : " <<signo << "\n";
    quit = true; // zmienna exit -> wymuszająca wyplucie wyniku
}

chrono::time_point<chrono::system_clock> startPomiar() {
    return chrono::high_resolution_clock::now();
}

double stopPomiar(chrono::time_point<chrono::system_clock> start) {
    auto stop = chrono::high_resolution_clock::now();
    chrono::duration<double> czas = stop - start;
    return czas.count();
}

int main(int argc, char *argv[]) {
    // ./a.out "fileName" N_task
    string filename = argv[1];

    // ustawianie N-task'ów
    int N_task;
    if (argc == 2) {
        N_task = 5000; // domyślnie 5000
    } else {
        N_task = atoi(argv[2]);
    }

    vector<zadanie> lista;
    // ŁADOWANIE N KOLEJNYCH ZADAŃ Z PLIKU
    try {
        lista = LoadFromFile(filename, N_task);
    }
    catch ( ... ) {
        perror("Load from file");
        return 1;
    }

    if (N_task < 0 || N_task >= MaxJobs) {
        N_task = MaxJobs;
    }

    cout << "N_task = " << N_task << ", MaxJobs = " << MaxJobs << "\n\n";

    for (int i = 0; i < lista.size(); i++) // Błąd konstruktora (-1) = nieużywany procesor
    {
        for (int j = 0; j < lista[i].processorNumber; j++)
            lista[i].processorUsed.push_back(-1);
    };

    signal(SIGALRM, stop); // quit == true, przy wykonaniu
    alarm(300); // sygnał gdy przekroczony czas = 5 minut.

    srand((unsigned int)time(NULL));
    auto start = startPomiar();

    mergeSort(lista, 0, (int)lista.size()-1, 'r'); // sortowanie po czasie 'run'
    mergeSort(lista, 0, (int)lista.size()-1, 's'); // sortowanie po czasie 'submit'

    vector<gotoweZadanie> zachlannaLista = zachlanny(lista);

    double durationZachlanny = stopPomiar(start);
    cout << "Czas trwania programu zachłannego : " << durationZachlanny << "s\n";


    auto startGRASP = startPomiar();

    testGRASP(lista, zachlannaLista);

    double durationGRASP = stopPomiar(startGRASP);

    cout << "Czas trwania prograu : " << durationGRASP << "s\n";

    return 0;
};