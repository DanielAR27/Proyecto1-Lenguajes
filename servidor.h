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

// Códigos de color ANSI consistentes
const string COLOR_RESET = "\033[0m";
const string COLOR_ROJO = "\033[1;31m";
const string COLOR_VERDE = "\033[1;32m";
const string COLOR_AMARILLO = "\033[1;33m";
const string COLOR_AZUL = "\033[1;34m";
const string COLOR_MAGENTA = "\033[1;35m";
const string COLOR_CYAN = "\033[1;36m";
const string COLOR_GRIS = "\033[1;90m";
const string COLOR_NEGRITA = "\033[1m";
const string COLOR_COMANDO = "\033[1;93m"; // Color específico para comandos (amarillo brillante)

// Función para mostrar el ASCII Art del servidor
void mostrar_ascii_servidor()
{
    const char *ascii[] = {
        " ░▒▓███████▓▒░▒▓████████▓▒░▒▓███████▓▒░░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░▒▓███████▓▒░ ░▒▓██████▓▒░░▒▓███████▓▒░  ",
        "░▒▓█▓▒░      ░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ",
        "░▒▓█▓▒░      ░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░░▒▓█▓▒▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ",
        " ░▒▓██████▓▒░░▒▓██████▓▒░ ░▒▓███████▓▒░ ░▒▓█▓▒▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓███████▓▒░  ",
        "       ░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▓█▓▒░ ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ",
        "       ░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░░▒▓█▓▒░ ░▒▓█▓▓█▓▒░ ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░ ",
        "░▒▓███████▓▒░░▒▓████████▓▒░▒▓█▓▒░░▒▓█▓▒░  ░▒▓██▓▒░  ░▒▓█▓▒░▒▓███████▓▒░ ░▒▓██████▓▒░░▒▓█▓▒░░▒▓█▓▒░ ",
    };

    cout << COLOR_CYAN;
    for (const auto &linea : ascii)
    {
        cout << linea << endl;
    }
    cout << COLOR_RESET << endl;
}

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

        cout << COLOR_VERDE << "[OK] Servidor iniciado en el puerto " << puerto << COLOR_RESET << endl;

        FD_ZERO(&conjuntoSockets);
        FD_SET(socketServidor, &conjuntoSockets);
        socketMaximo = socketServidor;
    }

    void registrarUsuario(string nombre, string ip, int socketUsuario)
    {
        // Validar nombre de usuario
        if (nombre.empty() || nombre.find_first_of(" \t\n\r") != string::npos)
        {
            string mensajeError = COLOR_ROJO + "[ERROR] Nombre de usuario inválido. No puede contener espacios.\n" + COLOR_RESET;
            send(socketUsuario, mensajeError.c_str(), mensajeError.size(), 0);
            close(socketUsuario);
            return;
        }

        if (memoria->usuarios.find(nombre) != memoria->usuarios.end())
        {
            string mensajeError = COLOR_ROJO + "[ERROR] Nombre de usuario ya en uso. Por favor elija otro.\n" + COLOR_RESET;
            send(socketUsuario, mensajeError.c_str(), mensajeError.size(), 0);
            close(socketUsuario);
            return;
        }

        memoria->usuarios[nombre] = Usuario(nombre, ip, socketUsuario);
        cout << COLOR_VERDE << "[OK] Usuario registrado: " << nombre << " (" << ip << ")" << " (Socket FD: " << socketUsuario << ")" << COLOR_RESET << "\n";

        // Enviar mensajes pendientes si los hay
        while (!memoria->mensajesPendientes.empty())
        {
            auto mensaje = memoria->mensajesPendientes.front();
            if (mensaje.first == nombre)
            {
                string mensajeFinal = COLOR_VERDE + "[PENDIENTE] " + mensaje.second + COLOR_RESET + "\n";
                send(socketUsuario, mensajeFinal.c_str(), mensajeFinal.size(), 0);
                memoria->mensajesPendientes.pop();
            }
            else
            {
                break;
            }
        }

        // Enviar lista de comandos con estilo mejorado
        string mensajeBienvenida = COLOR_MAGENTA + "\n════════════════════════════════════════════════════════════════════\n" +
                                   COLOR_VERDE + "                     - Bienvenido " + nombre + "-" +
                                   COLOR_MAGENTA + "\n════════════════════════════════════════════════════════════════════\n\n" + COLOR_RESET +

                                   COLOR_AZUL + "Comandos disponibles:\n" + COLOR_RESET +
                                   COLOR_CYAN + "/broadcast [mensaje]" + COLOR_RESET + "     - Enviar mensaje a todos los usuarios\n" +
                                   COLOR_CYAN + "/listar" + COLOR_RESET + "                  - Ver usuarios conectados\n" +
                                   COLOR_CYAN + "/msg [usuario] [mensaje]" + COLOR_RESET + " - Enviar mensaje privado\n" +
                                   COLOR_CYAN + "/ayuda" + COLOR_RESET + "                   - Mostrar esta ayuda\n" +
                                   COLOR_CYAN + "/salir" + COLOR_RESET + "                   - Desconectarse\n" + COLOR_RESET;

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
                cerr << COLOR_ROJO << "[ERROR] Error en select(): " << strerror(errno) << COLOR_RESET << endl;
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
                    cerr << COLOR_ROJO << "[ERROR] Error al aceptar conexión: " << strerror(errno) << COLOR_RESET << endl;
                    continue;
                }

                char ipCliente[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &direccionCliente.sin_addr, ipCliente, INET_ADDRSTRLEN);

                const char *mensajeBienvenida = "> Por favor ingrese su nombre de usuario (sin espacios): ";
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

    // Función auxiliar para formatear mensajes de manera consistente
    string formatearMensaje(const string &tipo, const string &contenido, const string &color = COLOR_RESET)
    {
        string mensaje;

        if (tipo == "ERROR")
        {
            mensaje = COLOR_ROJO + "[" + tipo + "] " + contenido + "\n" + COLOR_RESET;
        }
        else if (tipo == "ENVIADO")
        {
            mensaje = COLOR_AMARILLO + "[" + tipo + "] " + contenido + "\n" + COLOR_RESET;
        }
        else if (tipo == "INFO")
        {
            mensaje = COLOR_AZUL + "[" + tipo + "] " + contenido + "\n" + COLOR_RESET;
        }
        else if (tipo == "AYUDA")
        {
            mensaje = COLOR_CYAN + "[" + tipo + "] " + contenido + COLOR_RESET;
        }
        else
        {
            mensaje = color + contenido + COLOR_RESET;
        }

        return mensaje;
    }

    void procesarMensaje(const string &remitente, const string &mensaje, int socket)
    {
        if (mensaje.empty())
            return;

        // Comandos especiales
        if (mensaje[0] == '/')
        {
            // Validación para el comando /msg
            if (mensaje.substr(0, 4) == "/msg")
            {
                size_t espacio1 = mensaje.find(' ', 4);
                if (espacio1 == string::npos)
                {
                    string error = formatearMensaje("ERROR", "Formato incorrecto. Use: /msg usuario mensaje");
                    send(socket, error.c_str(), error.size(), 0);
                    return;
                }

                size_t espacio2 = mensaje.find(' ', espacio1 + 1);
                if (espacio2 == string::npos)
                {
                    string error = formatearMensaje("ERROR", "Formato incorrecto. Use: /msg usuario mensaje");
                    send(socket, error.c_str(), error.size(), 0);
                    return;
                }

                string destinatario = mensaje.substr(espacio1 + 1, espacio2 - espacio1 - 1);
                string contenido = mensaje.substr(espacio2 + 1);

                if (contenido.empty())
                {
                    string error = formatearMensaje("ERROR", "El mensaje no puede estar vacío");
                    send(socket, error.c_str(), error.size(), 0);
                    return;
                }

                enviarMensajePrivado(remitente, destinatario, contenido);
                string confirmacion = formatearMensaje("ENVIADO", "Mensaje privado enviado a " + destinatario);
                send(socket, confirmacion.c_str(), confirmacion.size(), 0);
                return;
            }
            else if (mensaje.substr(0, 10) == "/broadcast")
            {
                if (mensaje.length() <= 11)
                {
                    string error = formatearMensaje("ERROR", "Mensaje vacío. Use: /broadcast mensaje");
                    send(socket, error.c_str(), error.size(), 0);
                    return;
                }

                string contenido = mensaje.substr(11); // "/broadcast ".length() = 11

                // Mensaje broadcast (a todos los usuarios)
                string mensajeBroadcast = formatearMensaje("", "[" + remitente + " a TODOS]: " + contenido + "\n", COLOR_AZUL);

                // Mostrar el mensaje en el servidor
                cout << COLOR_AZUL << "[BROADCAST] " << remitente << ": " << contenido << COLOR_RESET << endl;

                for (auto &par : memoria->usuarios)
                {
                    if (par.first != remitente)
                    {
                        send(par.second.socketUsuario, mensajeBroadcast.c_str(), mensajeBroadcast.size(), 0);
                    }
                }

                string confirmacion = formatearMensaje("ENVIADO", "Mensaje global enviado");
                send(socket, confirmacion.c_str(), confirmacion.size(), 0);
                return;
            }
            else if (mensaje == "/listar")
            {
                listarUsuarios(socket);
                // Enviar prompt inmediatamente
                string prompt = formatearMensaje("", "> ", COLOR_AZUL);
                send(socket, prompt.c_str(), prompt.size(), 0);
                return;
            }
            else if (mensaje == "/salir")
            {
                manejarDesconexion(socket);
                return;
            }
            else if (mensaje == "/ayuda")
            {
                string ayuda = formatearMensaje("AYUDA",
                                                "\nComandos disponibles:\n" +
                                                    COLOR_COMANDO + "/broadcast <mensaje>" + COLOR_RESET + " - Enviar mensaje a todos los usuarios\n" +
                                                    COLOR_COMANDO + "/listar" + COLOR_RESET + " - Ver usuarios conectados\n" +
                                                    COLOR_COMANDO + "/msg <usuario> <mensaje>" + COLOR_RESET + " - Enviar mensaje privado\n" +
                                                    COLOR_COMANDO + "/ayuda" + COLOR_RESET + " - Mostrar esta ayuda\n" +
                                                    COLOR_COMANDO + "/salir" + COLOR_RESET + " - Desconectarse\n" +
                                                    "\n");
                send(socket, ayuda.c_str(), ayuda.size(), 0);
                return;
            }
            else
            {
                // Comando no reconocido
                string error = formatearMensaje("ERROR", "Comando no reconocido. Use /ayuda para ver los comandos disponibles.");
                send(socket, error.c_str(), error.size(), 0);
                return;
            }
        }
        else
        {
            // No es un comando - indicar que debe usar /broadcast
            string error = formatearMensaje("ERROR", "Para enviar un mensaje global use: /broadcast mensaje");
            send(socket, error.c_str(), error.size(), 0);
            return;
        }
    }

    void enviarMensajePrivado(const string &remitente, const string &destinatario, const string &contenido)
    {
        auto it = memoria->usuarios.find(destinatario);
        if (it != memoria->usuarios.end())
        {
            string mensajeFinal = COLOR_VERDE + "[MP de " + remitente + "]: " + contenido + COLOR_RESET + "\n";
            send(it->second.socketUsuario, mensajeFinal.c_str(), mensajeFinal.size(), 0);
            cout << COLOR_AZUL << "[MP] Mensaje privado de " << remitente << " a " << destinatario << COLOR_RESET << "\n";
        }
        else
        {
            // Guardar mensaje pendiente si el usuario no está conectado
            memoria->mensajesPendientes.push({destinatario, "[" + remitente + "]: " + contenido});
            string notificacion = COLOR_AMARILLO + "[AVISO] Usuario " + destinatario + " no está conectado. El mensaje se entregará cuando se conecte.\n" + COLOR_RESET;
            auto remit = memoria->usuarios.find(remitente);
            if (remit != memoria->usuarios.end())
            {
                send(remit->second.socketUsuario, notificacion.c_str(), notificacion.size(), 0);
            }
            cout << COLOR_AMARILLO << "[AVISO] Mensaje pendiente para " << destinatario << " de " << remitente << COLOR_RESET << endl;
        }
    }

    void listarUsuarios(int socket)
    {
        string lista = COLOR_AMARILLO + "Usuarios conectados (" + to_string(memoria->usuarios.size()) + "):\n" + COLOR_RESET;

        for (auto &par : memoria->usuarios)
        {
            lista += "   - " + par.first + " (" + par.second.ip + ")\n";
        }

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
            cerr << COLOR_ROJO << "[DESCONEXIÓN] Usuario desconectado: " << usuarioDesconectado
                 << " (Socket FD: " << socket << ")" << COLOR_RESET << "\n";
        }
        else
        {
            cerr << COLOR_ROJO << "[DESCONEXIÓN] Socket desconectado: " << socket << COLOR_RESET << "\n";
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
                cerr << COLOR_AMARILLO << "[AVISO] Limpiando usuario inactivo: " << it->first << COLOR_RESET << "\n";
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