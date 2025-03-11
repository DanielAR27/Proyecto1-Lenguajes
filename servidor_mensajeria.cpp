#include "servidor.h"
#include <csignal>
#include <sys/wait.h>

using namespace std;

// 🔹 Manejar señales para limpiar procesos hijos
void manejarSenal(int signo) {
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

int main() {
    signal(SIGCHLD, manejarSenal);

    try {
        Servidor servidor("config.txt");

        cout << "Esperando conexiones de clientes y mensajes..." << endl;
        
        servidor.manejarConexiones();

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}
