/*
 * CSE 320 — TCP Congestion Control: TCP Reno Implementation
 * node.c
 *
 * Compile (Linux/macOS):   gcc node.c -o node
 * Compile (Windows MinGW): gcc node.c -o node.exe
 *
 * Run (event file mode):
 *   ./node A.conf reno events/example_events.txt
 */

/* --- ADDED: Network / Socket headers START --- */
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  #define CLOSESOCKET closesocket
  #define SOCKET_T SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #define CLOSESOCKET close
  #define SOCKET_T int
  #define INVALID_SOCKET (-1)
#endif
/* --- ADDED: Network / Socket headers END --- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "node.h"

/* ════════════════════════════════════════════════════
   PART 1 — Algorithm helpers  [ORIGINAL - NO CHANGE]
   ════════════════════════════════════════════════════ */

Algorithm parse_algorithm(const char *name) {
    if (strcmp(name, "tahoe")   == 0) return ALG_TAHOE;
    if (strcmp(name, "reno")    == 0) return ALG_RENO;
    if (strcmp(name, "newreno") == 0) return ALG_NEWRENO;
    return ALG_UNKNOWN;
}

const char *algorithm_name(Algorithm alg) {
    switch (alg) {
        case ALG_TAHOE:   return "TCP Tahoe";
        case ALG_RENO:    return "TCP Reno";
        case ALG_NEWRENO: return "TCP NewReno";
        default:          return "Unknown";
    }
}

const char *state_name(CCState state) {
    switch (state) {
        case SLOW_START:           return "Slow Start";
        case CONGESTION_AVOIDANCE: return "Congestion Avoidance";
        case FAST_RECOVERY:        return "Fast Recovery";
        default:                   return "Unknown";
    }
}

/* ════════════════════════════════════════════════════
   PART 2 — Config loader  [ORIGINAL - NO CHANGE]
   ════════════════════════════════════════════════════ */

int load_config(const char *filename, NodeConfig *config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char line[MAX_LINE];
    config->neighbor_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, " %c %d", &config->node_id, &config->port) == 2) break;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (config->neighbor_count >= MAX_NEIGHBORS) {
            printf("Too many neighbors.\n");
            fclose(fp);
            return 0;
        }
        Neighbor *n = &config->neighbors[config->neighbor_count];
        if (sscanf(line, " %c %63s %d %d", &n->id, n->ip, &n->port, &n->cost) == 4)
            config->neighbor_count++;
    }

    fclose(fp);
    return 1;
}

static void print_config(NodeConfig *config) {
    int i;
    printf("Node %c listening on port %d\n", config->node_id, config->port);
    printf("Neighbors:\n");
    for (i = 0; i < config->neighbor_count; i++)
        printf("  %c  %s  port=%d  cost=%d\n",
               config->neighbors[i].id,
               config->neighbors[i].ip,
               config->neighbors[i].port,
               config->neighbors[i].cost);
}

/* ════════════════════════════════════════════════════
   PART 3 — Dijkstra Routing Table
   [--- ADDED: This entire section is new ---]
   ════════════════════════════════════════════════════ */

/*
 * Full topology (hard-coded from assignment PDF):
 *   A-B=4, A-C=7, A-D=13, A-F=5
 *   B-D=8, B-E=3
 *   C-D=9, C-E=12
 *
 * Node index mapping: A=0, B=1, C=2, D=3, E=4, F=5
 */

/* --- ADDED: Helper index converters START --- */
static int node_index(char id) {
    if (id >= 'A' && id <= 'F') return id - 'A';
    return -1;
}

static char index_node(int idx) {
    return (char)('A' + idx);
}
/* --- ADDED: Helper index converters END --- */

/* --- ADDED: build_routing_table (Dijkstra) START --- */
void build_routing_table(NodeConfig *config, RoutingTable *rt) {
    int i, j, u, v, alt;

    /* Full 6-node cost matrix  (A=0 B=1 C=2 D=3 E=4 F=5) */
    int cm[6][6];
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            cm[i][j] = INF;

    /* Hard-coded topology from assignment */
    cm[0][1]=4;  cm[1][0]=4;   /* A-B */
    cm[0][2]=7;  cm[2][0]=7;   /* A-C */
    cm[0][3]=13; cm[3][0]=13;  /* A-D */
    cm[0][5]=5;  cm[5][0]=5;   /* A-F */
    cm[1][3]=8;  cm[3][1]=8;   /* B-D */
    cm[1][4]=3;  cm[4][1]=3;   /* B-E */
    cm[2][3]=9;  cm[3][2]=9;   /* C-D */
    cm[2][4]=12; cm[4][2]=12;  /* C-E */

    /* Override with actual .conf values (keeps flexibility) */
    for (i = 0; i < config->neighbor_count; i++) {
        int a = node_index(config->node_id);
        int b = node_index(config->neighbors[i].id);
        if (a >= 0 && a < 6 && b >= 0 && b < 6) {
            cm[a][b] = config->neighbors[i].cost;
            cm[b][a] = config->neighbors[i].cost;
        }
    }

    /* Dijkstra */
    int dist[6], prev[6], vis[6];
    for (i = 0; i < 6; i++) { dist[i]=INF; prev[i]=-1; vis[i]=0; }

    int src = node_index(config->node_id);
    dist[src] = 0;

    for (i = 0; i < 6; i++) {
        u = -1;
        for (j = 0; j < 6; j++)
            if (!vis[j] && (u == -1 || dist[j] < dist[u])) u = j;
        if (dist[u] == INF) break;
        vis[u] = 1;

        for (v = 0; v < 6; v++) {
            if (cm[u][v] == INF) continue;
            alt = dist[u] + cm[u][v];
            if (alt < dist[v]) { dist[v] = alt; prev[v] = u; }
        }
    }

    /* Build routing table entries */
    rt->count = 0;
    for (i = 0; i < 6; i++) {
        RouteEntry *e = &rt->entries[rt->count];
        e->dest = index_node(i);

        if (i == src) {
            e->next_hop = config->node_id;
            e->cost     = 0;
            snprintf(e->path, sizeof(e->path), "%c", config->node_id);
            rt->count++;
            continue;
        }
        if (dist[i] == INF) continue;

        e->cost = dist[i];

        /* Trace back to find next hop and build path string */
        int path_buf[6];
        int path_len = 0;
        int cur = i;
        while (cur != -1) { path_buf[path_len++] = cur; cur = prev[cur]; }

        /* next_hop = second node in path (first after src) */
        e->next_hop = (path_len >= 2) ? index_node(path_buf[path_len - 2])
                                       : e->dest;

        /* Build "A -> B -> D" string */
        e->path[0] = '\0';
        for (j = path_len - 1; j >= 0; j--) {
            char tmp[8];
            if (j == path_len - 1)
                snprintf(tmp, sizeof(tmp), "%c", index_node(path_buf[j]));
            else
                snprintf(tmp, sizeof(tmp), " -> %c", index_node(path_buf[j]));
            strncat(e->path, tmp, sizeof(e->path) - strlen(e->path) - 1);
        }

        rt->count++;
    }
}
/* --- ADDED: build_routing_table (Dijkstra) END --- */

/* --- ADDED: print_routing_table START --- */
void print_routing_table(NodeConfig *config, RoutingTable *rt) {
    int i;
    printf("\nRouting table for node %c\n", config->node_id);
    printf("%-12s %-10s %-6s %s\n", "Destination", "Next Hop", "Cost", "Path");
    printf("--------------------------------------------------\n");
    for (i = 0; i < rt->count; i++) {
        RouteEntry *e = &rt->entries[i];
        if (e->cost == 0)
            printf("%-12c %-10c %-6d %s\n", e->dest, '-', 0, e->path);
        else
            printf("%-12c %-10c %-6d %s\n", e->dest, e->next_hop, e->cost, e->path);
    }
}
/* --- ADDED: print_routing_table END --- */

/* --- ADDED: find_route START --- */
RouteEntry *find_route(RoutingTable *rt, char dest) {
    int i;
    for (i = 0; i < rt->count; i++)
        if (rt->entries[i].dest == dest) return &rt->entries[i];
    return NULL;
}
/* --- ADDED: find_route END --- */

/* ════════════════════════════════════════════════════
   PART 4 — TCP Reno congestion control
   ════════════════════════════════════════════════════ */

void init_tcp(TCPState *tcp, Algorithm alg) {
    tcp->algorithm     = alg;
    tcp->state         = SLOW_START;
    tcp->cwnd          = 1.0;
    tcp->ssthresh      = 16.0;
    tcp->dup_ack_count = 0;
    tcp->round         = 0;
}

static void print_tcp_status(TCPState *tcp, const char *event,
                              int ack_no, const char *note) {
    printf("%5d | %-10s | ACK=%-3d | cwnd=%6.2f | ssthresh=%6.2f | %-22s | %s\n",
           tcp->round, event, ack_no,
           tcp->cwnd, tcp->ssthresh,
           state_name(tcp->state), note);
}

/* [ORIGINAL on_ack - NO CHANGE NEEDED, already correct] */
void on_ack(TCPState *tcp, int ack_no) {
    tcp->round++;
    tcp->dup_ack_count = 0;

    if (tcp->state == SLOW_START) {
        tcp->cwnd += 1.0;
        if (tcp->cwnd >= tcp->ssthresh)
            tcp->state = CONGESTION_AVOIDANCE;
        print_tcp_status(tcp, "ACK", ack_no, "new ACK received; cwnd increased");
    }
    else if (tcp->state == CONGESTION_AVOIDANCE) {
        tcp->cwnd += 1.0 / tcp->cwnd;
        print_tcp_status(tcp, "ACK", ack_no, "new ACK received; additive increase");
    }
    else if (tcp->state == FAST_RECOVERY) {
        /* Reno: full ACK → exit Fast Recovery, set cwnd = ssthresh */
        tcp->cwnd  = tcp->ssthresh;
        tcp->state = CONGESTION_AVOIDANCE;
        print_tcp_status(tcp, "ACK", ack_no, "full ACK; exit Fast Recovery");
    }
}

void on_duplicate_ack(TCPState *tcp, int ack_no) {
    tcp->round++;
    tcp->dup_ack_count++;

    /* 1st and 2nd duplicate ACK — just wait */
    if (tcp->dup_ack_count < 3) {
        print_tcp_status(tcp, "DUPACK", ack_no, "duplicate ACK received; waiting");
        return;
    }

    /* --- ADDED: Window Inflation for 4th, 5th, 6th... dup-ACK START ---
     *
     * If we are ALREADY in Fast Recovery (dup_ack_count > 3),
     * each extra dup-ACK means one more packet left the network.
     * Reno inflates cwnd by 1 MSS for each such ACK.
     * We must return early so ssthresh is NOT recalculated again.
     */
    if (tcp->state == FAST_RECOVERY) {
        tcp->cwnd += 1.0;
        print_tcp_status(tcp, "DUPACK", ack_no,
                         "Window Inflation; cwnd+=1 (still in Fast Recovery)");
        return;
    }
    /* --- ADDED: Window Inflation END --- */

    /*
     * Exactly the 3rd duplicate ACK — packet loss detected for the first time.
     * ssthresh is computed ONCE here and never again during this loss episode.
     *
     * [FIXED: ssthresh was previously recalculated on every dup-ACK >= 3,
     *  which caused it to shrink wrongly on the 4th, 5th ACK too.]
     */
    tcp->ssthresh = tcp->cwnd / 2.0;
    if (tcp->ssthresh < 2.0) tcp->ssthresh = 2.0;

    if (tcp->algorithm == ALG_TAHOE) {
        /* Tahoe: reset cwnd to 1, go back to Slow Start */
        tcp->cwnd  = 1.0;
        tcp->state = SLOW_START;
        print_tcp_status(tcp, "DUPACK", ack_no,
                         "3 dup ACKs; Tahoe resets cwnd to 1");
    }
    else if (tcp->algorithm == ALG_RENO) {
        /* Reno: Fast Retransmit + enter Fast Recovery */
        /* cwnd = ssthresh + 3 (the 3 packets that triggered dup-ACKs) */
        tcp->cwnd  = tcp->ssthresh + 3.0;
        tcp->state = FAST_RECOVERY;
        print_tcp_status(tcp, "DUPACK", ack_no,
                         "3 dup ACKs; Reno enters Fast Recovery");
    }
    else if (tcp->algorithm == ALG_NEWRENO) {
        tcp->cwnd  = tcp->ssthresh + 3.0;
        tcp->state = FAST_RECOVERY;
        print_tcp_status(tcp, "DUPACK", ack_no,
                         "3 dup ACKs; NewReno Fast Recovery");
    }
}

/* [ORIGINAL on_timeout - NO CHANGE] */
void on_timeout(TCPState *tcp) {
    tcp->round++;
    tcp->ssthresh = tcp->cwnd / 2.0;
    if (tcp->ssthresh < 2.0) tcp->ssthresh = 2.0;
    tcp->cwnd          = 1.0;
    tcp->dup_ack_count = 0;
    tcp->state         = SLOW_START;
    print_tcp_status(tcp, "TIMEOUT", 0, "timeout; cwnd reset to 1; back to Slow Start");
}

/* ════════════════════════════════════════════════════
   PART 5 — Event file runner  [ORIGINAL + minor label]
   ════════════════════════════════════════════════════ */

static int run_event_file(const char *event_file, TCPState *tcp) {
    FILE *fp = fopen(event_file, "r");
    if (!fp) {
        printf("Could not open event file: %s\n", event_file);
        return 0;
    }

    char event[32];
    int  ack_no;

    printf("\nCongestion-control trace  [%s]\n", event_file);
    printf("Round | Event      | ACK    | cwnd         | ssthresh     | State                  | Explanation\n");
    printf("------------------------------------------------------------------------------------------------------\n");

    while (fscanf(fp, "%31s", event) == 1) {
        if (strcmp(event, "ACK") == 0) {
            if (fscanf(fp, "%d", &ack_no) != 1) {
                printf("Bad ACK line.\n"); fclose(fp); return 0;
            }
            on_ack(tcp, ack_no);
        }
        else if (strcmp(event, "DUPACK") == 0) {
            if (fscanf(fp, "%d", &ack_no) != 1) {
                printf("Bad DUPACK line.\n"); fclose(fp); return 0;
            }
            on_duplicate_ack(tcp, ack_no);
        }
        else if (strcmp(event, "TIMEOUT") == 0) {
            on_timeout(tcp);
        }
        else {
            printf("Unknown event: %s\n", event);
            fclose(fp); return 0;
        }
    }

    fclose(fp);
    return 1;
}

/* ════════════════════════════════════════════════════
   PART 6 — UDP socket helpers
   [--- ADDED: UDP Socket Functions START ---]
   ════════════════════════════════════════════════════ */

int start_listener(int port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    SOCKET_T sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Error: could not create socket.\n");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Error: could not bind to port %d.\n", port);
        CLOSESOCKET(sock);
        return -1;
    }

    /* Non-blocking so we can also read stdin */
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    return (int)sock;
}

static void udp_send(const char *ip, int port, const char *msg) {
#ifdef _WIN32
    SOCKET_T s = socket(AF_INET, SOCK_DGRAM, 0);
#else
    int s = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons((uint16_t)port);
    inet_pton(AF_INET, ip, &dst.sin_addr);
    sendto(s, msg, (int)strlen(msg), 0, (struct sockaddr *)&dst, sizeof(dst));
    CLOSESOCKET(s);
}

static Neighbor *find_neighbor(NodeConfig *config, char id) {
    int i;
    for (i = 0; i < config->neighbor_count; i++)
        if (config->neighbors[i].id == id) return &config->neighbors[i];
    return NULL;
}

/* ════════════════════════════════════════════════════
   PART 7 — Interactive node loop
   [--- ADDED: Interactive Console & Network Loop START ---]
   ════════════════════════════════════════════════════ */

void run_node(NodeConfig *config, RoutingTable *rt, TCPState *tcp, int sock) {
    char buf[1024];
    char input[1024];
    struct sockaddr_in sender;
    socklen_t slen = sizeof(sender);

    printf("\nNode %c is running. Type commands:\n", config->node_id);
    printf("  send <dest> <message>   - send a message\n");
    printf("  acksim <n>              - simulate n ACKs\n");
    printf("  dupack <n> <count>      - simulate duplicate ACKs\n");
    printf("  timeout                 - simulate a timeout\n");
    printf("  route                   - show routing table\n");
    printf("  quit                    - exit\n\n");

    while (1) {
        /* Check incoming UDP messages */
        int n = (int)recvfrom(sock, buf, sizeof(buf) - 1, 0,
                              (struct sockaddr *)&sender, &slen);
        if (n > 0) {
            buf[n] = '\0';
            char src_id, dest_id;
            char payload[900];
            if (sscanf(buf, "MSG %c %c %899s", &src_id, &dest_id, payload) == 3) {
                if (dest_id == config->node_id) {
                    printf("[%c] Received message from %c: %s\n",
                           config->node_id, src_id, payload);
                    /* Simulate ACK back */
                    tcp->round++;
                    tcp->dup_ack_count = 0;
                    on_ack(tcp, tcp->round);
                } else {
                    RouteEntry *route = find_route(rt, dest_id);
                    if (!route) {
                        printf("[%c] No route to %c - dropping.\n", config->node_id, dest_id);
                    } else {
                        Neighbor *nb = find_neighbor(config, route->next_hop);
                        if (nb) {
                            printf("[%c] Forwarding message from %c to %c, next hop %c\n",
                                   config->node_id, src_id, dest_id, route->next_hop);
                            udp_send(nb->ip, nb->port, buf);
                        }
                    }
                }
            }
        }

        /* Check stdin for user commands */
#ifdef _WIN32
        /* --- FIXED: WaitForSingleObject works in both cmd and Git Bash --- */
        {
            HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
            DWORD wait_result = WaitForSingleObject(h, 30);
            if (wait_result != WAIT_OBJECT_0) continue;
            if (!fgets(input, sizeof(input), stdin)) continue;
        }
#else
        {
            struct timeval tv = {0, 50000};
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(0, &rfds);
            if (select(1, &rfds, NULL, NULL, &tv) <= 0) continue;
            if (!fgets(input, sizeof(input), stdin)) continue;
        }
#endif

        input[strcspn(input, "\r\n")] = '\0';
        if (strlen(input) == 0) continue;

        if (strcmp(input, "quit") == 0) break;

        if (strcmp(input, "route") == 0) {
            print_routing_table(config, rt);
            continue;
        }

        if (strcmp(input, "timeout") == 0) {
            printf("\nSimulating TIMEOUT:\n");
            on_timeout(tcp);
            continue;
        }

        int ack_n;
        if (sscanf(input, "acksim %d", &ack_n) == 1) {
            printf("\nSimulating %d ACKs:\n", ack_n);
            for (int i = 1; i <= ack_n; i++) on_ack(tcp, i);
            continue;
        }

        int dp_ack, dp_count;
        if (sscanf(input, "dupack %d %d", &dp_ack, &dp_count) == 2) {
            printf("\nSimulating %d duplicate ACKs for ACK=%d:\n", dp_count, dp_ack);
            for (int i = 0; i < dp_count; i++) on_duplicate_ack(tcp, dp_ack);
            continue;
        }

        char dest_id;
        char msg_payload[900];
        if (sscanf(input, "send %c %899s", &dest_id, msg_payload) == 2) {
            if (dest_id >= 'a' && dest_id <= 'z') dest_id -= 32;
            if (dest_id == config->node_id) {
                printf("[%c] Cannot send to yourself.\n", config->node_id);
                continue;
            }
            RouteEntry *route = find_route(rt, dest_id);
            if (!route) {
                printf("[%c] No route to %c.\n", config->node_id, dest_id);
                continue;
            }
            Neighbor *nb = find_neighbor(config, route->next_hop);
            if (!nb) {
                printf("[%c] Next hop %c not a direct neighbor.\n", config->node_id, route->next_hop);
                continue;
            }
            printf("[%c] Destination %c, next hop %c\n", config->node_id, dest_id, route->next_hop);
            char full_msg[1024];
            snprintf(full_msg, sizeof(full_msg), "MSG %c %c %s", config->node_id, dest_id, msg_payload);
            udp_send(nb->ip, nb->port, full_msg);
            on_ack(tcp, tcp->round + 1);
            continue;
        }
        printf("Commands: send <dest> <msg> | acksim <n> | dupack <ack> <count> | timeout | route | quit\n");
    }
    CLOSESOCKET(sock);
}
/* --- ADDED: Interactive Console & Network Loop / UDP END --- */

/* ════════════════════════════════════════════════════
   PART 8 — main
   ════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    NodeConfig   config;
    TCPState     tcp;
    RoutingTable rt;

    if (argc < 3) {
        printf("Usage:\n");
        printf("  Simulation:  ./node <config_file> <algorithm> <event_file>\n");
        printf("  Interactive: ./node <config_file> <algorithm>\n");
        printf("Algorithms: tahoe  reno  newreno\n");
        return 1;
    }

    Algorithm selected = parse_algorithm(argv[2]);
    if (selected == ALG_UNKNOWN) {
        printf("Invalid algorithm. Use: tahoe, reno, or newreno\n");
        return 1;
    }

    if (!load_config(argv[1], &config)) {
        printf("Could not load config file: %s\n", argv[1]);
        return 1;
    }

    print_config(&config);
    build_routing_table(&config, &rt);
    print_routing_table(&config, &rt);

    init_tcp(&tcp, selected);
    printf("\nSelected congestion control algorithm: %s\n", algorithm_name(selected));
    printf("Initial cwnd=%.2f  ssthresh=%.2f\n", tcp.cwnd, tcp.ssthresh);

    /* --- ADDED: Main handling for both Simulation and Interactive mode START --- */
    if (argc == 4) {
        /* Run simulation mode using event file */
        if (!run_event_file(argv[3], &tcp)) return 1;
        return 0;
    }

    /* Run interactive network mode */
    int sock = start_listener(config.port);
    if (sock < 0) return 1;

    run_node(&config, &rt, &tcp, sock);
    /* --- ADDED: Main handling for both Simulation and Interactive mode END --- */
    return 0;
}
