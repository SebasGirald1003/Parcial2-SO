# Sistema de Chat con Colas de Mensajes (IPC en C)

## Integrantes

- Santiago Álvarez Peña

- Sebastián Giraldo Álvarez

- Juan Jose Vásquez Gómez

## Descripción

Este proyecto implementa un **sistema de chat multiusuario** en lenguaje **C**, utilizando **colas de mensajes System V** como mecanismo de **comunicación entre procesos (IPC)**.  

El sistema permite que múltiples clientes se conecten a un **servidor central**, se unan a diferentes **salas de chat** y envíen mensajes que son retransmitidos automáticamente a todos los miembros de la sala.

De esta forma se simula el funcionamiento básico de aplicaciones de mensajería instantánea como **WhatsApp o Discord**, pero empleando colas de mensajes del sistema operativo.

---

## Características

- Servidor central que administra:
  - Creación de salas de chat.
  - Gestión de usuarios por sala.
  - Reenvío (broadcast) de mensajes.
- Soporte para múltiples salas en paralelo.
- Múltiples clientes pueden conectarse y comunicarse en tiempo real.
- Cada cliente:
  - Se conecta a la cola global.
  - Puede **unirse a salas** con el comando `join <sala>`.
  - Envía mensajes al servidor, quien los distribuye al resto de usuarios en la misma sala.

---

## Estructura del Proyecto

├── servidor.c # Código fuente del servidor de chat

├── cliente.c # Código fuente del cliente de chat

└── README.md # Documentación del proyecto

---

## Compilación

En **Linux** (requiere gcc y librerías de desarrollo estándar):

gcc servidor.c -o servidor
gcc cliente.c -o cliente 

Ejecución

Servidor
En una terminal:
./servidor

Cliente(s)
En una o varias terminales distintas:
./cliente <nombre_usuario>

Comandos del Cliente

join <sala> → Unirse (o crear) una sala de chat.
Texto normal → Envía un mensaje a todos los usuarios de la sala actual.

Estructura de los Mensajes
struct mensaje {
    long mtype;         // Tipo de mensaje (controla destinatarios)
    int  cola_id;       // ID de la cola de la sala (devuelto por el servidor)
    long reply_to;      // Identificador único del cliente (PID)
    char remitente[MAX_NOMBRE]; // Nombre del usuario
    char texto[MAX_TEXTO];      // Contenido del mensaje
    char sala[MAX_NOMBRE];      // Sala a la que pertenece el mensaje
};

## Explicación del Funcionamiento
En el servidor, se implementan funciones clave como `crear_sala`, que genera nuevas salas de chat con su propia cola de mensajes utilizando `msgget()`, y `buscar_sala`, que permite comprobar si una sala ya existe. La función `agregar_usuario_a_sala` añade usuarios a una sala y registra su identificador `reply_to` (PID), permitiendo también la reconexión de un usuario si es necesario. Para enviar mensajes, `broadcast_a_sala` reenvía el contenido a todos los usuarios de una sala excepto al emisor, cambiando dinámicamente el `mtype` de acuerdo con el destinatario. La función `main` del servidor se encarga de crear la cola global, recibir mensajes de tipo `JOIN` o `MSG`, y gestionar tanto la creación de salas como la distribución de mensajes a los usuarios.

## Link del video.

https://youtu.be/miuw7Jao4aA


