#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define MAX_TEXTO 256
#define MAX_NOMBRE 50

struct mensaje {
    long mtype;         // Tipo de mensaje
    int  cola_id;       // Lo usa el servidor para devolver cola de sala
    long reply_to;
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

#define MSGSZ (sizeof(struct mensaje) - sizeof(long))

int  cola_global = -1;
int  cola_sala   = -1;
long mytype      = 0;
char nombre_usuario[MAX_NOMBRE];
char sala_actual[MAX_NOMBRE] = "";

// Hilo receptor: escucha SOLO lo dirigido al cliente
void *recibir_mensajes(void *arg) {
    struct mensaje msg;
    for (;;) {
        if (cola_sala != -1) {
            if (msgrcv(cola_sala, &msg, MSGSZ, mytype, 0) == -1) {
                if (errno == ENOMSG) {
                    continue;
                }
                perror("Error al recibir mensaje de la sala");
                usleep(100000);
                continue;
            }

            // Mostrar (ya viene filtrado por mtype)
            printf("\033[1m%s: %s\033[0m\n", msg.remitente, msg.texto);
            printf("> ");
            fflush(stdout);
        }
        usleep(8000); // Evitar consumo de CPU excesivo
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <nombre_usuario>\n", argv[0]);
        exit(1);
    }

    strncpy(nombre_usuario, argv[1], MAX_NOMBRE - 1);
    nombre_usuario[MAX_NOMBRE - 1] = '\0';

    // Conectarse a la cola global
    key_t key_global = ftok("/tmp", 'A');
    cola_global = msgget(key_global, 0666);
    if (cola_global == -1) {
        perror("Error al conectar a la cola global");
        exit(1);
    }

    // ID único de recepción
    mytype = (long)getpid();

    printf("Bienvenido, %s. Comandos: join <sala>, luego escribe para chatear.\n", nombre_usuario);

    // Hilo receptor
    pthread_t hilo_receptor;
    pthread_create(&hilo_receptor, NULL, recibir_mensajes, NULL);

    struct mensaje msg;
    char comando[MAX_TEXTO];

    for (;;) {
        printf("> ");
        fflush(stdout);
        if (!fgets(comando, MAX_TEXTO, stdin)) break;
        comando[strcspn(comando, "\n")] = '\0';

        if (strncmp(comando, "join ", 5) == 0) {
            // Parse sala
            char sala[MAX_NOMBRE] = {0};
            sscanf(comando, "join %49s", sala);
            if (sala[0] == '\0') {
                printf("Uso: join <sala>\n");
                continue;
            }

            // Enviar JOIN al servidor por cola_global
            memset(&msg, 0, sizeof(msg));
            msg.mtype    = 1;
            msg.reply_to = mytype;
            strncpy(msg.remitente, nombre_usuario, MAX_NOMBRE - 1);
            strncpy(msg.sala, sala, MAX_NOMBRE - 1);

            if (msgsnd(cola_global, &msg, MSGSZ, 0) == -1) {
                perror("Error al enviar solicitud de JOIN");
                continue;
            }

            // Esperar confirmación dirigida a mí (mtype == mytype)
            memset(&msg, 0, sizeof(msg));
            if (msgrcv(cola_global, &msg, MSGSZ, mytype, 0) == -1) {
                perror("Error al recibir confirmación JOIN");
                continue;
            }

            // Guardar cola de sala y sala actual
            cola_sala = msg.cola_id;
            if (cola_sala == -1) {
                fprintf(stderr, "JOIN falló (cola_sala inválida)\n");
                continue;
            }
            strncpy(sala_actual, sala, MAX_NOMBRE - 1);
            sala_actual[MAX_NOMBRE - 1] = '\0';

            printf("%s (cola_id=%d)\n", msg.texto, cola_sala);

        } else if (strlen(comando) > 0) {
            if (sala_actual[0] == '\0') {
                printf("No estás en ninguna sala. Usa 'join <sala>' primero.\n");
                continue;
            }

            // Enviar mensaje al servidor (no directo a cola_sala) para que haga fan-out
            memset(&msg, 0, sizeof(msg));
            msg.mtype    = 3; // MSG
            msg.reply_to = mytype;
            strncpy(msg.remitente, nombre_usuario, MAX_NOMBRE - 1);
            strncpy(msg.sala, sala_actual, MAX_NOMBRE - 1);
            strncpy(msg.texto, comando, MAX_TEXTO - 1);

            if (msgsnd(cola_global, &msg, MSGSZ, 0) == -1) {
                perror("Error al enviar mensaje al servidor");
                continue;
            }
        }
    }

    return 0;
}
