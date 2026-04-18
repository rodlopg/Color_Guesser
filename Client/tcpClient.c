/*
   Nombre Archivo: tcpclient.c   
   Compilacion: gcc tcpclient.c -o tcpclient
   Ejecucion: ./tcpclient <host> <usuario>  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>  
#include <netdb.h>

#define  DIRSIZE    2048   /* longitud maxima parametro entrada/salida */
#define  PUERTO     5000   /* numero puerto arbitrario */
#define  MSGSIZE    2048   /* longitud de los mensajes */

int main(int argc, char *argv[]) {
    int                 sd;        
    struct hostent     *hp;        
    struct sockaddr_in  pin;       
    char               *host;      
    char               *username;

    /* verificando el paso de parametros */
    if ( argc != 3) {
        fprintf(stderr,"Error uso: %s <host> <nombre_usuario> \n",argv[0]);
        exit(1);
    }
    
    host = argv[1];
    username = argv[2];

    /* encontrando todo lo referente acerca de la maquina host */
    if ( (hp = gethostbyname(host)) == 0) {
        perror("gethostbyname");
        exit(1);
    }
    
    /* llenar la estructura de direcciones con la informacion del host */
    pin.sin_family = AF_INET;
    pin.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
    pin.sin_port = htons(PUERTO);                    

    /* obtencion de un socket tipo internet */
    if ( (sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* conectandose al PUERTO en el HOST  */
    if ( connect(sd, (struct sockaddr *)&pin, sizeof(pin)) == -1) {
        perror("connect");
        exit(1);
    }

    /* 1. Enviar el comando JOIN inicial al servidor */
    char buffer[MSGSIZE];
    snprintf(buffer, sizeof(buffer), "JOIN|%s\n", username);
    if (send(sd, buffer, strlen(buffer), 0) == -1) {
        perror("send");
        exit(1);
    }

    /* 2. Ciclo principal para escuchar al servidor */
    int playing = 1;
    while (playing) {
        memset(buffer, 0, MSGSIZE);
        int n = recv(sd, buffer, MSGSIZE - 1, 0);
        
        if (n <= 0) {
            printf("\nConexion cerrada por el servidor.\n");
            break;
        }
        
        buffer[n] = '\0';

        /* Separar mensajes por salto de línea (\n) porque TCP puede juntarlos */
        char *line = strtok(buffer, "\n");
        
        while (line != NULL) {
            /* En C no se puede usar switch con strings. 
               Usamos if/else if con strncmp para evaluar el comando.
            */
            if (strncmp(line, "WAIT", 4) == 0) {
                printf("Server: Esperando turno/jugadores...\n");
            } 
            else if (strncmp(line, "COUNTDOWN|", 10) == 0) {
                int t;
                sscanf(line + 10, "%d", &t);
                printf("Server: El juego inicia en: %d\n", t);
            } 
            else if (strncmp(line, "START", 5) == 0) {
                printf("\n=============================\n");
                printf("Server: ¡EL JUEGO HA COMENZADO!\n");
                printf("=============================\n");
            } 
            else if (strncmp(line, "ROUND|", 6) == 0) {
                int r;
                sscanf(line + 6, "%d", &r);
                printf("\nRONDA %d\n", r);
            } 
            else if (strncmp(line, "SHOW_COLOR|", 11) == 0) {
                int r, g, b;
                sscanf(line + 11, "%d|%d|%d", &r, &g, &b);
                printf("Server: Memoriza este color: R=%d, G=%d, B=%d\n", r, g, b);
            } 
            else if (strncmp(line, "INPUT_PHASE", 11) == 0) {
                printf("\nIngresa tu intento (Tres numeros separados por espacio R G B) [Tienes 28 seg]: ");
                fflush(stdout); /* Imprimir texto antes de leer input */

                fd_set readfds;
                struct timeval tv = {28, 0};
                FD_ZERO(&readfds);
                FD_SET(0, &readfds); /* Escuchar al teclado  */
                
                /* Esperara hasta que el usuario escriba, o hasta que pasen los 28s */
                int activity = select(1, &readfds, NULL, NULL, &tv);
                
                if (activity > 0) {
                    /* Si el usuario escribió algo antes de los 28 segundos */
                    char input_buf[256];
                    int r = 0, g = 0, b = 0;

                    /* Usamos fgets en lugar de scanf para limpiar bien el salto de línea */
                    if (fgets(input_buf, sizeof(input_buf), stdin) != NULL && sscanf(input_buf, "%d %d %d", &r, &g, &b) == 3) {
                        char msg_buffer[256];
                        snprintf(msg_buffer, sizeof(msg_buffer), "GUESS|%d|%d|%d\n", r, g, b);
                        send(sd, msg_buffer, strlen(msg_buffer), 0);
                    } else {
                        printf("Entrada invalida. Enviando color 0 0 0 por defecto.\n");
                        send(sd, "GUESS|0|0|0\n", 12, 0);
                    }
                } else{
                    printf("\n¡Tiempo agotado! Se acabó tu turno.\n");
                    send(sd, "GUESS|0|0|0\n", 12, 0);
                }
            }
            else if (strncmp(line, "CALCULATING_RANKINGS", 20) == 0) {
                printf("\nServer: Calculando los resultados finales...\n");
            } 
            else if (strncmp(line, "RESULT|", 7) == 0) {
                char usr[64];
                float score;
                int rank;
                sscanf(line + 7, "%[^|]|%f|%d", usr, &score, &rank);
                printf("Server: RANKING -> Jugador: %s | Puntos: %.0f | Posicion: %d\n", usr, score, rank);
            } 
            else if (strncmp(line, "END", 3) == 0) {
                printf("Server: Fin del juego. ¡Gracias por jugar!\n");
                playing = 0; /* Esto rompe el while principal */
            } 
            else if (strncmp(line, "ERROR|", 6) == 0) {
                printf("[Error del Servidor] %s\n", line + 6);
            } 
            else {
                /* Por si el servidor manda algo que no contemplamos */
                printf("[Log de Servidor]: %s\n", line);
            }

            /* Procesar el siguiente mensaje en el buffer (si existe) */
            line = strtok(NULL, "\n");
        }
    }
    close(sd);
    return 0;
}
