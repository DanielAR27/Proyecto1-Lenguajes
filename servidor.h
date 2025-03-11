#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <sys/wait.h>
#include <netinet/in.h>  // Para sockets
#include <unistd.h>      // Para close()
#include <arpa/inet.h>   // Para inet_ntop()

using namespace std;

class Usuario {
public:
    string nombre;
    string ip;
    int puerto;

    // Constructor e inicializador
    Usuario(string nombre, string ip, int puerto)
        : nombre(nombre), ip(ip), puerto(puerto) {}
};

class Servidor {
private:
    vector<Usuario> usuarios;
    int puerto;
    int servidorSocket;
    int puertoClientes; // Nuevo: puerto predeterminado para los clientes
public:
    Servidor(string configFile) {
        leerConfiguracion(configFile);
        iniciarServidor();
    }

    void leerConfiguracion(string configFile) {
        ifstream archivo(configFile);
        if (!archivo) {
            throw runtime_error("No se pudo abrir el archivo de configuración.");
        }
        archivo >> puerto;         // Puerto del servidor
        archivo >> puertoClientes; // Puerto que usarán los clientes
    }

    // Iniciar el servidor y abrir el socket
    void iniciarServidor() {
        servidorSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (servidorSocket == -1) {
            throw runtime_error("Error al crear el socket del servidor.");
        }

        // 🔹 Permitir reutilizar el puerto inmediatamente después de cerrar el servidor
        int opcion = 1;
        setsockopt(servidorSocket, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));
        setsockopt(servidorSocket, SOL_SOCKET, SO_REUSEPORT, &opcion, sizeof(opcion));

        sockaddr_in direccionServidor{};
        direccionServidor.sin_family = AF_INET;
        direccionServidor.sin_addr.s_addr = INADDR_ANY;
        direccionServidor.sin_port = htons(puerto);

        // 🔹 Intentar hacer bind hasta que funcione
        while (bind(servidorSocket, (struct sockaddr*)&direccionServidor, sizeof(direccionServidor)) < 0) {
            cerr << "Error al enlazar el socket. Intentando de nuevo en 2 segundos..." << endl;
            sleep(2);  // Esperar 2 segundos antes de reintentar
        }

        if (listen(servidorSocket, 5) < 0) {
            throw runtime_error("Error al escuchar conexiones.");
        }

        cout << "Servidor iniciado en el puerto " << puerto << endl;
    }


    // Registrar un nuevo usuario
    void registrarUsuario(string nombre, string ip, int puerto) {
        usuarios.emplace_back(nombre, ip, puerto);
        cout << "Usuario registrado: " << nombre << " (" << ip << ":" << puerto << ")\n";
    }

    // Listar todos los usuarios registrados
    void listarUsuarios() {
        cout << "Usuarios registrados:\n";
        for (auto usuario : usuarios) {
            cout << usuario.nombre << " - " << usuario.ip << ":" << usuario.puerto << endl;
        }
    }

    // Función encargada de procesar la lógica de la conexión de sockets y de clientes
    void aceptarConexiones() {
        while (true) {
            sockaddr_in direccionCliente{};
            socklen_t tamanoCliente = sizeof(direccionCliente);
            int clienteSocket = accept(servidorSocket, (struct sockaddr*)&direccionCliente, &tamanoCliente);
    
            if (clienteSocket < 0) {
                cerr << "Error al aceptar conexión de cliente." << endl;
                continue;
            }
    
            int pid = fork();
    
            if (pid < 0) {
                cerr << "Error al hacer fork." << endl;
                close(clienteSocket);
                continue;
            }
    
            if (pid == 0) { // Proceso hijo
                char ipCliente[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &direccionCliente.sin_addr, ipCliente, INET_ADDRSTRLEN);
    
                char buffer[1024] = {0};
                recv(clienteSocket, buffer, 1024, 0);
                string nombreUsuario(buffer);
    
                // Usamos el puerto predeterminado en lugar del que nos da el socket
                registrarUsuario(nombreUsuario, ipCliente, puertoClientes);
    
                close(clienteSocket);
                exit(0);
            }
        }
    }
           
    void manejarMensajes() {
        int socketMensajes = socket(AF_INET, SOCK_STREAM, 0);
        if (socketMensajes == -1) {
            throw runtime_error("Error al crear el socket de mensajería.");
        }
    
        sockaddr_in direccionMensajes{};
        direccionMensajes.sin_family = AF_INET;
        direccionMensajes.sin_addr.s_addr = INADDR_ANY;
        direccionMensajes.sin_port = htons(puertoClientes);  // Usa el puerto 6000
    
        if (bind(socketMensajes, (struct sockaddr*)&direccionMensajes, sizeof(direccionMensajes)) < 0) {
            throw runtime_error("Error al enlazar el socket de mensajería.");
        }
    
        if (listen(socketMensajes, 5) < 0) {
            throw runtime_error("Error al escuchar en el socket de mensajería.");
        }
    
        cout << "Servidor de mensajería iniciado en el puerto " << puertoClientes << endl;
    
        while (true) {
            sockaddr_in direccionCliente{};
            socklen_t tamanoCliente = sizeof(direccionCliente);
            int clienteSocket = accept(socketMensajes, (struct sockaddr*)&direccionCliente, &tamanoCliente);
    
            if (clienteSocket < 0) {
                cerr << "Error al aceptar conexión en el socket de mensajería." << endl;
                continue;
            }
    
            int pid = fork();
            if (pid < 0) {
                cerr << "Error al hacer fork en el socket de mensajería." << endl;
                close(clienteSocket);
                continue;
            }
    
            if (pid == 0) {  // Proceso hijo maneja el mensaje
                char buffer[1024] = {0};
                recv(clienteSocket, buffer, 1024, 0);
                string mensaje(buffer);
    
                // Extraer destinatario y contenido del mensaje
                size_t pos = mensaje.find(": ");
                if (pos == string::npos) {
                    cerr << "Formato de mensaje incorrecto." << endl;
                    close(clienteSocket);
                    exit(0);
                }
    
                string destinatario = mensaje.substr(0, pos);
                string contenido = mensaje.substr(pos + 2);
    
                // Determinar remitente por IP
                char ipCliente[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &direccionCliente.sin_addr, ipCliente, INET_ADDRSTRLEN);
                string remitente;
                for (const auto& usuario : usuarios) {
                    if (usuario.ip == ipCliente) {
                        remitente = usuario.nombre;
                        break;
                    }
                }
    
                if (remitente.empty()) {
                    cerr << "No se pudo determinar el remitente." << endl;
                    close(clienteSocket);
                    exit(0);
                }
    
                // Buscar destinatario
                bool encontrado = false;
                for (const auto& usuario : usuarios) {
                    if (usuario.nombre == destinatario) {
                        encontrado = true;
    
                        // Enviar mensaje al destinatario
                        int socketDestino = socket(AF_INET, SOCK_STREAM, 0);
                        sockaddr_in direccionDestino{};
                        direccionDestino.sin_family = AF_INET;
                        direccionDestino.sin_port = htons(puertoClientes);
                        inet_pton(AF_INET, usuario.ip.c_str(), &direccionDestino.sin_addr);
    
                        if (connect(socketDestino, (struct sockaddr*)&direccionDestino, sizeof(direccionDestino)) < 0) {
                            cerr << "No se pudo conectar con el destinatario." << endl;
                            close(socketDestino);
                            close(clienteSocket);
                            exit(0);
                        }
    
                        // Enviar mensaje al destinatario
                        string mensajeFinal = remitente + ": " + contenido;
                        send(socketDestino, mensajeFinal.c_str(), mensajeFinal.size(), 0);
    
                        close(socketDestino);
                        break;
                    }
                }
    
                if (!encontrado) {
                    cerr << "Usuario destinatario no encontrado." << endl;
                }
    
                close(clienteSocket);
                exit(0);
            }
        }
    }
    
    // Destructor para cerrar el socket del servidor
    ~Servidor() {
        close(servidorSocket);
    }
};