#include "servidor.h"
#include <csignal>
#include <sys/wait.h>

using namespace std;

// Manejar señales para limpiar procesos hijos
void manejarSenal(int signo) {
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

int main() {
    signal(SIGCHLD, manejarSenal);

    try {
        Servidor servidor("config.txt");

        cout << "Esperando conexiones de clientes y mensajes..." << endl;

        // Crear dos procesos: uno para conexiones, otro para mensajes
        int pid = fork();
        if (pid < 0) {
            cerr << "Error al hacer fork del servidor de mensajería." << endl;
            return 1;
        }

        if (pid == 0) {
            // Proceso hijo maneja los mensajes
            servidor.manejarMensajes();
            exit(0);
        }

        // Proceso padre maneja conexiones de clientes
        servidor.aceptarConexiones();

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}
