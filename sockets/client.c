/********************************
**      TP SOCKETS TCP/IP      **
**  Hello World UDP (client)   **
********************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr, htons

#define MSG_MAX_SIZE 32
#define MSG "Hello World!"

/*=====================
= Programme principal =
======================*/
int main(int argc, char * argv[]) {

    int sock_id, server_port;
    char server_ip[16]; // address as a string
    struct sockaddr_in server_adr;

    char message[MSG_MAX_SIZE];

    if (argc!=3) {
        printf("Usage: %s adresse_ip port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    strcpy(server_ip, argv[1]);
    server_port = atoi(argv[2]);

    /* Creation d'une socket en mode datagramme  */
    /*-------------------------------------------*/
    sock_id = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sock_id < 0 ) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    /* Initiatlisation de l'adresse du serveur */
    /*-----------------------------------------*/
    memset(&server_adr, 0, sizeof(server_adr));
    server_adr.sin_family = AF_INET;
    server_adr.sin_port = htons(server_port); // htons: host to net byte order (short int)
    server_adr.sin_addr.s_addr = inet_addr(server_ip);

    /* Envoi du message au serveur */
    /*-----------------------------*/
    strcpy(message, MSG);
    if ( (sendto(sock_id, message, strlen(message)+1, 0, (struct sockaddr*) &server_adr, sizeof(server_adr))) < 0) {
        perror("sendto error");
        exit(EXIT_FAILURE);
    }
    printf("--> Message envoyé au serveur.\n");

    /* Réception de la réponse */
    /*-------------------------*/

    /* --> A COMPLETER <-- */

    /* Fermeture de la socket */
    /*------------------------*/
    close(sock_id);

    return 0;
}