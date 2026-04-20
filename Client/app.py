import os, socket
from flask import Flask, render_template, request
from flask_socketio import SocketIO

app = Flask(__name__)
# Forzamos el uso de 'threading' para que las conexiones TCP no bloqueen la web
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

SERVER_HOST = os.getenv('SERVER_HOST', 'juego-server')
SERVER_PORT = 5000 

# Diccionario mágico: Relaciona cada pestaña del navegador con su propio socket TCP
jugadores_sockets = {}

def escuchar_servidor(sock, sid):
    """Hilo individual que escucha al servidor C para cada jugador"""
    while True:
        try:
            data = sock.recv(2048).decode('utf-8')
            if not data:
                print(f"[{sid}] El servidor de C cerró la conexión.")
                break
            
            # Procesamos cada línea enviada por el servidor
            for line in data.strip().split('\n'):
                line = line.strip()
                if line:
                    print(f"[{sid}] Servidor C dice: {line}")
                    
                    # --- LA MAGIA ESTÁ AQUÍ ---
                    if line.startswith("RESULT"):
                        # Al quitar el 'to=sid', SocketIO hace un BROADCAST.
                        # Le envía este resultado a TODAS las pestañas abiertas, 
                        # sin importar de quién sea el socket original.
                        socketio.emit('mensaje_servidor', {'msg': line})
                    else:
                        # Los demás mensajes (rondas, colores, turnos) siguen siendo 
                        # privados solo para la pestaña que le corresponde.
                        socketio.emit('mensaje_servidor', {'msg': line}, to=sid)
                    # --------------------------
                    
        except Exception as e:
            print(f"[{sid}] Hilo de escucha terminado: {e}")
            break

@socketio.on('unirse')
def join(data):
    sid = request.sid # ID único de la pestaña del navegador
    try:
        user = data.get('user', 'Invitado')
        print(f"[{sid}] Conectando al servidor C para el jugador: {user}")
        
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((SERVER_HOST, SERVER_PORT))
        
        # Guardamos el socket en el diccionario
        jugadores_sockets[sid] = client_socket
        
        # Enviamos el mensaje con el salto de línea
        mensaje = f"JOIN|{user}\n"
        client_socket.sendall(mensaje.encode('utf-8'))
        
        # Iniciamos el hilo exclusivo para este jugador
        socketio.start_background_task(escuchar_servidor, client_socket, sid)
        print(f"[{sid}] ¡Conectado con éxito!")
        
    except Exception as e:
        print(f"[{sid}] ERROR FATAL DE CONEXIÓN: {e}")

@socketio.on('enviar_intento')
def handle_guess(data):
    sid = request.sid
    # Buscamos el socket específico de este jugador
    if sid in jugadores_sockets:
        try:
            msg = f"GUESS|{data['r']}|{data['g']}|{data['b']}\n"
            jugadores_sockets[sid].sendall(msg.encode('utf-8'))
            print(f"[{sid}] Color enviado: {msg.strip()}")
        except Exception as e:
            print(f"[{sid}] Error al enviar color: {e}")

@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5001, allow_unsafe_werkzeug=True)