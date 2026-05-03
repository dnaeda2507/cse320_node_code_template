# CSE 320 - Computer Networks Programming Assignment
## TCP Congestion Control Algorithms Comparison

**Assigned Algorithm:** TCP Reno  
**Name:** Helin Turan, Eda Dana  
**Student ID:** 20240808613, 20210808072

---

### 1. Overview
This project simulates the **TCP Reno** congestion control algorithm. It includes a complete implementation of Slow Start, Congestion Avoidance, Fast Retransmit, and Fast Recovery (including Window Inflation). 

Additionally, a 6-node network topology is implemented using UDP sockets. The routing table is dynamically calculated using **Dijkstra's Shortest Path Algorithm**, ensuring messages are routed optimally rather than just directly (e.g., A to D routes through B).

### How It Works
The program has **two modes**:

**Mode 1 — Event File Simulation** (shows cwnd evolution):
```
./node.exe A.conf reno events/timeout_test.txt
./node.exe A.conf reno events/duplicate_ack_test.txt
./node.exe A.conf reno events/reno_full_demo.txt
./node.exe A.conf reno events/example_events.txt
```

**Mode 2 — Interactive 6-Node UDP Network**:
```
./node.exe A.conf reno
./node.exe B.conf reno
./node.exe C.conf reno
./node.exe D.conf reno
./node.exe E.conf reno
./node.exe F.conf reno
```

### Data Structures
- `TCPState` — holds `cwnd`, `ssthresh`, `dup_ack_count`, `round`, `state` (Slow Start / Congestion Avoidance / Fast Recovery)
- `NodeConfig` — node ID, port, neighbor list from `.conf` file
- `RoutingTable / RouteEntry` — Dijkstra shortest path result per destination
- `Neighbor` — neighbor ID, IP, port, cost

---

## 2. How to Compile

**Windows (MinGW/Git Bash):**
```bash
gcc node.c -o node.exe -lws2_32 -Wall
```

**Linux / macOS:**
```bash
gcc node.c -o node -lpthread -Wall
```

---

## 3. TCP Reno — Algorithm Explanation

### Slow Start
- Starts with `cwnd = 1`
- On each ACK: `cwnd += 1` (doubles each round)
- Continues until `cwnd >= ssthresh`

### Congestion Avoidance
- When `cwnd >= ssthresh`: `cwnd += 1/cwnd` per ACK (additive increase)

### Fast Retransmit + Fast Recovery (KEY difference from Tahoe)
- On **3 duplicate ACKs**:
  - `ssthresh = cwnd / 2`
  - `cwnd = ssthresh + 3` ← does NOT drop to 1!
  - Enters **Fast Recovery** state
- On **4th, 5th... duplicate ACK** (Window Inflation): `cwnd += 1`
- On **full ACK**: `cwnd = ssthresh`, exit to Congestion Avoidance

### Timeout
- `ssthresh = cwnd / 2`
- `cwnd = 1`
- Returns to **Slow Start** (same as Tahoe for this case)

### Why Reno > Tahoe
- Tahoe drops `cwnd = 1` on ANY loss (timeout OR 3 dup ACKs)
- Reno uses **Fast Recovery** on 3 dup ACKs — does not fully restart
- Result: Reno maintains much higher throughput after loss

---

## 4. Input / Output

### Event File Format
```
ACK <number>
DUPACK <number>
TIMEOUT
```

### Scenario Demonstrations

We provided several event files to demonstrate specific behaviors:
- `events/timeout_test.txt` — Demonstrates pure timeout behavior.
- `events/duplicate_ack_test.txt` — Demonstrates pure Fast Recovery behavior.
- `events/reno_full_demo.txt` — A long trace demonstrating all states including Window Inflation.
- `events/example_events.txt` — A concise trace showing both mechanisms.

**Sample Output (`example_events.txt`)**:
```bash
$ ./node.exe A.conf reno events/example_events.txt
```
```text
Round | Event      | ACK    | cwnd         | ssthresh     | State                  | Explanation
------------------------------------------------------------------------------------------------------
    1 | ACK        | ACK=1   | cwnd=  2.00 | ssthresh= 16.00 | Slow Start             | new ACK received; cwnd increased
    2 | ACK        | ACK=2   | cwnd=  3.00 | ssthresh= 16.00 | Slow Start             | new ACK received; cwnd increased
    3 | ACK        | ACK=3   | cwnd=  4.00 | ssthresh= 16.00 | Slow Start             | new ACK received; cwnd increased
    4 | ACK        | ACK=4   | cwnd=  5.00 | ssthresh= 16.00 | Slow Start             | new ACK received; cwnd increased
    5 | DUPACK     | ACK=4   | cwnd=  5.00 | ssthresh= 16.00 | Slow Start             | duplicate ACK received; waiting
    6 | DUPACK     | ACK=4   | cwnd=  5.00 | ssthresh= 16.00 | Slow Start             | duplicate ACK received; waiting
    7 | DUPACK     | ACK=4   | cwnd=  5.50 | ssthresh=  2.50 | Fast Recovery          | 3 dup ACKs; Reno enters Fast Recovery
    8 | ACK        | ACK=5   | cwnd=  2.50 | ssthresh=  2.50 | Congestion Avoidance   | full ACK; exit Fast Recovery
    9 | ACK        | ACK=6   | cwnd=  2.90 | ssthresh=  2.50 | Congestion Avoidance   | new ACK received; additive increase
   10 | TIMEOUT    | ACK=0   | cwnd=  1.00 | ssthresh=  2.00 | Slow Start             | timeout; cwnd reset to 1; back to Slow Start
   11 | ACK        | ACK=7   | cwnd=  2.00 | ssthresh=  2.00 | Congestion Avoidance   | new ACK received; cwnd increased
   12 | ACK        | ACK=8   | cwnd=  2.50 | ssthresh=  2.00 | Congestion Avoidance   | new ACK received; additive increase
```

---

## 5. Routing Table (Node A)

The routing table is computed using **Dijkstra's Algorithm** from the hardcoded 6-node topology.

| Destination | Next Hop | Cost | Path |
|---|---|---|---|
| A | - | 0 | A |
| B | B | 4 | A -> B |
| C | C | 7 | A -> C |
| D | B | 12 | A -> B -> D |
| E | B | 7 | A -> B -> E |
| F | F | 5 | A -> F |

**Key:** A→D direct cost = 13, but A→B→D = 4+8 = **12** (cheaper), so next hop is **B**.

---

## 6. 6-Node UDP Demo

Open 6 terminals and run each node. Then from Node A:
```
send D hello
```
Expected output across terminals:
```
[A] Destination D, next hop B
[B] Forwarding message from A to D, next hop D
[D] Received message from A: hello
```

---

## 7. Closing Argument

The TCP Reno implementation correctly demonstrates the core insight of the algorithm: when 3 duplicate ACKs are received, the network is not fully congested — some packets are still arriving. Reno exploits this by entering Fast Recovery instead of restarting from cwnd=1. This keeps throughput significantly higher than TCP Tahoe under the same conditions.

One limitation of this simulation is that the event file is manually crafted — in a real network, loss events would be detected dynamically. The UDP forwarding layer shows the routing correctly, but does not implement real TCP reliability (retransmission timers, sequence numbers). These are intentional simplifications for the scope of this assignment.
