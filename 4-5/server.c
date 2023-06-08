#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>

int servClntSock;
int servHrdrSock;
struct sockaddr_in hrdrAddr; /* Hairdresser address */

void DieWithError(char *errorMessage)
{
    close(servClntSock);
    close(servHrdrSock);
    perror(errorMessage);
    exit(0);
}

int createSocket(int port, in_addr_t servInAddr)
{
    int servSock;
    struct sockaddr_in servAddr;

    /* Create socket for incoming connections */
    if ((servSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr)); /* Zero out structure */
    servAddr.sin_family = AF_INET;          /* Internet address family */
    servAddr.sin_addr.s_addr = servInAddr;  /* Any incoming interface */
    servAddr.sin_port = htons(port);        /* Local port */

    /* Bind to the local address */
    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
        DieWithError("bind() failed");

    return servSock;
}

void HandleTCPClient()
{
    unsigned int clntLen;            /* Length of client address data structure */
    struct sockaddr_in echoClntAddr; /* Client address */
    /* Set the size of the in-out parameter */
    clntLen = sizeof(echoClntAddr);

    pid_t pid;       /* Client's id */
    int recvMsgSize; /* Size of received message */
    if ((recvMsgSize = recvfrom(servClntSock, &pid, sizeof(int), 0, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
    {
        DieWithError("recvfrom() from client failed");
    }
    printf("Handling %s\n", inet_ntoa(echoClntAddr.sin_addr));

    printf("Client %d is in the queue\n", pid);

    /* Send client to hairdresser */
    if (sendto(servHrdrSock, &pid, sizeof(int), 0, (struct sockaddr *)&hrdrAddr, sizeof(hrdrAddr)) != sizeof(int))
    {
        DieWithError("sendto() sent to the hairdresser a different number of bytes than expected");
    }
    printf("Client %d leaves the queue for a haircut\n", pid);

    /* Receive notification about the end of the haircut */
    if ((recvMsgSize = recvfrom(servHrdrSock, &pid, sizeof(int), 0, (struct sockaddr *)NULL, 0)) < 0)
    {
        DieWithError("recvfrom() from hairdresser failed");
    }

    /* Release client */
    if (sendto(servClntSock, &pid, sizeof(int), 0, (struct sockaddr *)&echoClntAddr, sizeof(echoClntAddr)) != sizeof(int))
    {
        DieWithError("sendto() sent to the client a different number of bytes than expected");
    }
}

void sigfunc(int sig)
{
    if (sig != SIGINT && sig != SIGTERM)
    {
        return;
    }

    close(servClntSock);
    close(servHrdrSock);
    printf("disconnected\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);

    int clntSock; /* Socket descriptor for client */
    int hrdrSock; /* Socket descriptor for hairdresser */
    unsigned int servClntPort;
    unsigned int servHrdrPort;

    if (argc != 4) /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <Server Address> <Port for Clients> <Port for Haidresser>\n", argv[0]);
        exit(1);
    }

    in_addr_t servAddr;
    if ((servAddr = inet_addr(argv[1])) < 0)
    {
        DieWithError("Invalid address");
    }

    servClntPort = atoi(argv[2]);
    servHrdrPort = atoi(argv[3]);

    servClntSock = createSocket(servClntPort, servAddr);
    servHrdrSock = createSocket(servHrdrPort, servAddr);

    /* Block until receive message from the hairdresser */
    int recvMsgSize;
    int h;
    unsigned int clntLen;
    clntLen = sizeof(hrdrAddr);
    if ((recvMsgSize = recvfrom(servHrdrSock, &h, sizeof(int), 0, (struct sockaddr *)&hrdrAddr, &clntLen)) < 0)
    {
        DieWithError("recvfrom() failed");
    }
    else
    {
        printf("The hairdresser's is open\n");
    }

    for (;;)
    {
        HandleTCPClient();
    }
}
