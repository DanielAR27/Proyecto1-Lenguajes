#pragma once

#include <iostream>
#include <vector>
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

using namespace std;

class Usuario {
public:
    char nombre[50];
    char ip[INET_ADDRSTRLEN];
    int socketUsuario;

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

        // 🔹 Reiniciar la memoria compartida al iniciar el servidor
        memoria->totalUsuarios = 0;
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

        if (bind(socketServidor, (struct sockaddr*)&direccionServidor, sizeof(direccionServidor)) < 0) {
            throw runtime_error("Error al enlazar el socket.");
        }

        if (listen(socketServidor, 10) < 0) {
            throw runtime_error("Error al escuchar conexiones.");
        }

        cout << "✅ Servidor iniciado en el puerto " << puerto << endl;
    }

    void registrarUsuario(string nombre, string ip, int socketUsuario) {
        // 🔹 Buscar usuario existente
        for (int i = 0; i < memoria->totalUsuarios; i++) {
            if (strcmp(memoria->usuarios[i].nombre, nombre.c_str()) == 0) {
                cout << "🔄 Usuario '" << nombre << "' ya estaba registrado. Actualizando su socket...\n";
                memoria->usuarios[i].socketUsuario = socketUsuario;  // ✅ Actualizar socket
                strcpy(memoria->usuarios[i].ip, ip.c_str());  // ✅ Asegurar que la IP también se actualice
                return;
            }
        }
    
        // 🔹 Si no está registrado, agregarlo
        if (memoria->totalUsuarios < 10) {
            memoria->usuarios[memoria->totalUsuarios] = Usuario(nombre, ip, socketUsuario);
            memoria->totalUsuarios++;
    
            cout << "✅ Usuario registrado: " << nombre << " (" << ip << " - Socket FD: " << socketUsuario << ")\n";
        } else {
            cerr << "❌ Máximo de usuarios alcanzado.\n";
        }
    
        // 🔹 Imprimir la lista de usuarios después de cada registro para depuración
        cout << "\n📋 Usuarios actualmente registrados:\n";
        for (int i = 0; i < memoria->totalUsuarios; i++) {
            cout << "- " << memoria->usuarios[i].nombre << " (" << memoria->usuarios[i].ip
                 << " - Socket FD: " << memoria->usuarios[i].socketUsuario << ")\n";
        }
    }
    
    void manejarConexiones() {
        while (true) {
            sockaddr_in direccionCliente{};
            socklen_t tamanoCliente = sizeof(direccionCliente);
            int clienteSocket = accept(socketServidor, (struct sockaddr*)&direccionCliente, &tamanoCliente);
    
            if (clienteSocket < 0) {
                cerr << "Error al aceptar conexión." << endl;
                continue;
            }
    
            int pid = fork();
            if (pid == 0) {  
                char buffer[1024] = {0};
                recv(clienteSocket, buffer, 1024, 0);
                string entrada(buffer);
                entrada.erase(entrada.find_last_not_of(" \n\r\t") + 1);
    
                // 🔹 Obtener IP del usuario
                char ipCliente[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &direccionCliente.sin_addr, ipCliente, INET_ADDRSTRLEN);
    
                // 🔹 Registrar o actualizar usuario
                registrarUsuario(entrada, ipCliente, clienteSocket);
    
                while (true) {
                    memset(buffer, 0, sizeof(buffer));
                    int bytesRecibidos = recv(clienteSocket, buffer, 1024, 0);
    
                    if (bytesRecibidos <= 0) {
                        cout << "❌ Usuario desconectado: " << entrada << endl;
                        close(clienteSocket);
                        exit(0);
                    }
    
                    string mensaje(buffer);
                    size_t pos = mensaje.find(": ");
                    if (pos == string::npos) {
                        cerr << "⚠ Formato de mensaje incorrecto." << endl;
                        continue;
                    }
    
                    string destinatario = mensaje.substr(0, pos);
                    string contenido = mensaje.substr(pos + 2);
                    string remitente = entrada;
    
                    cout << "\n📢 Mensaje recibido:\n";
                    cout << "Remitente: " << remitente << "\n";
                    cout << "Destinatario: " << destinatario << "\n";
                    cout << "Contenido: " << contenido << "\n";
    
                    bool encontrado = false;
                    for (int i = 0; i < memoria->totalUsuarios; i++) {
                        if (destinatario == memoria->usuarios[i].nombre) {
                            encontrado = true;
    
                            // 🔹 Agregar `fflush(stdout)` en el lado del servidor
                            string mensajeFinal = "📩 " + remitente + ": " + contenido + "\n";
                            send(memoria->usuarios[i].socketUsuario, mensajeFinal.c_str(), mensajeFinal.size(), 0);
                            cout << "📨 Mensaje enviado de " << remitente << " a " << destinatario << "\n";
                            fflush(stdout); // 🔹 Forzar salida inmediata en la terminal
    
                            break;
                        }
                    }
    
                    if (!encontrado) {
                        cerr << "⚠ Usuario destinatario no encontrado: '" << destinatario << "'" << endl;
                    } else {
                        cout << "✅ Mensaje enviado correctamente.\n";
                    }
                }
            }
        }
    }
        
    ~Servidor() {
        close(socketServidor);
    }
};
