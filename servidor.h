#pragma once

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
#include <stdexcept>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/select.h>
#include <map>
#include <queue>

using namespace std;

// Códigos de color ANSI
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

class Usuario
{
public:
    char nombre[50];
    char ip[INET_ADDRSTRLEN];
    int socketUsuario;
    time_t ultimaConexion;

    Usuario() {}
    Usuario(string n, string i, int s)
    {
        strncpy(nombre, n.c_str(), sizeof(nombre) - 1);
        nombre[sizeof(nombre) - 1] = '\0';
        strncpy(ip, i.c_str(), sizeof(ip) - 1);
        ip[sizeof(ip) - 1] = '\0';
        socketUsuario = s;
        ultimaConexion = time(nullptr);
    }

    void actualizarActividad()
    {
        ultimaConexion = time(nullptr);
    }
};

struct MemoriaCompartida
{
    map<string, Usuario> usuarios;                  // Usamos map para búsqueda más eficiente
    queue<pair<string, string>> mensajesPendientes; // Cola de mensajes para usuarios desconectados
};

class Servidor
{
private:
    MemoriaCompartida *memoria;
    int puerto;
    int socketServidor;
    fd_set conjuntoSockets;
    int socketMaximo;
    bool ejecutando;

public:
    Servidor(string configFile) : ejecutando(true)
    {
        leerConfiguracion(configFile);
        iniciarMemoriaCompartida();
        iniciarServidor();
    }

    void iniciarMemoriaCompartida()
    {
        int fd = shm_open("/memoriaUsuarios", O_CREAT | O_RDWR, 0666);
        ftruncate(fd, sizeof(MemoriaCompartida));
        memoria = (MemoriaCompartida *)mmap(nullptr, sizeof(MemoriaCompartida),
                                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (memoria == MAP_FAILED)
        {
            throw runtime_error("Error al asignar memoria compartida.");
        }

        new (memoria) MemoriaCompartida(); // Inicialización del objeto en memoria compartida
    }

    void leerConfiguracion(string configFile)
    {
        ifstream archivo(configFile);
        if (!archivo)
        {
            throw runtime_error("No se pudo abrir el archivo de configuración.");
        }
        archivo >> puerto;
        if (puerto <= 1024 || puerto > 65535)
        {
            throw runtime_error("Puerto inválido. Debe estar entre 1025 y 65535");
        }
    }

    void iniciarServidor()
    {
        socketServidor = socket(AF_INET, SOCK_STREAM, 0);
        if (socketServidor == -1)
        {
            throw runtime_error("Error al crear el socket del servidor.");
        }

        int opcion = 1;
        setsockopt(socketServidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));

        sockaddr_in direccionServidor{};
        direccionServidor.sin_family = AF_INET;
        direccionServidor.sin_addr.s_addr = INADDR_ANY;
        direccionServidor.sin_port = htons(puerto);

        if (::bind(socketServidor, (struct sockaddr *)&direccionServidor, sizeof(direccionServidor)) < 0)
        {
            throw runtime_error("Error al enlazar el socket.");
        }

        if (listen(socketServidor, 10) < 0)
        {
            throw runtime_error("Error al escuchar conexiones.");
        }

        cout << "✅ Servidor iniciado en el puerto " << puerto << endl;

        FD_ZERO(&conjuntoSockets);
        FD_SET(socketServidor, &conjuntoSockets);
        socketMaximo = socketServidor;
    }

    void registrarUsuario(string nombre, string ip, int socketUsuario)
    {
        // Validar nombre de usuario
        if (nombre.empty() || nombre.find_first_of(" \t\n\r") != string::npos)
        {
            string mensajeError = "Nombre de usuario inválido. No puede contener espacios.\n";
            send(socketUsuario, mensajeError.c_str(), mensajeError.size(), 0);
            close(socketUsuario);
            return;
        }

        if (memoria->usuarios.find(nombre) != memoria->usuarios.end())
        {
            string mensajeError = "Nombre de usuario ya en uso. Por favor elija otro.\n";
            send(socketUsuario, mensajeError.c_str(), mensajeError.size(), 0);
            close(socketUsuario);
            return;
        }

        memoria->usuarios[nombre] = Usuario(nombre, ip, socketUsuario);
        cout << "✅ Usuario registrado: " << nombre << " (" << ip << ")" << " (Socket FD: " << socketUsuario << ")\n";

        // Enviar mensajes pendientes si los hay
        while (!memoria->mensajesPendientes.empty())
        {
            auto mensaje = memoria->mensajesPendientes.front();
            if (mensaje.first == nombre)
            {
                string mensajeFinal = string(GREEN) + "📨 [Pendiente] " + mensaje.second + RESET + "\n";
                send(socketUsuario, mensajeFinal.c_str(), mensajeFinal.size(), 0);
                memoria->mensajesPendientes.pop();
            }
            else
            {
                break;
            }
        }

        // Enviar lista de comandos
        string mensajeBienvenida = string(CYAN) + "\nBienvenido " + nombre + "!\n" +
                                   "Comandos disponibles:\n" +
                                   "/listar - Ver usuarios conectados\n" +
                                   "/salir - Desconectarse\n" +
                                   "/msg <usuario> <mensaje> - Enviar mensaje privado\n" + RESET;
        send(socketUsuario, mensajeBienvenida.c_str(), mensajeBienvenida.size(), 0);
    }

    void manejarConexiones()
    {
        fd_set lecturaSockets;
        timeval timeout;
        timeout.tv_sec = 30; // Timeout de 30 segundos para verificar inactividad

        while (ejecutando)
        {
            lecturaSockets = conjuntoSockets;
            int actividad = select(socketMaximo + 1, &lecturaSockets, nullptr, nullptr, &timeout);

            if (actividad < 0 && errno != EINTR)
            {
                cerr << "Error en select(): " << strerror(errno) << endl;
                continue;
            }

            // Verificar timeout para limpieza de usuarios inactivos
            if (actividad == 0)
            {
                limpiarInactivos();
                continue;
            }

            // Nueva conexión entrante
            if (FD_ISSET(socketServidor, &lecturaSockets))
            {
                sockaddr_in direccionCliente{};
                socklen_t tamanoCliente = sizeof(direccionCliente);
                int nuevoSocket = accept(socketServidor, (struct sockaddr *)&direccionCliente, &tamanoCliente);

                if (nuevoSocket < 0)
                {
                    cerr << "Error al aceptar conexión: " << strerror(errno) << endl;
                    continue;
                }

                char ipCliente[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &direccionCliente.sin_addr, ipCliente, INET_ADDRSTRLEN);

                const char *mensajeBienvenida = "Por favor ingrese su nombre de usuario (sin espacios):\n";
                send(nuevoSocket, mensajeBienvenida, strlen(mensajeBienvenida), 0);

                FD_SET(nuevoSocket, &conjuntoSockets);
                if (nuevoSocket > socketMaximo)
                {
                    socketMaximo = nuevoSocket;
                }
            }

            // Procesar mensajes de clientes existentes
            for (int i = 0; i <= socketMaximo; i++)
            {
                if (FD_ISSET(i, &lecturaSockets))
                {
                    if (i == socketServidor)
                        continue;

                    char buffer[1024] = {0};
                    int bytesRecibidos = recv(i, buffer, sizeof(buffer) - 1, 0);

                    if (bytesRecibidos <= 0)
                    {
                        manejarDesconexion(i);
                        continue;
                    }

                    buffer[bytesRecibidos] = '\0';
                    string mensaje(buffer);
                    mensaje.erase(mensaje.find_last_not_of(" \n\r\t") + 1);

                    // Buscar el usuario que envió el mensaje
                    string remitente;
                    for (auto &par : memoria->usuarios)
                    {
                        if (par.second.socketUsuario == i)
                        {
                            remitente = par.first;
                            par.second.actualizarActividad();
                            break;
                        }
                    }

                    if (remitente.empty())
                    {
                        // Usuario no registrado aún - procesar nombre de usuario
                        string nombreUsuario = mensaje;
                        char ipCliente[INET_ADDRSTRLEN];
                        sockaddr_in direccionCliente{};
                        socklen_t len = sizeof(direccionCliente);
                        getpeername(i, (struct sockaddr *)&direccionCliente, &len);
                        inet_ntop(AF_INET, &direccionCliente.sin_addr, ipCliente, INET_ADDRSTRLEN);

                        registrarUsuario(nombreUsuario, ipCliente, i);
                    }
                    else
                    {
                        // Usuario registrado - procesar mensaje/comando
                        procesarMensaje(remitente, mensaje, i);
                    }
                }
            }
        }
    }

    void procesarMensaje(const string &remitente, const string &mensaje, int socket)
    {
        if (mensaje.empty())
            return;

        // Comandos especiales
        if (mensaje[0] == '/')
        {
            if (mensaje == "/listar")
            {
                listarUsuarios(socket);
                return;
            }
            else if (mensaje == "/salir")
            {
                manejarDesconexion(socket);
                return;
            }
            else if (mensaje.substr(0, 4) == "/msg")
            {
                size_t espacio1 = mensaje.find(' ', 4);
                if (espacio1 == string::npos)
                {
                    string error = "Formato incorrecto. Use: /msg usuario mensaje\n";
                    send(socket, error.c_str(), error.size(), 0);
                    return;
                }
                size_t espacio2 = mensaje.find(' ', espacio1 + 1);
                if (espacio2 == string::npos)
                {
                    espacio2 = mensaje.length();
                }

                string destinatario = mensaje.substr(espacio1 + 1, espacio2 - espacio1 - 1);
                string contenido = mensaje.substr(espacio2 + 1);

                enviarMensajePrivado(remitente, destinatario, contenido);
                return;
            }
        }

        // Mensaje broadcast (a todos los usuarios)
        string mensajeBroadcast = string(BLUE) + "[" + remitente + " a TODOS]: " + mensaje + RESET + "\n";
        for (auto &par : memoria->usuarios)
        {
            if (par.first != remitente)
            {
                send(par.second.socketUsuario, mensajeBroadcast.c_str(), mensajeBroadcast.size(), 0);
            }
        }
    }

    void enviarMensajePrivado(const string &remitente, const string &destinatario, const string &contenido)
    {
        auto it = memoria->usuarios.find(destinatario);
        if (it != memoria->usuarios.end())
        {
            string mensajeFinal = string(GREEN) + "📨 [" + remitente + "]: " + contenido + RESET + "\n";
            send(it->second.socketUsuario, mensajeFinal.c_str(), mensajeFinal.size(), 0);
            cout << BLUE << "📨 Mensaje privado de " << remitente << " a " << destinatario << RESET << "\n";
        }
        else
        {
            // Guardar mensaje pendiente si el usuario no está conectado
            memoria->mensajesPendientes.push({destinatario, "[" + remitente + "]: " + contenido});
            string notificacion = "Usuario " + destinatario + " no está conectado. El mensaje se entregará cuando se conecte.\n";
            auto remit = memoria->usuarios.find(remitente);
            if (remit != memoria->usuarios.end())
            {
                send(remit->second.socketUsuario, notificacion.c_str(), notificacion.size(), 0);
            }
            cout << YELLOW << "⚠ Mensaje pendiente para " << destinatario << " de " << remitente << RESET << endl;
        }
    }

    void listarUsuarios(int socket)
    {
        string lista = "\nUsuarios conectados (" + to_string(memoria->usuarios.size()) + "):\n";
        for (auto &par : memoria->usuarios)
        {
            lista += "• " + par.first + " (" + par.second.ip + ")\n";
        }
        lista += "\n";
        send(socket, lista.c_str(), lista.size(), 0);
    }

    void manejarDesconexion(int socket)
    {
        string usuarioDesconectado;
        for (auto it = memoria->usuarios.begin(); it != memoria->usuarios.end();)
        {
            if (it->second.socketUsuario == socket)
            {
                usuarioDesconectado = it->first;
                it = memoria->usuarios.erase(it);
                break;
            }
            else
            {
                ++it;
            }
        }

        if (!usuarioDesconectado.empty())
        {
            cerr << RED << "❌ Usuario desconectado: " << usuarioDesconectado
                 << " (Socket FD: " << socket << ")" << RESET << "\n";
        }
        else
        {
            cerr << RED << "❌ Socket desconectado: " << socket << RESET << "\n";
        }

        close(socket);
        FD_CLR(socket, &conjuntoSockets);
    }

    void limpiarInactivos()
    {
        time_t ahora = time(nullptr);
        for (auto it = memoria->usuarios.begin(); it != memoria->usuarios.end();)
        {
            if (difftime(ahora, it->second.ultimaConexion) > 300)
            { // 5 minutos de inactividad
                cerr << YELLOW << "⚠ Limpiando usuario inactivo: " << it->first << RESET << "\n";
                close(it->second.socketUsuario);
                FD_CLR(it->second.socketUsuario, &conjuntoSockets);
                it = memoria->usuarios.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    ~Servidor()
    {
        ejecutando = false;
        close(socketServidor);
        shm_unlink("/memoriaUsuarios");
    }
};