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
        cout << "🔹 Iniciando servidor de mensajería..." << endl;
        Servidor servidor("config.txt");

        cout << "🔄 Esperando conexiones de clientes..." << endl;
        cout << "📌 Presiona Ctrl+C para detener el servidor\n"
             << endl;

        servidor.manejarConexiones();
    }
    catch (const exception &e)
    {
        cerr << RED << "❌ Error crítico: " << e.what() << RESET << endl;
        return 1;
    }

    return 0;
}