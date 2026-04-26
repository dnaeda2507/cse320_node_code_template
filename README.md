# CSE 320 - Computer Networks Programming Assignment
## TCP Congestion Control Algorithms Comparison

**Assigned Algorithm:** TCP Reno
**Name:** [Adını Soyadını Buraya Yaz]
**Student ID:** [Öğrenci Numaranı Buraya Yaz]

---

### 1. Overview
This project simulates the **TCP Reno** congestion control algorithm. It includes a complete implementation of Slow Start, Congestion Avoidance, Fast Retransmit, and Fast Recovery (including Window Inflation). 

Additionally, a 6-node network topology is implemented using UDP sockets. The routing table is dynamically calculated using **Dijkstra's Shortest Path Algorithm**, ensuring messages are routed optimally rather than just directly (e.g., A to D routes through B).

---

### 2. How to Compile
A C compiler (like GCC/MinGW) is required. To compile the code on Windows (Git Bash or MinGW), use the following command:

```bash
gcc node.c -o node.exe -lws2_32 -Wall
```
*(Note: `-lws2_32` is required to link the Windows socket library for the UDP routing features).*

---

### 3. Execution Mode 1: Event Simulation (TCP Reno Behavior)
This mode tests how the congestion window (cwnd) reacts to ACKs, Duplicate ACKs, and Timeouts.

**Timeout Scenario:**
```bash
./node.exe A.conf reno events/timeout_test.txt
```
*Expected Behavior:* When a TIMEOUT occurs, TCP Reno sets `ssthresh` to `cwnd / 2` and completely resets `cwnd` to 1.0 (returning to Slow Start).

**Triple-Duplicate ACK Scenario (Fast Retransmit & Fast Recovery):**
```bash
./node.exe A.conf reno events/duplicate_ack_test.txt
```
*Expected Behavior:* When 3 Duplicate ACKs are received, Reno enters **Fast Recovery**. `ssthresh` becomes `cwnd / 2`, and `cwnd` is set to `ssthresh + 3`. It does *not* drop to 1.0 like Tahoe does.

**Full TCP Reno Demo (Includes Window Inflation):**
```bash
./node.exe A.conf reno events/reno_full_demo.txt
```
*Expected Behavior:* Shows full congestion avoidance. On the 4th and 5th duplicate ACKs, the `cwnd` inflates by 1 for each (Window Inflation) to reflect packets leaving the network. Upon receiving a full ACK, `cwnd` is restored back to `ssthresh`.

---

### 4. Execution Mode 2: Interactive 6-Node Topology (UDP Routing)
This mode demonstrates the Dijkstra routing and actual UDP packet forwarding between 6 nodes (A, B, C, D, E, F).

**Step 1:** Open 6 separate terminals (e.g., Git Bash tabs) and run each node:
```bash
Terminal 1: ./node.exe A.conf reno
Terminal 2: ./node.exe B.conf reno
Terminal 3: ./node.exe C.conf reno
Terminal 4: ./node.exe D.conf reno
Terminal 5: ./node.exe E.conf reno
Terminal 6: ./node.exe F.conf reno
```

**Step 2:** Send a message from Node A to Node D.
In Terminal A, type:
```
send D hello_from_A_to_D
```
*Routing Result:* Because A->D direct cost is 13, and A->B->D cost is 12, Dijkstra forces the message through B. 
- Terminal A logs: `Destination D, next hop B`
- Terminal B logs: `Forwarding message from A to D, next hop D`
- Terminal D logs: `Received message from A...`

**Step 3:** Send a message from Node F to Node E.
In Terminal F, type:
```
send E hello_from_F_to_E
```
*Routing Result:* The shortest path is F -> A -> B -> E.
- Terminal F forwards to A.
- Terminal A forwards to B.
- Terminal B forwards to E.
- Terminal E receives the message.

---

### 5. Algorithm Comparison (Tahoe vs. Reno)
Although this assignment focuses on **TCP Reno**, the code includes TCP Tahoe logic for comparison purposes to explain the differences in the demonstration video:

- **Packet Loss via Timeout:** Both Tahoe and Reno behave identically. They both set `ssthresh = cwnd / 2`, drop `cwnd = 1`, and restart in Slow Start phase.
- **Packet Loss via 3 Duplicate ACKs:** 
  - **TCP Tahoe** treats this exactly like a timeout. It drops `cwnd` all the way down to 1. This causes a massive drop in network throughput.
  - **TCP Reno** uses *Fast Recovery*. It realizes the network isn't fully congested (since duplicate ACKs mean some packets are still arriving). It cuts `cwnd` in half instead of dropping it to 1, maintaining much higher data throughput.
