#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SALAS 10
#define MAX_USUARIOS_POR_SALA 20
#define MAX_TEXTO 256
#define MAX_NOMBRE 50

// ----- Estructuras -----

struct mensaje {
    long mtype;                 // Tipo de mensaje (no cuenta en payload)
    int  cola_id;               // Para pasar el ID de la cola de la sala en respuestas
    long reply_to;              // <-- NUEVO: canal de respuesta del cliente (PID)
    char remitente[MAX_NOMBRE];
    char texto[MAX_TEXTO];
    char sala[MAX_NOMBRE];
};

struct usuario {
    char nombre[MAX_NOMBRE];
    long reply_to;              // <-- NUEVO: PID del cliente para direccionar mtype
};

struct sala {
    char nombre[MAX_NOMBRE];
    int  cola_id;               // ID de la cola de mensajes de la sala
    int  num_usuarios;
    struct usuario usuarios[MAX_USUARIOS_POR_SALA];
};

static struct sala salas[MAX_SALAS];
static int num_salas = 0;

// Tamaño de payload que se usa en msgsnd/msgrcv
#define MSGSZ (sizeof(struct mensaje) - sizeof(long))

// ----- Helpers -----

static int crear_sala(const char *nombre) {
    if (num_salas >= MAX_SALAS) return -1;

    // Generar key (puedes mejorar esto con un archivo por sala + proj_id fijo)
    key_t key = ftok("/tmp", num_salas + 1);
    int cola_id = msgget(key, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear la cola de la sala");
        return -1;
    }

    strncpy(salas[num_salas].nombre, nombre, MAX_NOMBRE - 1);
    salas[num_salas].nombre[MAX_NOMBRE - 1] = '\0';
    salas[num_salas].cola_id = cola_id;
    salas[num_salas].num_usuarios = 0;

    num_salas++;
    return num_salas - 1;
}

static int buscar_sala(const char *nombre) {
    for (int i = 0; i < num_salas; i++) {
        if (strcmp(salas[i].nombre, nombre) == 0) {
            return i;
        }
    }
    return -1;
}

static int agregar_usuario_a_sala(int indice_sala, const char *nombre_usuario, long reply_to) {
    if (indice_sala < 0 || indice_sala >= num_salas) return -1;

    struct sala *s = &salas[indice_sala];
    if (s->num_usuarios >= MAX_USUARIOS_POR_SALA) return -1;

    // Verificar si ya está
    for (int i = 0; i < s->num_usuarios; i++) {
        if (strcmp(s->usuarios[i].nombre, nombre_usuario) == 0) {
            // Actualiza reply_to por si cambió (p.ej. reconexión)
            s->usuarios[i].reply_to = reply_to;
            return 0;
        }
    }

    // Agregar
    strncpy(s->usuarios[s->num_usuarios].nombre, nombre_usuario, MAX_NOMBRE - 1);
    s->usuarios[s->num_usuarios].nombre[MAX_NOMBRE - 1] = '\0';
    s->usuarios[s->num_usuarios].reply_to = reply_to;
    s->num_usuarios++;
    return 0;
}

// Envía msg (ya con texto/remitente/sala) a TODOS los usuarios de la sala;
// cambia mtype a reply_to de cada usuario y usa la cola de la sala para fan-out.
static void broadcast_a_sala(int indice_sala, const struct mensaje *in, long exclude) {
    if (indice_sala < 0 || indice_sala >= num_salas) return;
    struct sala *s = &salas[indice_sala];

    for (int i = 0; i < s->num_usuarios; i++) {
        if (s->usuarios[i].reply_to == exclude) {
            continue; // <-- NO enviar al emisor
        }
        struct mensaje out = *in;
        out.mtype = s->usuarios[i].reply_to;  // dirigido a ese usuario
        if (msgsnd(s->cola_id, &out, MSGSZ, 0) == -1) {
            perror("Error al enviar broadcast a la sala");
        }
    }
}

// ----- main -----

int main(void) {
    // Cola global
    key_t key_global = ftok("/tmp", 'A');
    int cola_global = msgget(key_global, IPC_CREAT | 0666);
    if (cola_global == -1) {
        perror("Error al crear la cola global");
        exit(1);
    }

    printf("Servidor de chat iniciado. Esperando clientes...\n");

    struct mensaje msg;

    for (;;) {
        if (msgrcv(cola_global, &msg, MSGSZ, 0, 0) == -1) {
            perror("Error al recibir en global");
            continue;
        }

        if (msg.mtype == 1) { // JOIN
            // msg.sala, msg.remitente, msg.reply_to están definidos por el cliente
            int indice = buscar_sala(msg.sala);
            if (indice == -1) {
                indice = crear_sala(msg.sala);
                if (indice == -1) {
                    struct mensaje resp = {0};
                    resp.mtype    = msg.reply_to;     // dirigido al cliente
                    resp.cola_id  = -1;
                    resp.reply_to = 0;
                    strncpy(resp.remitente, "Servidor", MAX_NOMBRE-1);
                    strncpy(resp.sala, msg.sala, MAX_NOMBRE-1);
                    snprintf(resp.texto, MAX_TEXTO,
                            "No se pudo crear la sala '%s': límite de salas alcanzado.",
                            msg.sala);

                    if (msgsnd(cola_global, &resp, MSGSZ, 0) == -1) {
                        perror("Error al responder JOIN (limite salas)");
                    }
                    break;
                }
                printf("Nueva sala creada: %s\n", msg.sala);
            }

            if (agregar_usuario_a_sala(indice, msg.remitente, msg.reply_to) == 0) {
                printf("El usuario %s se ha unido a sala %s (reply_to=%ld)\n",
                       msg.remitente, msg.sala, msg.reply_to);

                // Responder SOLO a este cliente por cola_global usando su reply_to como mtype
                struct mensaje resp = {0};
                resp.mtype   = msg.reply_to;                 // <-- dirigido
                resp.cola_id = salas[indice].cola_id;        // <-- cola de la sala
                resp.reply_to = 0;
                strncpy(resp.remitente, "Servidor", MAX_NOMBRE - 1);
                strncpy(resp.sala, msg.sala, MAX_NOMBRE - 1);
                snprintf(resp.texto, MAX_TEXTO, "Te uniste a '%s'", msg.sala);

                if (msgsnd(cola_global, &resp, MSGSZ, 0) == -1) {
                    perror("Error al enviar confirmación JOIN");
                }
            } else {
                fprintf(stderr, "No se pudo agregar usuario %s a sala %s\n",
                        msg.remitente, msg.sala);
            }

        } else if (msg.mtype == 3) { // MSG desde cliente -> hacer fan-out
            int indice = buscar_sala(msg.sala);
            if (indice == -1) {
                // Si la sala no existe, ignora o responde error dirigido al cliente:
                struct mensaje resp = {0};
                resp.mtype = msg.reply_to ? msg.reply_to : 2;
                snprintf(resp.texto, MAX_TEXTO, "Sala '%s' no existe", msg.sala);
                strncpy(resp.remitente, "Servidor", MAX_NOMBRE - 1);
                strncpy(resp.sala, msg.sala, MAX_NOMBRE - 1);
                msgsnd(cola_global, &resp, MSGSZ, 0);
                continue;
            }

            // Reenviar a todos (mtype por usuario)
            broadcast_a_sala(indice, &msg, msg.reply_to);
            printf("Broadcast en sala %s: %s: %s\n", msg.sala, msg.remitente, msg.texto);
        }
    }

    return 0;
}
