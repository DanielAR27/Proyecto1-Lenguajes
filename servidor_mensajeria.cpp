#include "servidor.h"
#include <csignal>
#include <sys/wait.h>

using namespace std;

void manejarSenal(int signo)
{
    while (waitpid(-1, nullptr, WNOHANG) > 0)
        ;
}

int main()
{
    signal(SIGCHLD, manejarSenal);
    signal(SIGINT, [](int)
           { exit(0); }); // Manejar Ctrl+C

    try
    {
        mostrar_ascii_servidor();
        cout << COLOR_GRIS << "[INFO] Iniciando servidor de mensajería..." << COLOR_RESET << endl;
        Servidor servidor("config.txt");

        cout << COLOR_GRIS << "[INFO] Esperando conexiones de clientes..." << COLOR_RESET << endl;

        servidor.manejarConexiones();
    }
    catch (const exception &e)
    {
        cerr << COLOR_ROJO << "[ERROR CRÍTICO] " << e.what() << COLOR_RESET << endl;
        return 1;
    }

    return 0;
}