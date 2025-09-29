# Sistema de Chat con Colas de Mensajes (IPC en C)

## Integrantes

-Santiago Álvarez Peña
-Sebastián Giraldo Álvarez
-Juan Jose Vásquez Gómez

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
