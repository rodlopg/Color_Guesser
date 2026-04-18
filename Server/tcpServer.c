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
#include <stdbool.h>
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

#include<sys/types.h>  //required to use getpid()
#include <unistd.h>    //required to use fork()

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
#define MAX_USERNAME_LEN 64

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

typedef struct {
    int socket;
    int idx;
    pid_t PID;
    char username[MAX_USERNAME_LEN];
    bool is_active;
    float score;
    int pipe_guess[2];
    int pipe_cmd[2];
} Player;

/* Session global */
GameState game_state = STATE_WAITING_FIRST;
Player    Players[MAX_PLAYERS];
int       current_round        = 0;
int       players_connected    = 0;
pid_t     server_PID;
Color     colors[TOTAL_ROUNDS];   /* generados antes de empezar */

bool i_am_child(){
    return server_PID != getpid();
}

bool i_am_father(){
    return server_PID == getpid();
}

int compare_players(const void *a, const void *b) {
    Player *p1 = (Player *)a;
    Player *p2 = (Player *)b;

    if (p2->score > p1->score) return 1;
    if (p2->score < p1->score) return -1;
    return 0;
}

void sort_players_by_score() {
    qsort(Players, players_connected, sizeof(Player), compare_players);
}

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
    for(int i = 0; i < players_connected; i++){
        close(Players[i].pipe_guess[0]);
        close(Players[i].pipe_guess[1]);
        Players[i].score = 0;
        Players[i].is_active = false;
    }
    players_connected = 0;
    game_state     = STATE_WAITING_FIRST;
    current_round  = 0;
    printf("[Server] Sesión reiniciada. Esperando nuevos jugadores...\n");
}

/* Función: manda mensajes a 1 solo cliente al cliente */
void send_to(int socket, const char* format, ...) {
    char buffer[jsonSIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    strcat(buffer, "\n");
    send(socket, buffer, strlen(buffer), 0);
    printf("Enviado a %d: %s", socket, buffer);
}

void send_all(const char* format, ...) {
    char buffer[jsonSIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    strcat(buffer, "\n");
    for(int i = 0; i < players_connected; i++){
        if(Players[i].is_active){
            send(Players[i].socket, buffer, strlen(buffer), 0);
            printf("Enviado a %d: %s", Players[i].socket, buffer);
        }
    }
    
}

int receive_message(char* message, int socket){
    int n = recv(socket, message, msgSIZE - 1, 0);
    if (n <= 0) return n;
    message[n] = '\0';
    if (n > 0 && message[n-1] == '\n') message[n-1] = '\0';
    printf("\n[Log] %d dice: %s\n", socket, message);

    return n;
}

void child_process(Player *p){
    char msg[msgSIZE];
    char cmd[msgSIZE];

    close(p->pipe_cmd[1]);    /* el hijo solo LEE de pipe_cmd */
    close(p->pipe_guess[0]);  /* el hijo solo ESCRIBE en pipe_guess */

    printf("[Hijo pid=%d] Manejando a %s (fd=%d)\n",
           getpid(), p->username, p->socket);
    
    while(1){
        /* Esperando que el padre escriba a través del pipe al hijo */
        int n = read(p->pipe_cmd[0], cmd, sizeof(cmd)-1);
        if(n <= 0){ // Ocurrió un error o el padre cerró el flujo de lectura
            printf("[Hijo pid=%d] Pipe de comandos cerrado. Saliendo.\n", getpid());
            break;
        }
        cmd[n] = '\0';
        // Quitar el '\n' que utilizamos para verificar los datos
        if (cmd[strlen(cmd)-1] == '\n') cmd[strlen(cmd)-1] = '\0';

        printf("[Hijo pid=%d -> %s] Comando recibido del padre: %s\n",
               getpid(), p->username, cmd);
        
        if(strcmp(cmd, "INPUT_PHASE") == 0){
            /* Reenviar INPUT_PHASE al cliente */
            send_to(p->socket, "INPUT_PHASE");

            int r = receive_message(msg, p->socket);
            if (r <= 0) {
                /* Cliente desconectado: mandar señal especial al padre */
                write(p->pipe_guess[1], "DISCONNECT\n", 11);
                break;
            }

            printf("[Hijo pid=%d] %s envió: %s\n", getpid(), p->username, msg);

            /* Reenviar GUESS al padre */
            char out[msgSIZE + 2];
            snprintf(out, sizeof(out), "%s\n", msg);

            write(p->pipe_guess[1], out, strlen(out));
        } else if (strcmp(cmd, "END") == 0) {
            send_to(p->socket, "END");
            break;
        } else {
            /* Cualquier otro comando se reenvía directo */
            send_to(p->socket, "%s", cmd);
        }
    }
    /* Se liberan canales accesibles por el hijo antes de terminar */
        close(p->pipe_cmd[0]);
        close(p->pipe_guess[1]);
        close(p->socket);
        exit(0);
}

/* Funciones para que el padre escriba a los hijos por el pipe */

void cmd_send_to(int i, const char *format, ...){
    char buffer[jsonSIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer) - 2, format, args);
    va_end(args);

    size_t len = strlen(buffer);
    if (len == 0 || buffer[len-1] != '\n') {
        buffer[len]   = '\n';
        buffer[len+1] = '\0';
    }

    write(Players[i].pipe_cmd[1], buffer, strlen(buffer));
    printf("[Padre -> hijo %s] %s", Players[i].username, buffer);
    
}

void cmd_send_all(const char *format, ...){
    char buffer[jsonSIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer)-2, format, args);
    va_end(args);

    size_t len = strlen(buffer);
    if (len == 0 || buffer[len-1] != '\n') {
        buffer[len]   = '\n';
        buffer[len+1] = '\0';
    }

    for(int i = 0; i < players_connected; i++){
            write(Players[i].pipe_cmd[1], buffer, strlen(buffer));
            printf("[Padre -> hijo %s] %s", Players[i].username, buffer);
    }
}

void add_player(int idx, int new_sd, char *new_uname){
    if(pipe(Players[idx].pipe_guess) < 0){
        perror("pipe_guess");
        exit(1);
    }
    if(pipe(Players[idx].pipe_cmd) < 0){
        perror("pipe_cmd");
        exit(1);
    }
    
    Players[idx].socket    = new_sd;
    Players[idx].is_active = true;
    Players[idx].score     = 0.0;
    Players[idx].PID = server_PID + idx;
    Players[idx].idx = idx;

    strncpy(Players[idx].username, new_uname, MAX_USERNAME_LEN - 1);
}

void fork_players(){
    for(int i = 0; i < players_connected; i++){
        pid_t pid = fork();
        if(pid == 0){
            // Cerrar pipes de los otros jugadores
            for(int j = 0; j < players_connected; j++){
                if(j == i) continue;
                close(Players[j].pipe_cmd[0]);
                close(Players[j].pipe_cmd[1]);
                close(Players[j].pipe_guess[0]);
                close(Players[j].pipe_guess[1]);
            }
            close(sd);  // tampoco necesita el socket de escucha
            child_process(&Players[i]);
            exit(0);
        } else if(pid > 0){    // Padre
            Players[i].PID = pid;
        } else {
            perror("fork");
            exit(1);
        }
    }

    for(int i = 0; i < players_connected; i++){
        close(Players[i].pipe_cmd[0]);
        close(Players[i].pipe_guess[1]);
    }
}



int main(){
    server_PID = getpid();
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
    
	char sigue='S';
	char msgReceived[1000];
	//strcpy(json," ");
	json[0] = '\0';
	while (sigue == 'S') {
        switch (game_state) {
            case STATE_WAITING_FIRST:
                printf("\n[Log] Esperando primer Jugador: %s\n", msg);
                /* esperando que un cliente solicite un servicio */
                if ((sd_actual = accept(sd, (struct sockaddr *)&pin, &addrlen)) == -1) {
                    perror("accept");
                    exit(1);
                }
                receive_message(msg,sd_actual);

                char username[64] = "";
                if (strncmp(msg, "JOIN|", 5) == 0) {
                    strncpy(username, msg + 5, sizeof(username) - 1);
                    if (strlen(username) > 0) {
                        add_player(players_connected++, sd_actual, username);

                        game_state = STATE_WAITING_ALL;
                    } else {
                        send_to(sd_actual, "ERROR|username_invalido");
                        break; 
                    }
                } else {
                    send_to(sd_actual, "ERROR|esperando_JOIN");
                    break;
                }
                break;

            case STATE_WAITING_ALL:
                int client_sd;
                
                for (int t = COUNTDOWN_SECS; t >= 0; t--) {
                    fd_set readfds;
                    FD_ZERO(&readfds);
                    FD_SET(sd, &readfds);
                    struct timeval timeout;
                    timeout.tv_sec = 1;   // revisa cada 1 segundo
                    timeout.tv_usec = 0;

                    int activity = select(sd + 1, &readfds, NULL, NULL, &timeout);

                    if(activity < 0){
                        perror("select error");
                        break;
                    } else if (FD_ISSET(sd, &readfds)){
                        if((client_sd = accept(sd, (struct sockaddr *)&pin, &addrlen)) >= 0){
                            char username[64] = "";
                            receive_message(msg, client_sd);
                            if (strncmp(msg, "JOIN|", 5) == 0) {
                                strncpy(username, msg + 5, sizeof(username) - 1);
                                if (strlen(username) > 0) {
                                    add_player(players_connected++, client_sd, username);
                                } else {
                                    send_to(client_sd, "ERROR|username_invalido");
                                    break; 
                                }
                            } else {
                                send_to(client_sd, "ERROR|esperando_JOIN");
                                break;
                            }
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    send_all("COUNTDOWN|%d", t);
                    sleep(1);
                }
                game_state = STATE_START;
                break;

            case STATE_START:
                generate_colors();
                fork_players();
                
                if(i_am_father()){
                    current_round = 1;
                    cmd_send_all("START");
                    cmd_send_all("WAIT");
                    game_state = STATE_ROUND;
                }
                break;
                
            case STATE_ROUND:
                if(i_am_father()){
                    cmd_send_all("ROUND|%d", current_round);
                    sleep(1);
                    game_state = STATE_SHOW_COLOR;
                }
                break;

            case STATE_SHOW_COLOR:
                cmd_send_all("SHOW_COLOR|%d|%d|%d", 
                            colors[current_round-1].r, 
                            colors[current_round-1].g, 
                            colors[current_round-1].b);
                sleep(3);
                game_state = STATE_INPUT_PHASE;
                break; 

            case STATE_INPUT_PHASE:
                cmd_send_all("INPUT_PHASE");

                for(int i = 0; i < players_connected; i++){
                    char guess[msgSIZE];
                    int n = read(Players[i].pipe_guess[0], guess, sizeof(guess)-1);
                    
                    if(n > 0){
                        printf("[Log] Lectura recibida: %s", guess);
                        guess[n] = '\0';
                        if(guess[n-1] == '\n') guess[n-1] = '\0';
                        int gr, gg, gb;
                        if (sscanf(guess, "GUESS|%d|%d|%d", &gr, &gg, &gb) == 3) {
                            double sim = compute_similarity(
                                colors[current_round-1].r,
                                colors[current_round-1].g,
                                colors[current_round-1].b,
                                gr, gg, gb
                            );
                            Players[i].score += sim * 100.0;
                        }
                    }
                }
                
                if (current_round < TOTAL_ROUNDS) {
                    current_round++;
                    cmd_send_all("WAIT");
                    sleep(1);
                    game_state = STATE_ROUND;
                } else {
                    cmd_send_all("CALCULATING_RANKINGS"); 
                    sort_players_by_score();
                    cmd_send_all("TOTAL_PLAYERS|%d", players_connected);
                    for(int i = 0; i < players_connected; i++){
                        cmd_send_to(i, "RESULT|%s|%.0f|%d", Players[i].username, Players[i].score, i+1);
                    }
                    
                    cmd_send_all("END");
                    reset_session();
                }
                break;

            default:
                send_to(sd_actual, "ERROR|estado_desconocido");
                break;
        }
    }

/* cerrar los dos sockets */
	close(sd_actual);  
    close(sd);
    printf("Conexion cerrada\n");
	return 0;
}
