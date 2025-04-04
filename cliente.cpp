#include <iostream>
#include <string>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <vector>
#include <algorithm>

using namespace std;

// 🎨 Colores ANSI
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define YELLOW "\033[33m"

volatile sig_atomic_t desconectado = 0;

void manejarSenal(int sig)
{
    desconectado = 1;
}

void mostrarAyuda()
{
    cout << CYAN << "\nComandos disponibles:\n"
         << "/listar - Ver usuarios conectados\n"
         << "/salir - Desconectarse\n"
         << "/msg <usuario> <mensaje> - Enviar mensaje privado\n"
         << "/ayuda - Mostrar esta ayuda\n"
         << RESET << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << RED << "Uso: " << argv[0] << " <direccion_servidor> <puerto>" << RESET << endl;
        return 1;
    }

    signal(SIGINT, manejarSenal);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        cerr << RED << "❌ Error al crear el socket: " << strerror(errno) << RESET << endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0)
    {
        cerr << RED << "❌ Dirección del servidor inválida" << RESET << endl;
        close(sock);
        return 1;
    }

    cout << "🔹 Conectando al servidor " << argv[1] << ":" << argv[2] << "..." << endl;

    if (connect(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr << RED << "❌ No se pudo conectar al servidor: " << strerror(errno) << RESET << endl;
        close(sock);
        return 1;
    }

    // Recibir mensaje de bienvenida
    char buffer[1024] = {0};
    recv(sock, buffer, sizeof(buffer), 0);
    cout << GREEN << buffer << RESET;

    // Ingresar nombre
    string nombre;
    cout << "Ingrese su nombre de usuario: ";
    getline(cin, nombre);

    // Validar nombre
    while (nombre.empty() || nombre.find_first_of(" \t\n\r") != string::npos)
    {
        cout << RED << "Nombre inválido. No puede contener espacios. Intente nuevamente: " << RESET;
        getline(cin, nombre);
    }

    send(sock, nombre.c_str(), nombre.length(), 0);

    // Recibir confirmación de registro
    memset(buffer, 0, sizeof(buffer));
    recv(sock, buffer, sizeof(buffer), 0);
    cout << buffer;

    pid_t pid = fork();

    if (pid == 0)
    { // Proceso hijo: recibe mensajes
        while (!desconectado)
        {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(sock, buffer, sizeof(buffer), 0);

            if (bytes <= 0)
            {
                if (!desconectado)
                {
                    cout << RED << "\n❌ Desconectado del servidor" << RESET << endl;
                }
                break;
            }

            cout << buffer << flush;
        }
        exit(0);
    }
    else
    { // Proceso padre: envía mensajes
        mostrarAyuda();

        while (!desconectado)
        {
            string mensaje;
            cout << BLUE << "> " << RESET;
            getline(cin, mensaje);

            if (desconectado)
                break;

            if (mensaje.empty())
                continue;

            if (mensaje == "/salir")
            {
                desconectado = 1;
                break;
            }
            else if (mensaje == "/ayuda")
            {
                mostrarAyuda();
                continue;
            }

            // Sanitizar mensaje
            mensaje.erase(remove(mensaje.begin(), mensaje.end(), '\r'), mensaje.end());
            mensaje.erase(remove(mensaje.begin(), mensaje.end(), '\n'), mensaje.end());

            send(sock, mensaje.c_str(), mensaje.length(), 0);

            if (mensaje.substr(0, 4) == "/msg")
            {
                cout << YELLOW << "📤 Mensaje privado enviado" << RESET << endl;
            }
        }

        if (pid > 0)
        {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
        }
    }

    close(sock);
    cout << "👋 Sesión finalizada" << endl;
    return 0;
}