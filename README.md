# Servidor de Mensajería

Este es un servidor de mensajería que permite la comunicación entre usuarios mediante sockets.

## Instrucciones de Uso

### Branch
Para ver los cambios más recientes, utilize la rama `v1`:

```bash
git checkout v1
```

### Compilación
Ejecuta el siguiente comando para compilar el programa:

```bash
g++ -o servidor servidor_mensajeria.cpp
```

### Ejecución del Servidor
Para iniciar el servidor, use:

```bash
./servidor
```

### Registro de Usuarios
Cada usuario debe abrir una terminal y ejecutar:

```bash
nc 127.0.0.1 5000
```

- En la primera terminal, ingrese un nombre de usuario.  
- En la segunda terminal, repita el mismo comando y coloque un nombre distinto.

### Formato de Mensajes
Para enviar un mensaje a otro usuario, escriba:

```
Receptor: Mensaje
```

Ejemplo de salida en la terminal del receptor:

```
Hola, ¿cómo estás? (Juan)
```

Esto indica que Juan envió el mensaje "Hola, ¿cómo estás?".

### Notas Adicionales
Si tiene problemas, asegurese de que el puerto `5000` no está en uso ejecutando:

```bash
ss -tulnp | grep 5000
```

Si necesita liberar el puerto, puede reiniciar el servidor o cerrar las conexiones activas.

Autor: Pani
Última actualización: *`$(date +"%Y-%m-%d")`*
