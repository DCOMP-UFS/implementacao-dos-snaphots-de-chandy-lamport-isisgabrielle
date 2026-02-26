#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mpi.h>
#include <unistd.h>

#define NPROC 3
#define BUFFER_SIZE 5
#define MARKER_VAL -1

typedef struct {
    int p[NPROC];
} Clock;

typedef struct {
    Clock clock;
    int source_id;
    int destination_id;
} Message;

Message fila_recepcao[BUFFER_SIZE];
Message fila_envio[BUFFER_SIZE];

int count_recepcao = 0;
int count_envio = 0;

pthread_mutex_t mutex_recepcao = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_envio = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t recepcao_tem_item = PTHREAD_COND_INITIALIZER;
pthread_cond_t recepcao_tem_espaco = PTHREAD_COND_INITIALIZER;

pthread_cond_t envio_tem_item = PTHREAD_COND_INITIALIZER;
pthread_cond_t envio_tem_espaco = PTHREAD_COND_INITIALIZER;

int my_rank;

int em_snapshot = 0;
int snapshot_concluido = 0;
Clock estado_salvo;
Message canal_estado[NPROC][50];
int canal_count[NPROC] = {0};
int marcador_recebido[NPROC] = {0};

void adicionar_na_fila_recebimento(Message msg) {
    pthread_mutex_lock(&mutex_recepcao);
    while (count_recepcao == BUFFER_SIZE)
        pthread_cond_wait(&recepcao_tem_espaco, &mutex_recepcao);

    fila_recepcao[count_recepcao++] = msg;

    pthread_mutex_unlock(&mutex_recepcao);
    pthread_cond_signal(&recepcao_tem_item);
}

Message remover_da_fila_recebimento() {
    pthread_mutex_lock(&mutex_recepcao);
    while (count_recepcao == 0)
        pthread_cond_wait(&recepcao_tem_item, &mutex_recepcao);

    Message msg = fila_recepcao[0];
    for (int i = 0; i < count_recepcao - 1; i++)
        fila_recepcao[i] = fila_recepcao[i + 1];

    count_recepcao--;

    pthread_mutex_unlock(&mutex_recepcao);
    pthread_cond_signal(&recepcao_tem_espaco);
    return msg;
}

void adicionar_na_fila_envio(Message msg) {
    pthread_mutex_lock(&mutex_envio);
    while (count_envio == BUFFER_SIZE)
        pthread_cond_wait(&envio_tem_espaco, &mutex_envio);

    fila_envio[count_envio++] = msg;

    pthread_mutex_unlock(&mutex_envio);
    pthread_cond_signal(&envio_tem_item);
}

Message remover_da_fila_envio() {
    pthread_mutex_lock(&mutex_envio);
    while (count_envio == 0)
        pthread_cond_wait(&envio_tem_item, &mutex_envio);

    Message msg = fila_envio[0];
    for (int i = 0; i < count_envio - 1; i++)
        fila_envio[i] = fila_envio[i + 1];

    count_envio--;

    pthread_mutex_unlock(&mutex_envio);
    pthread_cond_signal(&envio_tem_espaco);
    return msg;
}

void IniciarSnapshot(int pid, Clock *clock) {
    printf("[P%d] ---------- INICIANDO SNAPSHOT (INICIADOR) ----------\n", pid);
    em_snapshot = 1;
    estado_salvo = *clock;
    marcador_recebido[pid] = 1; 

    for (int i = 0; i < NPROC; i++) {
        if (i != pid) {
            Clock marker = {{MARKER_VAL, MARKER_VAL, MARKER_VAL}};
            Message msg;
            msg.clock = marker;
            msg.source_id = pid;
            msg.destination_id = i;
            adicionar_na_fila_envio(msg);
        }
    }
}

void VerificarConclusaoSnapshot(int pid) {
    if (snapshot_concluido) return;

    int todos_recebidos = 1;
    for (int i = 0; i < NPROC; i++) {
        if (!marcador_recebido[i]) {
            todos_recebidos = 0;
            break;
        }
    }

    if (todos_recebidos) {
        snapshot_concluido = 1;
        printf("\n========= RESULTADO PARCIAL SNAPSHOT [P%d] =========\n", pid);
        printf("Estado Salvo (Relogio Local): (%d, %d, %d)\n", 
               estado_salvo.p[0], estado_salvo.p[1], estado_salvo.p[2]);
        
        for (int i = 0; i < NPROC; i++) {
            if (i != pid) {
                printf("Estado do Canal C(%d -> %d): { ", i, pid);
                for (int j = 0; j < canal_count[i]; j++) {
                    printf("(%d,%d,%d) ", 
                        canal_estado[i][j].clock.p[0], 
                        canal_estado[i][j].clock.p[1], 
                        canal_estado[i][j].clock.p[2]);
                }
                printf("}\n");
            }
        }
        printf("====================================================\n\n");
    }
}

int ProcessarMensagem(int pid, Clock *clock, Message msg) {
    if (msg.clock.p[0] == MARKER_VAL && msg.clock.p[1] == MARKER_VAL && msg.clock.p[2] == MARKER_VAL) {
        if (!em_snapshot) {
            printf("[P%d] ---------- INICIANDO SNAPSHOT (PARTICIPANTE - Disparado por P%d) ----------\n", pid, msg.source_id);
            em_snapshot = 1;
            
            estado_salvo = *clock;
            marcador_recebido[pid] = 1;

            for (int i = 0; i < NPROC; i++) {
                if (i != pid) {
                    Clock marker = {{MARKER_VAL, MARKER_VAL, MARKER_VAL}};
                    Message m;
                    m.clock = marker;
                    m.source_id = pid;
                    m.destination_id = i;
                    adicionar_na_fila_envio(m);
                }
            }
        }

        marcador_recebido[msg.source_id] = 1;
        VerificarConclusaoSnapshot(pid);
        
        return 0; 
        
    } else {
        clock->p[pid]++;
        for (int i = 0; i < NPROC; i++) {
            if (clock->p[i] < msg.clock.p[i])
                clock->p[i] = msg.clock.p[i];
        }

        printf("[P%d] RECEIVE ← %d | Clock (%d,%d,%d)\n",
               pid, msg.source_id, clock->p[0], clock->p[1], clock->p[2]);

        if (em_snapshot && !marcador_recebido[msg.source_id]) {
            canal_estado[msg.source_id][canal_count[msg.source_id]] = msg;
            canal_count[msg.source_id]++;
        }

        return 1;
    }
}

void Event(int pid, Clock *clock) {
    clock->p[pid]++;
    printf("[P%d] EVENT | Clock (%d,%d,%d)\n",
           pid, clock->p[0], clock->p[1], clock->p[2]);
}

void Send(int pid, int dest, Clock *clock) {
    clock->p[pid]++;

    Message msg;
    msg.clock = *clock;
    msg.source_id = pid;
    msg.destination_id = dest;

    adicionar_na_fila_envio(msg);

    printf("[P%d] SEND → %d | Clock (%d,%d,%d)\n",
           pid, dest, clock->p[0], clock->p[1], clock->p[2]);
}

void Receive(int pid, Clock *clock) {
    while(1) {
        Message msg = remover_da_fila_recebimento();
        if (ProcessarMensagem(pid, clock, msg) == 1) {
            break;
        }
    }
}

void* thread_receptora(void* arg) {
    while (1) {
        Message msg;
        MPI_Status status;

        MPI_Recv(msg.clock.p, NPROC, MPI_INT,
                 MPI_ANY_SOURCE, MPI_ANY_TAG,
                 MPI_COMM_WORLD, &status);

        msg.source_id = status.MPI_SOURCE;
        adicionar_na_fila_recebimento(msg);
    }
}

void* thread_emissora(void* arg) {
    while (1) {
        Message msg = remover_da_fila_envio();
        MPI_Send(msg.clock.p, NPROC, MPI_INT,
                 msg.destination_id, my_rank, MPI_COMM_WORLD);
    }
}

void* thread_central(void* arg) {
    Clock clock = {{0,0,0}};

    printf("[P%d] Clock inicial (%d,%d,%d)\n",
           my_rank, clock.p[0], clock.p[1], clock.p[2]);

    if (my_rank == 0) {
        Event(0, &clock);      
        Send(0, 1, &clock);    
        
        IniciarSnapshot(0, &clock); 
        
        Receive(0, &clock);   
        Send(0, 2, &clock);   
        Receive(0, &clock);    
        Send(0, 1, &clock);
        Event(0, &clock); 
    }
    else if (my_rank == 1) {
        Send(1, 0, &clock);
        Receive(1, &clock);    
        Receive(1, &clock);   
    }
    else if (my_rank == 2) {
        Event(2, &clock);   
        sleep(1);
        Send(2, 0, &clock);   
        Receive(2, &clock);   
    }

    printf("[P%d] Clock final (%d,%d,%d)\n",
           my_rank, clock.p[0], clock.p[1], clock.p[2]);

    while (!snapshot_concluido) {
        Message msg = remover_da_fila_recebimento();
        ProcessarMensagem(my_rank, &clock, msg);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    sleep(1);
    MPI_Abort(MPI_COMM_WORLD, 0);
    
    return NULL;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    pthread_t thread_receptora_id, thread_emissora_id, thread_central_id;

    pthread_create(&thread_receptora_id, NULL, thread_receptora, NULL);
    pthread_create(&thread_emissora_id, NULL, thread_emissora, NULL);
    pthread_create(&thread_central_id, NULL, thread_central, NULL);

    pthread_join(thread_central_id, NULL);

    MPI_Finalize();
    return 0;
}