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
#include <sys/select.h>  // 🔹 Para manejar múltiples conexiones con un solo socket

using namespace std;

class Usuario {
    public:
        char nombre[50];
        char ip[INET_ADDRSTRLEN];
        int socketUsuario;  // ✅ Agregar esta línea
    
        Usuario() {}
        Usuario(string n, string i, int s) {
            strncpy(nombre, n.c_str(), sizeof(nombre) - 1);
            nombre[sizeof(nombre) - 1] = '\0';
            strncpy(ip, i.c_str(), sizeof(ip) - 1);
            ip[sizeof(ip) - 1] = '\0';
            socketUsuario = s;
        }
};
    

struct MemoriaCompartida {
    Usuario usuarios[10];
    int totalUsuarios;
};

class Servidor {
private:
    MemoriaCompartida* memoria;
    int puerto;
    int socketServidor;
    fd_set conjuntoSockets;  // 🔹 Manejador de múltiples conexiones
    int socketMaximo;  // 🔹 El socket con el número más alto

public:
    Servidor(string configFile) {
        leerConfiguracion(configFile);
        iniciarMemoriaCompartida();
        iniciarServidor();
    }

    void iniciarMemoriaCompartida() {
        int fd = shm_open("/memoriaUsuarios", O_CREAT | O_RDWR, 0666);
        ftruncate(fd, sizeof(MemoriaCompartida));
        memoria = (MemoriaCompartida*)mmap(nullptr, sizeof(MemoriaCompartida),
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (memoria == MAP_FAILED) {
            throw runtime_error("Error al asignar memoria compartida.");
        }

        memoria->totalUsuarios = 0; // 🔹 Reiniciar la memoria al iniciar
    }

    void leerConfiguracion(string configFile) {
        ifstream archivo(configFile);
        if (!archivo) {
            throw runtime_error("No se pudo abrir el archivo de configuración.");
        }
        archivo >> puerto;
    }

    void iniciarServidor() {
        socketServidor = socket(AF_INET, SOCK_STREAM, 0);
        if (socketServidor == -1) {
            throw runtime_error("Error al crear el socket del servidor.");
        }

        int opcion = 1;
        setsockopt(socketServidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));

        sockaddr_in direccionServidor{};
        direccionServidor.sin_family = AF_INET;
        direccionServidor.sin_addr.s_addr = INADDR_ANY;
        direccionServidor.sin_port = htons(puerto);

        if (::bind(socketServidor, (struct sockaddr*)&direccionServidor, sizeof(direccionServidor)) < 0) {
            throw runtime_error("Error al enlazar el socket.");
        }

        if (listen(socketServidor, 10) < 0) {
            throw runtime_error("Error al escuchar conexiones.");
        }

        cout << "✅ Servidor iniciado en el puerto " << puerto << endl;

        // 🔹 Configurar `select()`
        FD_ZERO(&conjuntoSockets);
        FD_SET(socketServidor, &conjuntoSockets);
        socketMaximo = socketServidor;
    }

    void registrarUsuario(string nombre, int socketUsuario) {
        if (memoria->totalUsuarios < 10) {
            memoria->usuarios[memoria->totalUsuarios] = Usuario(nombre, "127.0.0.1", socketUsuario);
            memoria->totalUsuarios++;
            cout << "✅ Usuario registrado: " << nombre << " (Socket FD: " << socketUsuario << ")\n";
        } else {
            cerr << "❌ Máximo de usuarios alcanzado.\n";
        }
    }
    
    void manejarConexiones() {
        fd_set lecturaSockets;
    
        while (true) {
            lecturaSockets = conjuntoSockets;  // Copiar la lista de sockets
            int actividad = select(socketMaximo + 1, &lecturaSockets, nullptr, nullptr, nullptr);
    
            if (actividad < 0) {
                cerr << "Error en select()" << endl;
                continue;
            }
    
            // 🔹 Verificar si hay una nueva conexión entrante
            if (FD_ISSET(socketServidor, &lecturaSockets)) {
                sockaddr_in direccionCliente{};
                socklen_t tamanoCliente = sizeof(direccionCliente);
                int nuevoSocket = accept(socketServidor, (struct sockaddr*)&direccionCliente, &tamanoCliente);
    
                if (nuevoSocket < 0) {
                    cerr << "Error al aceptar conexión." << endl;
                    continue;
                }
    
                char buffer[50] = {0};
                recv(nuevoSocket, buffer, 50, 0);
                string nombreUsuario(buffer);
                nombreUsuario.erase(nombreUsuario.find_last_not_of(" \n\r\t") + 1);
    
                registrarUsuario(nombreUsuario, nuevoSocket);
    
                // 🔹 Agregar el nuevo socket al conjunto
                FD_SET(nuevoSocket, &conjuntoSockets);
                if (nuevoSocket > socketMaximo) {
                    socketMaximo = nuevoSocket;
                }
    
                cout << "👤 Nuevo usuario conectado: " << nombreUsuario 
                     << " (Socket FD: " << nuevoSocket << ")\n";
            }
    
            // 🔹 Verificar si hay mensajes de usuarios
            for (int i = 0; i <= socketMaximo; i++) {
                if (FD_ISSET(i, &lecturaSockets) && i != socketServidor) {
                    char buffer[1024] = {0};
                    int bytesRecibidos = recv(i, buffer, 1024, 0);
    
                    if (bytesRecibidos <= 0) {
                        cout << "❌ Usuario desconectado (Socket FD: " << i << ")\n";
                        close(i);
                        FD_CLR(i, &conjuntoSockets);
                        continue;
                    }
    
                    string mensaje(buffer);
                    size_t pos = mensaje.find(": ");
                    if (pos == string::npos) {
                        cerr << "⚠ Formato de mensaje incorrecto." << endl;
                        continue;
                    }
    
                    string destinatario = mensaje.substr(0, pos);
                    string contenido = mensaje.substr(pos + 2);
    
                    // 🔹 Determinar quién envió el mensaje
                    string remitente = "[Desconocido]";
                    for (int k = 0; k < memoria->totalUsuarios; k++) {
                        if (memoria->usuarios[k].socketUsuario == i) {
                            remitente = string(memoria->usuarios[k].nombre);
                            break;
                        }
                    }
    
                    cout << "\n📢 Mensaje recibido:\n";
                    cout << "Remitente: " << remitente << "\n";
                    cout << "Destinatario: " << destinatario << "\n";
                    cout << "Contenido: " << contenido << "\n";
    
                    // 🔹 Buscar el destinatario en la memoria compartida
                    bool encontrado = false;
                    for (int j = 0; j < memoria->totalUsuarios; j++) {
                        if (strcmp(memoria->usuarios[j].nombre, destinatario.c_str()) == 0) {
                            encontrado = true;

                            replace(contenido.begin(), contenido.end(), '\n', ' ');
                            replace(contenido.begin(), contenido.end(), '\r', ' ');
                            replace(remitente.begin(), remitente.end(), '\n', ' ');
                            replace(remitente.begin(), remitente.end(), '\r', ' ');
                            
                            // Construir mensaje correctamente
                            string mensajeFinal = "📨 " + contenido + "(" + remitente + ")\n\r";
    
                            // Enviar mensaje solo al destinatario
                            send(memoria->usuarios[j].socketUsuario, mensajeFinal.c_str(), mensajeFinal.size(), 0);
                            cout << "📨 Mensaje enviado de " << remitente << " a " << destinatario << "\n";
                            break;
                        }
                    }
    
                    if (!encontrado) {
                        cerr << "⚠ Usuario destinatario no encontrado: '" << destinatario << "'" << endl;
                    }
                }
            }
        }
    }
    
    ~Servidor() {
        close(socketServidor);
    }
};
