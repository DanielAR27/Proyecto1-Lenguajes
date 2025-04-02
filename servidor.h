#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/select.h>
#include "colores.h"

class Usuario
{
public:
    std::string nombre;
    std::string ip;
    int socketUsuario;

    Usuario() {}
    Usuario(const std::string &n, const std::string &i, int s)
        : nombre(n), ip(i), socketUsuario(s) {}
};

class Servidor
{
private:
    std::vector<Usuario> usuarios;
    int puerto;
    int socketServidor;
    fd_set conjuntoSockets;
    int socketMaximo;

public:
    Servidor(const std::string &configFile);
    void iniciarServidor();
    void registrarUsuario(const std::string &nombre, int socketUsuario);
    void manejarConexiones();
    void eliminarUsuario(int socketUsuario);
    ~Servidor();
};