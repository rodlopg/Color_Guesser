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
                int r = 0, g = 0, b = 0;
                printf("\nIngresa tu intento (Tres numeros separados por espacio R G B): ");
                
                if (scanf("%d %d %d", &r, &g, &b) == 3) {
                    char msg_send[256];
                    snprintf(msg_send, sizeof(msg_send), "GUESS|%d|%d|%d\n", r, g, b);
                    send(sd, msg_send, strlen(msg_send), 0);
                } else {
                    /* Manejo de error si el usuario no mete numeros */
                    printf("Entrada invalida. Enviando color 0 0 0 por defecto.\n");
                    send(sd, "GUESS|0|0|0\n", 12, 0);
                    
                    /* Limpiar el buffer del teclado */
                    int c;
                    while ((c = getchar()) != '\n' && c != EOF);
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
