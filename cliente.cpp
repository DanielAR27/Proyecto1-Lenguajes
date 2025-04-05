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

// Colores ANSI consistentes
const string COLOR_RESET = "\033[0m";
const string COLOR_ROJO = "\033[1;31m";
const string COLOR_VERDE = "\033[1;32m";
const string COLOR_AMARILLO = "\033[1;33m";
const string COLOR_AZUL = "\033[1;34m";
const string COLOR_MAGENTA = "\033[1;35m";
const string COLOR_CYAN = "\033[1;36m";
const string COLOR_GRIS = "\033[1;90m";
const string COLOR_COMANDO = "\033[1;93m"; // Color específico para comandos (amarillo brillante)

// ASCII Art para mostrar
void mostrar_ascii_logo()
{
    const char *ascii[] = {
        "  ,-.       _,---._ __  / \\",
        " /  )    .-'       `./ /   \\",
        "(  (   ,'            `/    /|",
        " \\  `-\"             \\'\\   / |",
        "  `.              ,  \\ \\ /  |",
        "   /`.          ,'-`----Y   |",
        "  (            ; Sistema|   '",
        "  |  ,-.    ,-'   de    |  /",
        "  |  | (   |  Mensajería| /",
        "  )  |  \\  `.___________|/",
        "  `--'   `--'"};

    cout << COLOR_CYAN;
    for (const auto &linea : ascii)
    {
        cout << linea << endl;
    }
    cout << COLOR_RESET << endl;
}

volatile sig_atomic_t desconectado = 0;

void manejarSenal(int sig)
{
    desconectado = 1;
}

void mostrarAyuda()
{
    // Enviar lista de comandos con estilo mejorado
    string mensajeBienvenida =
        COLOR_AZUL + "Comandos disponibles:\n" + COLOR_RESET +
        COLOR_CYAN + "/broadcast [mensaje]" + COLOR_RESET + "     - Enviar mensaje a todos los usuarios\n" +
        COLOR_CYAN + "/listar" + COLOR_RESET + "                  - Ver usuarios conectados\n" +
        COLOR_CYAN + "/msg [usuario] [mensaje]" + COLOR_RESET + " - Enviar mensaje privado\n" +
        COLOR_CYAN + "/ayuda" + COLOR_RESET + "                   - Mostrar esta ayuda\n" +
        COLOR_CYAN + "/salir" + COLOR_RESET + "                   - Desconectarse\n" + COLOR_RESET;

    cout << mensajeBienvenida << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cerr << COLOR_ROJO << "Uso: " << argv[0] << " <direccion_servidor> <puerto>" << COLOR_RESET << endl;
        return 1;
    }

    signal(SIGINT, manejarSenal);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        cerr << COLOR_ROJO << "[ERROR] Error al crear el socket: " << strerror(errno) << COLOR_RESET << endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serverAddr.sin_addr) <= 0)
    {
        cerr << COLOR_ROJO << "[ERROR] Dirección del servidor inválida" << COLOR_RESET << endl;
        close(sock);
        return 1;
    }

    mostrar_ascii_logo();
    cout << COLOR_GRIS << "[INFO] Conectando al servidor " << argv[1] << ":" << argv[2] << "..." << COLOR_RESET << endl;

    if (connect(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cerr << COLOR_ROJO << "[ERROR] No se pudo conectar al servidor: " << strerror(errno) << COLOR_RESET << endl;
        close(sock);
        return 1;
    }

    // Recibir mensaje de bienvenida
    char buffer[1024] = {0};
    recv(sock, buffer, sizeof(buffer), 0);
    cout << endl
         << buffer << COLOR_RESET;

    // Ingresar nombre
    string nombre;
    getline(cin, nombre);

    // Validar nombre
    while (nombre.empty() || nombre.find_first_of(" \t\n\r") != string::npos)
    {
        cout << COLOR_ROJO << "[ERROR] Nombre inválido. No puede contener espacios. Intente nuevamente: " << COLOR_RESET;
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
                    cout << COLOR_ROJO << "\n[ERROR] Desconectado del servidor" << COLOR_RESET << endl;
                }
                break;
            }

            // Solo imprimir si hay algo que mostrar
            if (strlen(buffer) > 0)
            {
                cout << buffer << flush;
            }
        }
        exit(0);
    }
    else
    { // Proceso padre: envía mensajes
        while (!desconectado)
        {
            string mensaje;
            cout << COLOR_AZUL << "> " << COLOR_RESET;
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
            else if (mensaje == "/ascii")
            {
                mostrar_ascii_logo();
                continue;
            }

            // Sanitizar mensaje
            mensaje.erase(remove(mensaje.begin(), mensaje.end(), '\r'), mensaje.end());
            mensaje.erase(remove(mensaje.begin(), mensaje.end(), '\n'), mensaje.end());

            send(sock, mensaje.c_str(), mensaje.length(), 0);
        }

        if (pid > 0)
        {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
        }
    }

    close(sock);
    cout << COLOR_GRIS << "[INFO] Sesión finalizada" << COLOR_RESET << endl;
    return 0;
}