#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>

int sock; /* Socket descriptor */

void DieWithError(char *errorMessage)
{
    close(sock);
    perror(errorMessage);
    exit(0);
}

void sigfunc(int sig)
{
    if (sig != SIGINT && sig != SIGTERM)
    {
        return;
    }

    close(sock);
    printf("disconnected\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);

    struct sockaddr_in servAddr; /* Echo server address */
    unsigned short servPort;     /* Echo server port */
    char *servIP;                /* Server IP address  */
    char buffer[150];
    int bytesRcvd; /* Bytes read in single recv() */

    if (argc != 3) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage: %s <Server IP> <Echo Port>\n",
                argv[0]);
        exit(-1);
    }

    servIP = argv[1];         /* First arg: server IP address (dotted quad) */
    servPort = atoi(argv[2]); /* port */

    /* Create a reliable, stream socket using TCP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Construct the server address structure */
    memset(&servAddr, 0, sizeof(servAddr));       /* Zero out structure */
    servAddr.sin_family = AF_INET;                /* Internet address family */
    servAddr.sin_addr.s_addr = inet_addr(servIP); /* Server IP address */
    servAddr.sin_port = htons(servPort);          /* Server port */

    /* Establish the connection to the echo server */
    if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("connect() failed");
    int h = 0;
    if (sendto(sock, &h, sizeof(int), 0, (struct sockaddr *)&servAddr, sizeof(servAddr)) != sizeof(int))
    {
        DieWithError("sendto() sent to the hairdresser a different number of bytes than expected");
    }
    // printf("Observer is ready\n");

    for (;;)
    {
        if ((bytesRcvd = recv(sock, &buffer, 149, 0)) <= 0)
            DieWithError("recv() failed or connection closed prematurely");

        buffer[bytesRcvd] = '\0';
        printf("%s", buffer);
    }
}
