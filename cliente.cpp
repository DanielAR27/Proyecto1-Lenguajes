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

using namespace std;

// 🎨 Colores ANSI
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define BLUE    "\033[34m"

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << RED << "❌ Error al crear el socket" << RESET << endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5050); 
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << RED << "❌ No se pudo conectar al servidor" << RESET << endl;
        return 1;
    }

    //  Recibir mensaje de bienvenida
    char buffer[1024] = {0};
    recv(sock, buffer, sizeof(buffer), 0);
    cout << GREEN << buffer << RESET;

    // Ingresar nombre
    string nombre;
    getline(cin, nombre);
    send(sock, nombre.c_str(), nombre.length(), 0);

    pid_t pid = fork();

    if (pid == 0) {
        //  Hijo: escucha mensajes del servidor
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;
            cout << GREEN << buffer << RESET << flush;
        }
    } else {
        //  Padre: envía mensajes
        while (true) {
            string mensaje;
            getline(cin, mensaje);
            if (mensaje == "/salir") break;
            send(sock, mensaje.c_str(), mensaje.length(), 0);
            cout << BLUE << "📤 " << mensaje << RESET << endl;
        }

        kill(pid, SIGKILL);  // Finaliza proceso hijo
    }

    close(sock);
    return 0;
}
