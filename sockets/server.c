/*********************************
**      TP SOCKETS TCP/IP       **
**  Hello World UDP (serveur)   **
*********************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr, htons

#define BUF_SIZE 500
#define SERVER_PORT 5555

/*==============
= MAIN PROGRAM =
===============*/
int main(void)
{
    int sock_id;
    struct sockaddr_in server_adr;
    struct sockaddr_in client_adr;
    socklen_t client_adr_len = sizeof(client_adr);

    char buffer[BUF_SIZE];

    /* Creation d'une socket en mode datagramme  */
    /*-------------------------------------------*/
    sock_id = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sock_id < 0 ) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    /* Association adresse et port à la socket (bind)  */
    /*-------------------------------------------------*/

    /* Initialisation adresse locale du serveur  */
    memset(&server_adr, 0, sizeof(server_adr));
    server_adr.sin_family = AF_INET;
    server_adr.sin_port = htons(SERVER_PORT); // htons: host to net byte order (short int)
    server_adr.sin_addr.s_addr = htonl(INADDR_ANY); // wildcard address

    if (bind(sock_id, (struct sockaddr *) &server_adr, sizeof(server_adr)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    /* Réception des messages (attente active)  */
    /*------------------------------------------*/
    while (1) {

        printf("\nServeur en attente...\n");
        client_adr_len = sizeof(client_adr);
        if (recvfrom(sock_id, buffer, BUF_SIZE, 0, (struct sockaddr *)&client_adr, &client_adr_len) < 0) {
            perror("recvfrom error");
            exit(EXIT_FAILURE);
        }

        printf("--> Message reçu : %s\n", buffer);

        /* Envoi de la réponse  */
        /*----------------------*/
        /* --> A COMPLETER <-- */

    } /* while */

    close(sock_id);

    return 0;
}