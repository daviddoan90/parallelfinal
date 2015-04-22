/* 
 * File:   main.cpp
 * Author: david
 *
 * Created on April 15, 2015, 10:05 PM
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <mpi.h>
#include <string.h> 
#include <cstdlib>
#include <iostream>
#include <fstream>

using namespace std;
void reductionPhase();
void computePi(int j, int i);
void backSubstitutionPhase();
void array_remove(double* arr, int size, int removeIdx);
void debug_array(double* arr, int size, string msg);
void debug(string msg, double varr);

//vars
long str_len;
int noOfProcesses;
int myId;
double s_time, e_time;

int N, M, n;

double *A, *B, *C, *D, *X;

double x;
double** P = new double*[n];
double* Po;
double* Pnext;
double* Pprevious;
int h;

/*
 * 
 */
int main(int argc, char** argv) {

    //communication
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &noOfProcesses);
    MPI_Comm_rank(MPI_COMM_WORLD, &myId);

    N = noOfProcesses;
    M = N - 1;
    n = log2(N);

    // wait for all to come together
    MPI_Barrier(MPI_COMM_WORLD);
    s_time = MPI_Wtime();

    //run
    if (myId == 0) {
        //broadcast initial values
        A = new double[N] {
            0, 0, 1, 1, 1, 1, 1, 1
        };
        
        B = new double[N] {
            0, -4, -3, -3, -3, -3, -3, -4
        };
        C = new double[N] {
            0, 1, 1, 1, 1, 1, 1, 0
        };
        D = new double[N] {
            0, -3, -2, -2, -2, -2, -2, -3
        };

        Po = new double[N*4];
        for (int i = 0, j = 0; i < N; i++) {
            Po[j++] = A[i];
            Po[j++] = B[i];
            Po[j++] = C[i];
            Po[j++] = D[i];
        }
    }

    //broadcast values
    debug_array(Po,N*4,"@@@@@@@@@@@@@@@@@@@@@@@@@ Po");
    P[0] = new double[4];
    MPI_Scatter(Po, 4, MPI_DOUBLE, P[0], 4, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    debug_array(P[0],4,"%%%%%%%%%%%%%%%%%%%%%%%%% P[0]");
    return 0;
    if (myId > 0) {

        reductionPhase();
        backSubstitutionPhase();
    }

    if (myId == 0) {
        X = new double[N];
    }
    MPI_Gather(&x, 1, MPI_DOUBLE, X, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);


    MPI_Barrier(MPI_COMM_WORLD);
    e_time = MPI_Wtime();

    if (myId == 0) {
        array_remove(X, N, 0);
        debug_array(X, M, "array X");
        printf("Run time: %f seconds\n", (e_time - s_time));
    }

    MPI_Finalize();
    return 0;
}

void backSubstitutionPhase() {
    int k = pow(2, n - 1);
    if (myId == k) {
        //debug_array(P[n - 1], 4, "Pn-1");
        x = P[n - 1][3] / P[n - 1][1];
        //printf("k:%i,id:%i - x = %f\n", k, myId, x);
    }

    for (int k = n - 1; k > 0; k--) {
        int h = pow(2, k - 1);
        double xpre, xnext;
        int t = 1;
        for (int i = t * h; i <= N - h; ++t, i = t * h) {
            if (myId == i) {
                if (t % 2 == 0) {
                    //                    printf("k:%i,h:%i - Processor %i send %f to %i\n", k, h, myId, x, myId + h);
                    MPI_Send(&x, 1, MPI_DOUBLE, myId + h, 0, MPI_COMM_WORLD);
                    //                    printf("k:%i,h:%i - Processor %i send %f to %i\n", k, h, myId, x, myId - h);
                    MPI_Send(&x, 1, MPI_DOUBLE, i - h, 0, MPI_COMM_WORLD);
                } else {
                    if (myId + h <= N - h) {
                        //printf("k:%i,h:%i - Processor %i receive from %i\n", k, h, myId, myId + h);
                        MPI_Recv(&xnext, 1, MPI_DOUBLE, myId + h, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        //                        printf("k:%i,h:%i - Processor %i received %f from %i\n", k, h, myId, xnext, myId + h);
                    }

                    if (myId - h >= h) {
                        //                        printf("k:%i,h:%i - Processor %i receive from %i\n", k, h, myId, myId - h);
                        MPI_Recv(&xpre, 1, MPI_DOUBLE, i - h, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        //                        printf("k:%i,h:%i - Processor %i received %f from %i\n", k, h, myId, xpre, myId - h);
                    }

                    x = (P[k - 1][3] - P[k - 1][0] * xpre - P[k - 1][2] * xnext) / (P[k - 1][1]);
                }
            }

        }
    }
}

void reductionPhase() {
    for (int j = 1; j < n; j++) {

        h = pow(2, j - 1);
        Pnext = new double[4];
        Pprevious = new double[4];

        for (int i = pow(2, j); i < pow(2, n); i += pow(2, j)) {

            //debug("Reduction: i", (double)i)            
            if (myId == i - h) {
                //                printf("Processor %i send %f to %i\n", myId, P[j - 1][0], i);
                MPI_Send(P[j - 1], 4, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
            } else if (myId == i + h) {
                //printf("Processor %i send %f to %i\n", myId, P[j - 1][0], i);
                MPI_Send(P[j - 1], 4, MPI_DOUBLE, i, 1, MPI_COMM_WORLD);

            } else if (myId == i) {

                //printf("Processor %i receive from %i and %i\n", myId, i - h, i + h);
                MPI_Recv(Pprevious, 4, MPI_DOUBLE, i - h, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                //printf("Processor %i received %f from %i\n", myId, Pprevious[0], i - h);
                MPI_Recv(Pnext, 4, MPI_DOUBLE, i + h, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                //printf("Processor %i received %f from %i\n", myId, Pnext[0], i + h);
                computePi(j, i);

            }
        }
    }
}

void computePi(int j, int i) {
    //init
    double e, f;

    //debug("(B[j - 1][i - h])", (B[j - 1][i - h]));
    //debug_array(B[j - 1], M, "B");

    //debug_array(P[j - 1], 4, "P " + std::to_string(j - 1));

    if (i <= 0 or i >= N) {
        P[j] = new double[4] {
            0, 1, 0, 0
        };
        x = 0;
    } else {

        P[j] = new double[4];

        //debug_array(Pprevious, 4, "Pprevious ");
        //debug_array(Pnext, 4, "Pnext ");

        e = -(P[j - 1][0]) / (Pprevious[1]);

        f = -(P[j - 1][2]) / (Pnext[1]);

        P[j][0] = e * Pprevious[0];
        P[j][2] = f * Pnext[2];
        P[j][1] = P[j - 1][1] + e * Pprevious[2] + f * Pnext[0];
        P[j][3] = P[j - 1][3] + e * Pprevious[3] + f * Pnext[3];

        //debug_array(P[j], 4, "P" + std::to_string(j) + " of processor " + std::to_string(myId));
        //printf("Processor %i computePi(j:%i, i:%i, h:%i, e:%f, f:%f)\n", i, j, i, h, e, f);

    }


}

//debug functions

void debug(string msg, double varr) {

    cout << "\nDebugging....." << msg << ": <" << varr << ">\n";
    fflush(stdout);
}

//debug a array

void debug_array(double* arr, int size, string msg = "") {
    int i = 0;
    cout << "\nDebugging..." << msg << ": ";

    for (; i < size; i++)
        cout << arr[i] << "  ";
    cout << "\n";

    fflush(stdout);
}

void array_show(double* arr, int size) {
    for (int i = 0; i < size; i++)
        cout << arr[i] << "  ";
}

void array_remove(double* arr, int size, int removeIdx) {
    for (int i = removeIdx + 1; i < size; i++) {
        arr[i - 1] = arr[i];
    }
}