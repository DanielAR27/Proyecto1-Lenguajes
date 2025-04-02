#include "servidor.h"
#include <csignal>
#include <sys/wait.h>

void manejarSenal(int signo)
{
    while (waitpid(-1, nullptr, WNOHANG) > 0)
        ;
}

Servidor::Servidor(const std::string &configFile)
{
    std::ifstream archivo(configFile);
    if (!archivo)
    {
        throw std::runtime_error(ERROR "No se pudo abrir el archivo de configuración.");
    }
    archivo >> puerto;
    iniciarServidor();
}

void Servidor::iniciarServidor()
{
    socketServidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketServidor == -1)
    {
        throw std::runtime_error(ERROR "Error al crear el socket del servidor.");
    }

    sockaddr_in direccionServidor = {}; // Sintaxis corregida
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_addr.s_addr = INADDR_ANY;
    direccionServidor.sin_port = htons(puerto);

    if (bind(socketServidor, (struct sockaddr *)&direccionServidor, sizeof(direccionServidor)) < 0)
    {
        throw std::runtime_error(ERROR "Error al enlazar el socket.");
    }

    if (listen(socketServidor, 10) < 0)
    {
        throw std::runtime_error(ERROR "Error al escuchar conexiones.");
    }

    FD_ZERO(&conjuntoSockets);
    FD_SET(socketServidor, &conjuntoSockets);
    socketMaximo = socketServidor;

    std::cout << VERDE << OK << "Servidor iniciado en el puerto " << puerto << RESET << std::endl;
}

void Servidor::registrarUsuario(const std::string &nombre, int socketUsuario)
{
    usuarios.emplace_back(nombre, "127.0.0.1", socketUsuario);
    std::cout << VERDE << OK << "Usuario registrado: " << nombre << RESET << std::endl;
}

void Servidor::eliminarUsuario(int socketUsuario)
{
    // Versión sin auto y sin lambda (compatible con C++98)
    std::vector<Usuario>::iterator it = usuarios.begin();
    while (it != usuarios.end())
    {
        if (it->socketUsuario == socketUsuario)
        {
            std::cout << AMARILLO << "[DESCONEXION] Usuario desconectado: " << it->nombre << RESET << std::endl;
            it = usuarios.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Servidor::manejarConexiones()
{
    while (true)
    {
        fd_set lecturaSockets;
        FD_ZERO(&lecturaSockets);
        FD_SET(socketServidor, &lecturaSockets);
        socketMaximo = socketServidor;

        // Agregar todos los sockets de clientes
        for (const auto &usuario : usuarios)
        {
            FD_SET(usuario.socketUsuario, &lecturaSockets);
            if (usuario.socketUsuario > socketMaximo)
            {
                socketMaximo = usuario.socketUsuario;
            }
        }

        int actividad = select(socketMaximo + 1, &lecturaSockets, nullptr, nullptr, nullptr);

        if (actividad < 0)
        {
            std::cerr << ROJO << ERROR << "Error en select()" << RESET << std::endl;
            continue;
        }

        // Manejar nuevas conexiones
        if (FD_ISSET(socketServidor, &lecturaSockets))
        {
            sockaddr_in direccionCliente = {};
            socklen_t tamanoCliente = sizeof(direccionCliente);
            int nuevoSocket = accept(socketServidor, (struct sockaddr *)&direccionCliente, &tamanoCliente);

            if (nuevoSocket < 0)
            {
                std::cerr << ROJO << ERROR << "Error al aceptar conexión." << RESET << std::endl;
                continue;
            }

            char nombre[50] = {0};
            int bytesRecibidos = recv(nuevoSocket, nombre, sizeof(nombre) - 1, 0);
            if (bytesRecibidos <= 0)
            {
                close(nuevoSocket);
                continue;
            }

            nombre[bytesRecibidos] = '\0';
            registrarUsuario(nombre, nuevoSocket);
        }

        // Verificar desconexiones
        for (auto it = usuarios.begin(); it != usuarios.end();)
        {
            if (FD_ISSET(it->socketUsuario, &lecturaSockets))
            {
                char buffer[1];
                int bytesRecibidos = recv(it->socketUsuario, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);

                if (bytesRecibidos == 0)
                {
                    std::cout << AMARILLO << "[DESCONEXION] Usuario desconectado: " << it->nombre << RESET << std::endl;
                    close(it->socketUsuario);
                    it = usuarios.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            else
            {
                ++it;
            }
        }
    }
}

Servidor::~Servidor()
{
    close(socketServidor);
}

int main()
{
    signal(SIGCHLD, manejarSenal);
    try
    {
        Servidor servidor("config.txt");
        std::cout << VERDE << OK << "Servidor listo. Esperando conexiones..." << RESET << std::endl;
        servidor.manejarConexiones();
    }
    catch (const std::exception &e)
    {
        std::cerr << ROJO << ERROR << e.what() << RESET << std::endl;
        return 1;
    }
    return 0;
}