#ifndef NODE_H
#define NODE_H

#define MAX_NEIGHBORS 16
#define MAX_LINE      256
#define MAX_NODES     16
#define INF           99999

/* ── Congestion-control enums ── */

typedef enum {
    ALG_TAHOE = 0,
    ALG_RENO,
    ALG_NEWRENO,
    ALG_UNKNOWN
} Algorithm;

typedef enum {
    SLOW_START = 0,
    CONGESTION_AVOIDANCE,
    FAST_RECOVERY
} CCState;

/* ── Network topology structs ── */

typedef struct {
    char id;
    char ip[64];
    int  port;
    int  cost;
} Neighbor;

typedef struct {
    char     node_id;
    int      port;
    Neighbor neighbors[MAX_NEIGHBORS];
    int      neighbor_count;
} NodeConfig;

/* --- ADDED: Routing table structs for Dijkstra START --- */
typedef struct {
    char dest;
    char next_hop;
    int  cost;
    char path[64];
} RouteEntry;

typedef struct {
    RouteEntry entries[MAX_NODES];
    int        count;
} RoutingTable;
/* --- ADDED: Routing table structs for Dijkstra END --- */

/* ── TCP congestion-control state ── */

typedef struct {
    Algorithm algorithm;
    CCState   state;
    double    cwnd;
    double    ssthresh;
    int       dup_ack_count;
    int       round;
} TCPState;

/* ── Function declarations ── */

int         load_config(const char *filename, NodeConfig *config);

Algorithm   parse_algorithm(const char *name);
const char *algorithm_name(Algorithm alg);
const char *state_name(CCState state);

void init_tcp(TCPState *tcp, Algorithm alg);
void on_ack(TCPState *tcp, int ack_no);
void on_duplicate_ack(TCPState *tcp, int ack_no);
void on_timeout(TCPState *tcp);

/* --- ADDED: Routing function declarations START --- */
void        build_routing_table(NodeConfig *config, RoutingTable *rt);
void        print_routing_table(NodeConfig *config, RoutingTable *rt);
RouteEntry *find_route(RoutingTable *rt, char dest);
/* --- ADDED: Routing function declarations END --- */

int  start_listener(int port);
void run_node(NodeConfig *config, RoutingTable *rt, TCPState *tcp, int sock);

#endif
