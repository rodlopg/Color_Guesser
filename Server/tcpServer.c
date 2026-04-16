/*

   Lectura remota de una palabra para devolver el numero de vocales usando sockets pertenecientes
   a la familia TCP, en modo conexion.
   Codigo del servidor

   Nombre Archivo: tcpserver.c   
   Fecha: Febrero 2023

   Compilacion: cc tcpserver.c -lnsl -lm -o tcpserver
   Ejecución: ./tcpserver
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* The following headers was required in old or some compilers*/
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>	// it is required to call signal handler functions
#include <unistd.h>  // it is required to close the socket descriptor
#include <math.h>
#include <time.h>
#include <stdarg.h>

#define  jsonSIZE  10000
#define  msgSIZE   2048      /* longitud maxima parametro entrada/salida */
#define  PUERTO    5000	     /* numero puerto arbitrario */

int                  sd, sd_actual;  /* descriptores de sockets */
int                  addrlen;        /* longitud msgecciones */
struct sockaddr_in   sind, pin;      /* msgecciones sockets cliente u servidor */


/*  procedimiento de aborte del servidor, si llega una senal SIGINT */
/* ( <ctrl> <c> ) se cierra el socket y se aborta el programa       */
void aborta_handler(int sig){
   printf("....abortando el proceso servidor %d\n",sig);
   close(sd);  
   close(sd_actual); 
   exit(1);
}

/* ===== ESTADO DEL JUEGO ===== */
#define MAX_PLAYERS  8
#define TOTAL_ROUNDS 5
#define COUNTDOWN_SECS 5

typedef enum {
    STATE_WAITING_FIRST,
    STATE_WAITING_ALL,
    STATE_START,
    STATE_ROUND,
    STATE_SHOW_COLOR,
    STATE_INPUT_PHASE,
    STATE_WAIT,
    STATE_CALCULATING_RANKINGS,
    STATE_DONE
} GameState;

typedef struct {
    int r, g, b;
} Color;

/* Session global */
GameState game_state = STATE_WAITING_FIRST;
char      username[64]         = "";
double    player_score         = 0.0;
int       current_round        = 0;
Color     colors[TOTAL_ROUNDS];   /* generados antes de empezar */

/* Función: genera colores aleatorios para la sesión */
void generate_colors() {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < TOTAL_ROUNDS; i++) {
        colors[i].r = rand() % 256;
        colors[i].g = rand() % 256;
        colors[i].b = rand() % 256;
    }
}

/* Función: similitud por producto punto normalizado (dot product similarity) */
double compute_similarity(int r1, int g1, int b1, int r2, int g2, int b2) {
    double dot  = r1*r2 + g1*g2 + b1*b2;
    double mag1 = sqrt(r1*r1 + g1*g1 + b1*b1);
    double mag2 = sqrt(r2*r2 + g2*g2 + b2*b2);
    if (mag1 == 0 || mag2 == 0) return 0.0;
    return dot / (mag1 * mag2);  /* valor entre 0.0 y 1.0 */
}

/* Función: reinicia la sesión para una nueva partida */
void reset_session() {
    game_state     = STATE_WAITING_FIRST;
    username[0]    = '\0';
    player_score   = 0.0;
    current_round  = 0;
}

/* Función: manda el estado del juego al cliente */
void send_state(const char* format, ...) {
    char buffer[jsonSIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Agregamos el salto de línea para el protocolo del cliente
    strcat(buffer, "\n");
    send(sd_actual, buffer, strlen(buffer), 0);
    printf("Enviado al cliente: %s", buffer);
}

int main(){
  
	char  msg[msgSIZE];	     /* parametro entrada y salida */
	char  json[jsonSIZE];	     /* parametro entrada y salida */

	/*
	When the user presses <Ctrl + C>, the aborta_handler function will be called, 
	and such a message will be printed. 
	Note that the signal function returns SIG_ERR if it is unable to set the 
	signal handler, executing line 54.
	*/	
   if(signal(SIGINT, aborta_handler) == SIG_ERR){
   	perror("Could not set signal handler");
      return 1;
   }
       //signal(SIGINT, aborta);      /* activando la senal SIGINT */

/* obtencion de un socket tipo internet */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
int opt = 1;
if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
    perror("setsockopt");
    exit(1);
}
/* asignar msgecciones en la estructura de msgecciones */
	sind.sin_family = AF_INET;
	sind.sin_addr.s_addr = INADDR_ANY;   /* INADDR_ANY=0x000000 = yo mismo */
	sind.sin_port = htons(PUERTO);       /*  convirtiendo a formato red */

/* asociando el socket al numero de puerto */
	if (bind(sd, (struct sockaddr *)&sind, sizeof(sind)) == -1) {
		perror("bind");
		exit(1);
	}

/* ponerse a escuchar a traves del socket */
	if (listen(sd, 5) == -1) {
		perror("listen");
		exit(1);
	}
addrlen = sizeof(pin);
/* esperando que un cliente solicite un servicio */
	if ((sd_actual = accept(sd, (struct sockaddr *)&pin, &addrlen)) == -1) {
		perror("accept");
		exit(1);
	}
	char sigue='S';
	char msgReceived[1000];
	//strcpy(json," ");
	json[0] = '\0';
	while (sigue == 'S') {
        int n = recv(sd_actual, msg, sizeof(msg) - 1, 0);
        if (n <= 0) break;
        msg[n] = '\0';
        if (n > 0 && msg[n-1] == '\n') msg[n-1] = '\0';
        printf("\n[Log] Cliente dice: %s\n", msg);

        switch (game_state) {
            case STATE_WAITING_FIRST:
                if (strncmp(msg, "JOIN|", 5) == 0) {
                    strncpy(username, msg + 5, sizeof(username) - 1);
                    if (strlen(username) > 0) {
                        send_state("WAIT");
                        game_state = STATE_WAITING_ALL;
						
                    } else {
                        send_state("ERROR|username_invalido");
                        break; 
                    }
                } else {
                    send_state("ERROR|esperando_JOIN");
                    break;
                }

            case STATE_WAITING_ALL:
                for (int t = COUNTDOWN_SECS; t >= 0; t--) {
                    send_state("COUNTDOWN|%d", t);
                    sleep(1);
                }
                game_state = STATE_START;

            case STATE_START:
                generate_colors();
                current_round = 1;
                send_state("START");
                game_state = STATE_ROUND;

            case STATE_ROUND:
                send_state("ROUND|%d", current_round);
                sleep(1);
                game_state = STATE_SHOW_COLOR;

            case STATE_SHOW_COLOR:
                send_state("SHOW_COLOR|%d|%d|%d", 
                            colors[current_round-1].r, 
                            colors[current_round-1].g, 
                            colors[current_round-1].b);
                sleep(3);
                game_state = STATE_INPUT_PHASE;
                send_state("INPUT_PHASE");
                break; 

            case STATE_INPUT_PHASE:
                if (strncmp(msg, "GUESS|", 6) == 0) {
                    int gr, gg, gb;
                    if (sscanf(msg + 6, "%d|%d|%d", &gr, &gg, &gb) == 3) {
                        double sim = compute_similarity(colors[current_round-1].r, colors[current_round-1].g, colors[current_round-1].b, gr, gg, gb);
                        player_score += (sim * 100.0);

                        if (current_round < TOTAL_ROUNDS) {
                            current_round++;
                            send_state("WAIT");
                            sleep(1);

                            // Mandamos la info de la nueva ronda
                            send_state("ROUND|%d", current_round);
                            send_state("SHOW_COLOR|%d|%d|%d", colors[current_round-1].r, colors[current_round-1].g, colors[current_round-1].b);
                            sleep(3);
                            send_state("INPUT_PHASE");

                            // Nos quedamos en INPUT_PHASE esperando el siguiente GUESS
                            game_state = STATE_INPUT_PHASE; 
                        } else {
                            send_state("CALCULATING_RANKINGS"); 
                            send_state("RESULT|%s|%.0f|1", username, player_score);
                            send_state("END");
                            sigue = 'N';
                        }
                    }
                }
                break;

            default:
                send_state("ERROR|estado_desconocido");
                break;
        }
    }

/* cerrar los dos sockets */
	close(sd_actual);  
   close(sd);
   printf("Conexion cerrada\n");
	return 0;
}
