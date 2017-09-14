#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>

int
readn(int sfd, char * const data, size_t* dsize)
{
    ssize_t n;
    size_t total = 0;
    size_t bytesleft = *dsize;

    while(total < *dsize)
    {
        n = recv(sfd, data + total, bytesleft, MSG_NOSIGNAL);
        if(-1 == n || 0 == n)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *dsize = total;

    return -1 == n ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    int sockfd, n;
    uint16_t portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = (uint16_t) atoi(argv[2]);

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *) &serv_addr.sin_addr.s_addr, (size_t) server->h_length);
    serv_addr.sin_port = htons(portno);

    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    /* Now ask for a message from the user, this message
       * will be read by server
    */

    printf("Please enter the message: ");
    bzero(buffer, 256);
    fgets(buffer, 255, stdin);

    /* Send message to the server */
    n = write(sockfd, buffer, strlen(buffer));
    shutdown(sockfd, SHUT_WR);

    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }

    /* Now read server response */
    size_t limit = 255;
    if(0 == (n = readn(sockfd, buffer, &limit)))
    {
        buffer[bsize] = '\0';
    }
    shutdown(sockfd, SHUT_RD);
    close(sockfd);

    if (n < 0)
    {
        fprintf(stderr, "Readed %ld bytes, but recv() failed:\n\t ", bsize);
        perror("");
        exit(1);
    }
    
    printf("%s\n", buffer);
    return 0;
}
