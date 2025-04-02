// cliente.cpp
#include "cliente.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>

// Función wrapper estática
static void threadMantenerConexion(Cliente *cliente)
{
    cliente->mantenerConexion();
}

Cliente::Cliente() : registrado(false), socketCliente(-1) {}

Cliente::~Cliente()
{
    if (hiloConexion.joinable())
    {
        registrado = false;
        hiloConexion.join();
    }
    if (socketCliente != -1)
        close(socketCliente);
}

void Cliente::mantenerConexion()
{
    while (registrado)
    {
        // Enviar un byte de "heartbeat"
        char hb = 0;
        send(socketCliente, &hb, 1, MSG_NOSIGNAL);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Cliente::registrarUsuario()
{
    std::cout << "Ingrese su nombre de usuario: ";
    std::cin >> nombreUsuario;

    socketCliente = socket(AF_INET, SOCK_STREAM, 0);
    if (socketCliente < 0)
    {
        std::cerr << ROJO << ERROR << "Error al crear socket." << RESET << std::endl;
        return;
    }

    sockaddr_in direccionServidor = {};
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(5050);
    inet_pton(AF_INET, "127.0.0.1", &direccionServidor.sin_addr);

    if (connect(socketCliente, (struct sockaddr *)&direccionServidor, sizeof(direccionServidor)))
    {
        std::cerr << ROJO << ERROR << "No se pudo conectar al servidor." << RESET << std::endl;
        close(socketCliente);
        return;
    }

    send(socketCliente, nombreUsuario.c_str(), nombreUsuario.size(), 0);
    registrado = true;

    // Crea un puntero al objeto actual para pasarlo al thread
    Cliente *punteroThis = this;

    // Crea el thread con una función simple (sin argumentos adicionales)
    hiloConexion = std::thread([punteroThis]()
                               { threadMantenerConexion(punteroThis); });

    // O alternativamente, si sigue fallando:
    // hiloConexion = std::thread(threadMantenerConexion, punteroThis);

    std::cout << VERDE << OK << "Registro exitoso." << RESET << std::endl;
}

void Cliente::menuPrincipal()
{
    while (registrado)
    {
        std::cout << AZUL << "\n[Bienvenido, " << nombreUsuario << "]" << RESET << std::endl;
        std::cout << "1. Nuevo chat\n";
        std::cout << "2. Chats activos\n";
        std::cout << "3. Salir\n";
        std::cout << "Ingrese una opción: ";

        int opcion;
        std::cin >> opcion;

        switch (opcion)
        {
        case 1:
            std::cout << "Opción seleccionada: Nuevo chat\n";
            break;
        case 2:
            std::cout << "Opción seleccionada: Chats activos\n";
            break;
        case 3:
            registrado = false;
            return;
        default:
            std::cerr << ROJO << ERROR << "Opción inválida." << RESET << std::endl;
        }
    }
}

void Cliente::ejecutar()
{
    int opcion;
    do
    {
        mostrarMenuInicial();
        std::cin >> opcion;

        switch (opcion)
        {
        case 1:
            registrarUsuario();
            if (registrado)
                menuPrincipal();
            break;
        case 2:
            std::cout << "Saliendo...\n";
            break;
        default:
            std::cerr << ROJO << ERROR << "Opción inválida." << RESET << std::endl;
        }
    } while (opcion != 2);
}

int main()
{
    Cliente cliente;
    cliente.ejecutar();
    return 0;
}