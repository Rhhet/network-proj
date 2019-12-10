#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr, htons
#include <time.h>
#include <errno.h>

#include "router.h"
#include "console.h"
#include "packet.h"
#include "test_forwarding.h"

#define BUF_SIZE 1024
#define RTR_BASE_PORT 5555
#define BROADCAST_PERIOD 10
#define FWD_DELAY_IN_MS 10
#define LOG_MSG_MAX_SIZE 256
#define MAX_METRIC 16       // example for RIPv2

#define SPLIT_HRZ       // if define, use the split-horizon method to broadcast the distance vector

#define PORT(x) (x+RTR_BASE_PORT)

static void overlay_addr_from_nt(const neighbors_table_t *nt, node_id_t id,overlay_addr_t *addr);

/* ==================================================================== */
/* ========================= LOG FUNCTIONS ============================ */
/* ==================================================================== */

// Log message to file log/Ri.txt
// Format: DATE [TAG]: MESSAGE
void logger(const char *tag, const char *message, ...) {
    
    time_t now;
    char buf[256], file_name[32];
    va_list params;

    time(&now);
    strncpy(buf, ctime(&now), sizeof(buf));
    buf[strlen(buf)-1]='\0'; // remove new line from ctime function

    sprintf(file_name, "%s%d%s", "log/R", MY_ID, ".txt");
    FILE *f = fopen(file_name, "at");

    fprintf(f, "%s [%s]: ", buf, tag);
    va_start(params, message);
    vfprintf(f, message, params);
    fprintf(f, ".\n");
    va_end(params);
    fclose(f);
}

// Log Distance Vector (DV) included in packet *p
// if output then the DV is sent to neigh, else it is received from neigh
void log_dv(packet_ctrl_t *p, node_id_t neigh, int output) {

    char buf_dv[256];
    char buf_dve[32];
    strcpy(buf_dv, "\t DEST | METRIC \n");
    for (int i=0; i<p->dv_size; i++) {
        sprintf(buf_dve, "\t   %d  |  %d\n", p->dv[i].dest, p->dv[i].metric);
        strcat(buf_dv, buf_dve);
    }
    if (output)
        logger("HELLO TH", "DV sent to R%d :\n %s", neigh, buf_dv);
    else
        logger("SERVER TH", "DV received from R%d :\n %s", neigh, buf_dv);
}

/* ==================================================================== */
/* =============== INIT NEIGHBORS AND ROUTING TABLE =================== */
/* ==================================================================== */

// Init node's overlay address
void init_node(overlay_addr_t *addr, node_id_t id, char *ip) {

    addr->id = id;
    addr->port = PORT(id);
    strcpy(addr->ipv4, ip);
}

// Add node to neighbor's table
void add_neighbor(neighbors_table_t *nt, const overlay_addr_t *node) {

    assert(nt->size < MAX_NEIGHBORS);
    nt->tab[nt->size] = *node;
    nt->size++;
}

// Read topo from conf file
void read_neighbors(char *file, int rid, neighbors_table_t *nt) {

    FILE *fichier = NULL;
    char ligne[80];
    int id = 0;
    overlay_addr_t node;
    char *token;

   	fichier = fopen(file, "rt");
   	if (fichier == NULL) {
   		perror("[Config] Error opening configuration file.\n");
   		exit(EXIT_FAILURE);
   	}

    while (!feof(fichier)) {
        // read line
        fgets(ligne, sizeof(ligne), fichier);
        ligne[strlen(ligne)-1]='\0'; // remove '\n'
        // printf("%s\n", ligne);
        if (ligne[0]!='#') {
            sscanf(ligne, "%d", &id);
            if (id == rid) {
                // read neighbors
                token = strtok(ligne, " ");
                token = strtok(NULL, " "); // discard first number (rid)
                while (token != NULL) {
                    // printf( "|%s|", token );
                    id = atoi(token);
                    init_node(&node, id, "127.0.0.1");
                    add_neighbor(nt, &node);
                    token = strtok(NULL, " ");
                }
                fclose(fichier);
                return ;
            }
        }
    }
    fclose(fichier);
}

// Add route to routing table
void add_route(routing_table_t *rt, node_id_t dest, const overlay_addr_t *next, short metric) {

    assert(rt->size < MAX_ROUTES);
    rt->tab[rt->size].dest    = dest;
    rt->tab[rt->size].nexthop = *next;
    rt->tab[rt->size].metric  = metric;
    rt->tab[rt->size].time    = time(NULL);
    rt->size++;
}

// Init routing table with one entry (myself)
void init_routing_table(routing_table_t *rt) {

    overlay_addr_t me;
    init_node(&me, MY_ID, LOCALHOST);
    add_route(rt, MY_ID, &me, 0);
}


/* ========================================= */
/* ========== FORWARD DATA PACKET ========== */
/* ========================================= */

int forward_packet(packet_data_t *packet, int psize, routing_table_t *rt) {
    node_id_t packet_dst = packet -> dst_id;
    routing_table_entry_t *rt_table = rt -> tab;

    for (int i = 0; i < rt -> size; i++) {
        node_id_t tabledst = rt_table[i].dest;

        if (tabledst == packet_dst) {                            // dest found in table
            int sock_id;
            struct sockaddr_in server_adr;
            unsigned short int port = rt_table[i].nexthop.port;  // port next router
            char *ip = rt_table[i].nexthop.ipv4;                 // ip@ next router
            
            // Create UDP socket (client)
            sock_id = socket(AF_INET, SOCK_DGRAM, 0);
            if ( sock_id < 0 ) {
                perror("socket error");
                exit(EXIT_FAILURE);
            }

            // initialize server address (next router)
            memset(&server_adr, 0, sizeof(server_adr));
            server_adr.sin_family = AF_INET;
            server_adr.sin_port = htons(port); // htons: host to net byte order (short int)
            server_adr.sin_addr.s_addr = inet_addr(ip);

            /* Send packet to the server (next hop/gateway) */
            /*-----------------------------*/
            if ((sendto(sock_id, packet, psize, 0, (struct sockaddr *)&server_adr, sizeof(server_adr))) < 0) {
                perror("sendto error");
                exit(EXIT_FAILURE);
            }
            // printf("--> Packet sent.\n");

            /* Réception de la réponse */
            /*-------------------------*/

            /* --> A COMPLETER <-- */

            // close the socket
            close(sock_id);
            return 1;  
        } 
    }
    return 0;   // cannot find the dest in routing table
}
/* ========================================================================= */
/* *************************** END FORWARD PACKET ************************** */
/* ========================================================================= */


/* ==================================================================== */
/* ========================== HELLO THREAD ============================ */
/* ==================================================================== */

#ifndef SPLIT_HRZ
// Build distance vector packet
void build_dv_packet(packet_ctrl_t *p, routing_table_t *rt) {
    p -> type = CTRL;
    p -> src_id = MY_ID;
    p -> dv_size = rt -> size;

    for (int i = 0; i < rt -> size; i++) {
        p -> dv[i].dest = rt -> tab[i].dest;
        p -> dv[i].metric = rt -> tab[i].metric;
    }
}
#else
// DV to prevent (partially) count to infinity problem
// Build a DV that contains the routes that have not been learned via
// this neighbour
void build_dv_specific(packet_ctrl_t *p, routing_table_t *rt, node_id_t neigh) {
    p -> type = CTRL;
    p -> src_id = MY_ID;
    p -> dv_size = rt -> size;

    int den = 0;    // number of discarded entries
    // the route was learnt from router A if and only if the gateway is A
    for (int i = 0; i < rt -> size; i++) {
        if (rt -> tab[i].nexthop.id != neigh             // this route was not learned from neigh
                && rt -> tab[i].metric <= MAX_METRIC) {  // and its metric is less than MAX_METRIC
            p -> dv[i - den].dest = rt -> tab[i].dest;
            p -> dv[i - den].metric = rt -> tab[i].metric;
        } else {                                         // route learned from neigh => discard it
            den++;                                       // or route metric exceeded MAX_METRIC
        }
    }
    p -> dv_size -= den;
}
#endif

// Remove old RT entries
void remove_obsolete_entries(routing_table_t *rt) {
    // go through the routing table, starting after the first 
    // entry always equal to 'this' router
    int i = 1;
    while (i < rt -> size - 1) {
        int r_lifetime = difftime(time(NULL), rt -> tab[i].time);
        if (r_lifetime > BROADCAST_PERIOD + 5 
                || rt -> tab[i].metric > MAX_METRIC) {
            // this will remove the entry i form the table rt when its lifetime
            // exceeds BROADCAST_PERIOD (+ 5) or if its metric is greater than MAX_METRIC
            memmove(rt + i, rt + i + 1, (rt -> size - i - 1) 
                                        * sizeof(routing_table_entry_t));
        
            rt -> size--;
        } else i++;     // only increment when we don't remove an entry
    }
    // manage the last entry
    if (difftime(time(NULL), rt -> tab[rt -> size - 1].time) > BROADCAST_PERIOD + 5) {
        memset(rt + rt -> size - 1, 0, sizeof(routing_table_entry_t));
        rt -> size--;
    }
}


// Hello thread to broadcast state to neighbors
void *hello(void *args) {

    /* Cast the pointer to the right type */
    struct th_args *pargs = (struct th_args *) args;

    routing_table_t *rt = pargs -> rt;
    neighbors_table_t *nt = pargs -> nt;
    int sock_id;
    struct sockaddr_in server_adr;
    packet_ctrl_t dv_packet;


    // Create client socket (UDP)
    sock_id = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sock_id < 0 ) {
        perror("dv socket error");
        exit(EXIT_FAILURE);
    }
    
    // Periodically send the distance vector to all the neighbors
    while (1) {
#ifndef SPLIT_HRZ
        build_dv_packet(&dv_packet, rt);                // initialize the packet with the dist vect
#endif
        // TODO split horizon => call build_dv_specific
        for (int i = 0; i < nt -> size; i++) {          // go through the neighbors table
#ifdef SPLIT_HRZ
            // build specific dist vector for node i (ignore routes learnt from i)
            build_dv_specific(&dv_packet, rt, nt -> tab[i].id);
#endif
            memset(&server_adr, 0, sizeof(server_adr)); 
            // recover socket address of neighbor:
            server_adr.sin_family = AF_INET;
            server_adr.sin_port = htons(nt -> tab[i].port); 
            server_adr.sin_addr.s_addr = inet_addr(nt -> tab[i].ipv4);

            // Send dv packet to the neighbor
            if ((sendto(sock_id, &dv_packet, sizeof(dv_packet), 0, (struct sockaddr *)&server_adr, sizeof(server_adr))) < 0) {
                perror("send dist vector error");
                logger("ERROR", "sendto %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
            log_dv(&dv_packet, nt -> tab[i].id, 1);     // log results
        }

        // send the vector every BROADCAST_PERIOD secs
        sleep(BROADCAST_PERIOD);
        remove_obsolete_entries(pargs->rt);
    }
    close(sock_id);     // close the socket
}


/* ==================================================================== */
/* ======================== UDP SERVER THREAD ========================= */
/* ==================================================================== */

// Update routing table from received distance vector
int update_rt(routing_table_t *rt, overlay_addr_t *src, dv_entry_t *dv, int dv_size) {
    for (int i = 0; i < dv_size; i++) {
        dv_entry_t dve = dv[i];
        for (int j = 0; j < rt -> size; j++) {
            if (dve.dest == rt -> tab[j].dest) {    // route already in table
                if (rt -> tab[j].metric > dve.metric + 1
                        || rt -> tab[j].nexthop.id == src -> id) {
                    rt -> tab[j].metric     = dve.metric + 1;   // update metric
                    rt -> tab[j].nexthop    = *src;             // update gateway
                    rt -> tab[j].time       = time(NULL);       // refresh route lifetime
                }
                goto dst_found;
            }
        }
        // if the route is not already in the table
        add_route(rt, dve.dest, src, dve.metric + 1);
        dst_found:;
    }
    return 1;
}

// Server thread waiting for input packets
void *process_input_packets(void *args) {

    int sock;
    struct sockaddr_in my_adr, neigh_adr;
    socklen_t adr_len = sizeof(struct sockaddr_in);
    char buffer_in[BUF_SIZE];
    /* Cast the pointer to the right type */
    struct th_args *pargs = (struct th_args *) args;

    // routing_table_t *rt = (routing_table_t *) arg;
    int port = PORT(MY_ID);
    int size = 0;

    /* Create (server) socket */
    /* ---------------------- */
    if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    /* Bind address and port */
    /*-----------------------*/
    /* Init server adr  */
    memset(&my_adr, 0, sizeof(my_adr));
    my_adr.sin_family = AF_INET;
    my_adr.sin_port = htons(port);
    my_adr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &my_adr, sizeof(my_adr)) < 0) {
        perror("bind error");
        close(sock);
        exit(EXIT_FAILURE);
    }

    logger("SERVER TH","waiting for incoming messages");
    while (1) {

        if ((size = recvfrom(sock, buffer_in, BUF_SIZE, 0, (struct sockaddr *)&neigh_adr, &adr_len)) < 0 ) {
            perror("recvfrom error");
            logger("ERROR", "rcvfrom %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        switch (buffer_in[0]) {

            case DATA:
                logger("SERVER TH","DATA packet received");
                packet_data_t *pdata = (packet_data_t *) buffer_in;
                if (pdata->dst_id == MY_ID) {
                    switch (pdata->subtype) {
                        case ECHO_REQUEST:
                            send_ping_reply(pdata, pargs->rt);
                            break;
                        case ECHO_REPLY:
                            print_ping_reply(pdata);
                            break;
                        case TR_REQUEST:
                            send_traceroute_reply(pdata, pargs->rt);
                            break;
                        case TR_TIME_EXCEEDED:
                            print_traceroute_path(pdata);
                            break;
                        case TR_ARRIVED:
                            print_traceroute_last(pdata);
                            break;
                        default:
                            logger("SERVER TH","unidentified data packet received");
                    }
                }
                else {      // this router is not the packet destination => forward packet
                    if (--pdata -> ttl == 0) {      // null ttl
                        send_time_exceeded(pdata, pargs -> rt);
                    } else {                        // non-zero ttl => forward packet
                        forward_packet(pdata, size, pargs -> rt);
                    }
                }
                break;

            case CTRL:
                logger("SERVER TH","CTRL packet received");
                packet_ctrl_t *pctrl = (packet_ctrl_t *) buffer_in;
                log_dv(pctrl, pctrl -> src_id, 0);
                overlay_addr_t src;
                overlay_addr_from_nt(pargs -> nt, pctrl -> src_id, &src);
                /* other way to do it:
                
                src.port = (unsigned short) ntohs(neigh_adr.sin_port);
                strcpy(src.ipv4, inet_ntoa((struct in_addr) {neigh_adr.sin_addr.s_addr}));
                src.id = pctrl -> src_id; */
                
                update_rt(pargs -> rt, &src, pctrl -> dv, pctrl -> dv_size);
                break;

            default:
                // drop
                logger("SERVER TH","unidentified packet received.");
                break;
        }
    }
}

// recover overlay address of a node of id 'id' from a neighbor table
static void overlay_addr_from_nt(const neighbors_table_t *nt, node_id_t id,overlay_addr_t *addr) {
    addr -> id = id;
    for (int i = 0; i < nt -> size; i++) {
        if (nt -> tab[i].id == id) {
            addr -> port = nt -> tab[i].port;
            strcpy(addr -> ipv4, nt -> tab[i].ipv4);
            return;
        }
    }
}


/* ==================================================================== */
/* ========================== MAIN PROGRAM ============================ */
/* ==================================================================== */

void process_command(char *cmd, routing_table_t *rt, neighbors_table_t *nt) {

    pthread_t th_id;

    if (!strcmp(cmd, HELP)) {
        print_help();
        return;
    }
    if (!strcmp(cmd, CLEAR)) {
        clear_screen();
        return;
    }
    if (!strcmp(cmd, SH_IP_ROUTE) || !strcmp(cmd, SH_IP_ROUTE_2)) {
        print_rt(rt);
        return;
    }
    if (!strcmp(cmd, SH_IP_NEIGH) || !strcmp(cmd, SH_IP_NEIGH_2)) {
        print_neighbors(nt);
        return;
    }
    if (!strncmp(cmd, PING, strlen(PING)) && cmd[strlen(PING)]==' ') {
        char temp[16];
        int did;
        sscanf(cmd, "%s%d", temp, &did);
        struct ping_traceroute_args args = {did, rt};
        pthread_create(&th_id, NULL, &ping, &args);
        pthread_join(th_id, NULL);
        return;
    }
    if (!strncmp(cmd, PINGFORCE, strlen(PINGFORCE))) {
        char temp[16];
        int did;
        sscanf(cmd, "%s%d", temp, &did);
        struct ping_traceroute_args args = {did, rt};
        pthread_create(&th_id, NULL, &pingforce, &args);
        pthread_join(th_id, NULL);
        return;
    }
    if (!strncmp(cmd, TRACEROUTE, strlen(TRACEROUTE))) {
        char temp[16];
        int did;
        sscanf(cmd, "%s%d", temp, &did);
        struct ping_traceroute_args args = {did, rt};
        pthread_create(&th_id, NULL, &traceroute, &args);
        pthread_join(th_id, NULL);
        return;
    }
    if (strlen(cmd)!=0)
        print_unknown_command();
}

// 1 router <-> 1 process (via xterm)
int main(int argc, char **argv) {

    routing_table_t myrt;
    neighbors_table_t mynt;
    pthread_t th1_id, th2_id;
    struct th_args args;
    int test_forwarding = 0;

    if (argc!=3) {
        printf("Usage: %s <id> <net_topo_conf>\n", argv[0]);
        printf("or\n");
        printf("Usage: %s <id> --test-forwarding\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // ==== Init ROUTER ====
    myrt.size = 0;
    mynt.size = 0;
    int rid = atoi(argv[1]);
    MY_ID = rid; // shared ID between threads
    printf("**************\n");
    printf("* RTR ID : %d *\n", MY_ID);
    printf("**************\n");

    if (strcmp(argv[2], "--test-forwarding") == 0) {
        init_full_routing_table(&myrt);
        test_forwarding = 1;
    }
    else {
        read_neighbors(argv[2], rid, &mynt);
        init_routing_table(&myrt);
    }
    // ====================
    // print_neighbors(&mynt);
    // print_rt(&myrt);
    args.rt = &myrt;
    args.nt = &mynt;

    /* Create a new thread th1 (process input packets) */
    pthread_create(&th1_id, NULL, &process_input_packets, &args);
    logger("MAIN TH","process input packets thread created with ID %u", (int) th1_id);

    if ( !test_forwarding ) {
        /* Create a new thread th2 (hello broadcast) */
        pthread_create(&th2_id, NULL, &hello, &args);
        logger("MAIN TH","hello thread created with ID %u", (int) th2_id);
    }

    int quit=0, len;
    char *command = NULL;
    size_t size;
    while (!quit) {
        print_prompt();
        len = getline(&command, &size, stdin);
        command[len-1] = '\0'; // remove newline
        quit = !strcmp("quit", command) || !strcmp("exit", command);
        if (!quit)
            process_command(command, &myrt, &mynt);
        free(command);
        command = NULL;
    }

    return EXIT_SUCCESS;
}
