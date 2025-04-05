# Documentación del Proyecto: Sistema de Mensajería en C++

## Portada

**Proyecto 1: Sistema de Mensajería**  
**Curso: Paradigma Imperativo**  
**Integrantes:**

- Daniel Alemán
- Luis Meza
- Daniel Zeas

---

## Índice

1. [Enlace de GitHub](#enlace-de-github)
2. [Descripción del Proyecto](#descripción-del-proyecto)
3. [Requisitos](#requisitos)
4. [Instalación](#instalación)
5. [Manual de Usuario](#manual-de-usuario)
6. [Arquitectura Lógica](#arquitectura-lógica)
7. [Funcionamiento](#funcionamiento)
8. [Evaluación](#evaluación)

---

## Enlace de GitHub

[Repositorio del Proyecto](https://github.com/[usuario]/[repositorio])

---

## Descripción del Proyecto

Este proyecto consiste en un sistema de mensajería cliente-servidor desarrollado en C++ para Linux. El servidor central gestiona los usuarios y los mensajes, mientras que los clientes pueden registrarse, enviar mensajes privados o globales, y recibir mensajes en tiempo real.

### Características principales:

- Registro automático de usuarios con IP y nombre.
- Envío de mensajes privados (`/msg`) y globales (`/broadcast`).
- Recepción de mensajes en tiempo real mediante procesos bifurcados (`fork()`).
- Persistencia de mensajes pendientes para usuarios desconectados.

---

## Requisitos

- **Sistema Operativo:** Linux
- **Compilador:** `g++` (C++11 o superior)
- **Librerías:**
  - `sys/socket.h`
  - `arpa/inet.h`
  - `unistd.h`
  - `csignal`
  - `sys/mman.h` (para memoria compartida)

---

## Instalación

1. **Clonar el repositorio:**

   ```bash
   git clone https://github.com/[usuario]/[repositorio].git
   cd [repositorio]
   ```

2. **Compilar el proyecto:**

   ```bash
   g++ servidor_mensajeria.cpp -o servidor -lpthread
   g++ cliente.cpp -o cliente
   ```

3. **Configurar el servidor:**  
   Editar el archivo `config.txt` para cambiar el puerto (por defecto: `5050`).

4. **Ejecutar:**
   - **Servidor:**
     ```bash
     ./servidor
     ```
   - **Cliente:**
     ```bash
     ./cliente <direccion_servidor> <puerto>
     ```
     Ejemplo:
     ```bash
     ./cliente 127.0.0.1 5050
     ```

---

## Manual de Usuario

### Comandos Disponibles (Cliente):

| Comando                  | Descripción                               |
| ------------------------ | ----------------------------------------- |
| `/broadcast <mensaje>`   | Envía un mensaje a todos los usuarios.    |
| `/msg <usuario> <texto>` | Envía un mensaje privado a un usuario.    |
| `/listar`                | Muestra la lista de usuarios conectados.  |
| `/ayuda`                 | Muestra la lista de comandos disponibles. |
| `/salir`                 | Cierra la sesión del usuario.             |

### Ejemplo de Uso:

1. **Registro:**  
   Al iniciar el cliente, se solicita un nombre de usuario (sin espacios).
2. **Enviar mensaje global:**
   ```bash
   /broadcast Hola a todos!
   ```
3. **Enviar mensaje privado:**
   ```bash
   /msg usuario2 ¿Cómo estás?
   ```

---

## Arquitectura Lógica

### Diagrama de Componentes:

```
+-------------------+       +-------------------+       +-------------------+
|     Cliente       | <---> |     Servidor      | <---> |     Cliente       |
|-------------------|       |-------------------|       |-------------------|
| - Socket de envío |       | - Memoria         |       | - Socket de envío |
| - Socket de recep.|       |   compartida      |       | - Socket de recep.|
| - Fork()          |       | - Gestión de      |       | - Fork()          |
+-------------------+       |   conexiones      |       +-------------------+
                            +-------------------+
```

### Estructuras Clave:

- **Servidor:**
  - Usa `select()` para manejar múltiples conexiones.
  - Memoria compartida (`shm_open`) para usuarios y mensajes pendientes.
- **Cliente:**
  - Proceso padre para enviar mensajes.
  - Proceso hijo (`fork()`) para recibir mensajes.

---

## Funcionamiento

1. **Registro de Usuarios:**
   - El cliente envía su nombre al servidor, que lo valida y registra.
2. **Envío de Mensajes:**
   - Los mensajes pasan por el servidor, que los redirige al destinatario o los almacena si está desconectado.
3. **Recepción:**
   - Cada cliente usa un proceso hijo para escuchar mensajes entrantes.
4. **Limpieza:**
   - El servidor elimina usuarios inactivos después de 5 minutos.

---

## Evaluación

| Aspecto               | Porcentaje | Cumplimiento |
| --------------------- | ---------- | ------------ |
| Documentación         | 5%         | ✅           |
| Registro de Usuarios  | 15%        | ✅           |
| Envío de Mensajes     | 25%        | ✅           |
| Recepción de Mensajes | 25%        | ✅           |
| Uso de `fork()`       | 10%        | ✅           |
| Desempeño en Revisión | 10%        | ✅           |
| Autoevaluación        | 10%        | ✅           |
