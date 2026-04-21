# Color Guesser

Real-time multiplayer color-matching game built in C with TCP sockets.

| Name | ID | GitHub |
|---|---|---|
| José Carlos Béjar Gómez | 0262149 | [@CarlosBejar65](https://github.com/CarlosBejar65) |
| Carlos Santiago Cruz Díaz | 0264547 | [@CSCD13](https://github.com/CSCD13) |
| Rodrigo López Gómez | 0262146 | [@rodlopg](https://github.com/rodlopg) |

---

## How to Play

1. Enter a username and click **Play** — a 15-second lobby starts for others to join.
2. Each round, memorize the color shown, then recreate it with R/G/B sliders.
3. The closer your guess, the more points you get. Best of 5 rounds wins.

---

## Run

Requires Docker Desktop.

```bash
docker-compose up --build   # first time
docker-compose up           # subsequent runs
docker-compose down         # stop
```

**Mobile:** open `http://<server-ip>:5001` on the same Wi-Fi (e.g. `http://192.168.1.77:5001`).

---

## Architecture

```
Player(s) → [Browser: GUI + client logic] ──TCP──► [Game Server (C, Docker)]
                                          ◄────────
```

The server controls everything: generates 5 colors before the game starts, broadcasts all game events, and collects/scores guesses. Clients only send `JOIN|username` and `GUESS|R|G|B`.

**Scoring:** `similarity = (A · B) / (|A| × |B|)` — dot product of RGB color vectors.

---

## Protocol

`TOKEN|value1|value2\n`

| Direction | Messages |
|---|---|
| Server → Client | `START`, `WAIT`, `COUNTDOWN\|N`, `ROUND\|N`, `SHOW_COLOR\|R\|G\|B`, `INPUT_PHASE`, `RESULT\|user\|score\|pos`, `END` |
| Client → Server | `JOIN\|username`, `GUESS\|R\|G\|B` |

---

## Stack

| Layer | Technology |
|---|---|
| Game Server | C · POSIX TCP sockets · `fork()` per client |
| GUI | HTML / CSS / JS |
| GUI Bridge | Python (Flask + Socket.IO) — relays player input from browser to C server |
| Deployment | Docker · No external libraries · No persistent storage |

---

## Responsibilities

| Member | Role |
|---|---|
| José Carlos Béjar | GUI (all screens, transitions, error states) |
| Carlos Santiago Cruz | Client logic (socket, protocol parser, state machine) |
| Rodrigo López | Server (session manager, game loop, scoring, sync) |

---

[github.com/rodlopg/Color_Guesser](https://github.com/rodlopg/Color_Guesser) · Universidad Panamericana · Distributed Computing · Dr. Juan Carlos López Pimentel
