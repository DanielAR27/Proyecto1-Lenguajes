#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include "colores.h"

class Cliente
{
private:
    std::string nombreUsuario;
    int socketCliente;
    std::atomic<bool> registrado;
    std::thread hiloConexion;

    // Cambiado a público temporalmente para solucionar el problema
public:
    void mantenerConexion();

public:
    Cliente();
    ~Cliente();
    void mostrarMenuInicial();
    void registrarUsuario();
    void menuPrincipal();
    void ejecutar();
};