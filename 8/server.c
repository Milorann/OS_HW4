#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

pthread_mutex_t mutex; /* For correct info messaging */

int servClntSock;
int servHrdrSock;
int servObsrvSock;
struct sockaddr_in hrdrAddr; /* Hairdresser address */

int info_pipe[2];

struct Observer
{
    struct sockaddr_in addr;
    int is_active;
};

struct Observer observers[15];

void DieWithError(char *errorMessage)
{
    close(servClntSock);
    close(servHrdrSock);
    close(servObsrvSock);
    pthread_mutex_destroy(&mutex);
    close(info_pipe[0]);
    close(info_pipe[1]);
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

void HandleUDPClient()
{
    unsigned int clntLen;            /* Length of client address data structure */
    struct sockaddr_in echoClntAddr; /* Client address */
    /* Set the size of the in-out parameter */
    clntLen = sizeof(echoClntAddr);

    pid_t pid;       /* Client's id */
    int recvMsgSize; /* Size of received message */
    char str[150];
    if ((recvMsgSize = recvfrom(servClntSock, &pid, sizeof(int), 0, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
    {
        DieWithError("recvfrom() from client failed");
    }
    printf("Handling %s\n", inet_ntoa(echoClntAddr.sin_addr));

    sprintf(str, "Client %d is in the queue\n", pid);
    write(info_pipe[1], str, strlen(str));

    /* Send client to hairdresser */
    if (sendto(servHrdrSock, &pid, sizeof(int), 0, (struct sockaddr *)&hrdrAddr, sizeof(hrdrAddr)) != sizeof(int))
    {
        DieWithError("sendto() sent to the hairdresser a different number of bytes than expected");
    }
    sprintf(str, "Client %d leaves the queue for a haircut\n", pid);
    write(info_pipe[1], str, strlen(str));

    /* Receive notification about the end of the haircut */
    if ((recvMsgSize = recvfrom(servHrdrSock, &pid, sizeof(int), 0, (struct sockaddr *)NULL, 0)) < 0)
    {
        DieWithError("recvfrom() from hairdresser failed");
    }
    sprintf(str, "Client %d got a haircut\nHaidresser is sleeping\n", pid);
    write(info_pipe[1], str, strlen(str));

    /* Release client */
    if (sendto(servClntSock, &pid, sizeof(int), 0, (struct sockaddr *)&echoClntAddr, sizeof(echoClntAddr)) != sizeof(int))
    {
        DieWithError("sendto() sent to the client a different number of bytes than expected");
    }
    sprintf(str, "Client %d left\n", pid);
    write(info_pipe[1], str, strlen(str));
}

void *AcceptObserver()
{
    struct sockaddr_in obsrvAddr;
    int recvMsgSize;
    int h;
    unsigned int clntLen;
    clntLen = sizeof(obsrvAddr);
    for (;;)
    {
        if ((recvMsgSize = recvfrom(servObsrvSock, &h, sizeof(int), 0, (struct sockaddr *)&obsrvAddr, &clntLen)) < 0)
        {
            DieWithError("recvfrom() failed");
        }
        for (int i = 0; i < 15; i++)
        {
            if (observers[i].is_active == 0)
            {
                printf("Set observer to position %d\n", i);
                observers[i].addr = obsrvAddr;
                observers[i].is_active = 1;
                break;
            }
        }
    }
}

void setObservers()
{
    for (int i = 0; i < 15; i++)
    {
        observers[i].is_active = 0;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, AcceptObserver, NULL);
}

void *WriteInfo()
{
    char str[150];
    ssize_t rdBytes;
    for (;;)
    {
        rdBytes = read(info_pipe[0], str, 150);
        if (rdBytes < 0)
        {
            DieWithError("Can't read from pipe");
        }
        str[rdBytes] = '\0';
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < 15; ++i)
        {
            if (observers[i].is_active == 1)
            {
                if (sendto(servObsrvSock, &str, strlen(str), 0, (struct sockaddr *)&observers[i].addr, sizeof(observers[i].addr)) != strlen(str))
                {
                    observers[i].is_active = 0;
                    printf("Observer is absent\n");
                }
            }
        }
        pthread_mutex_unlock(&mutex);
    }
}

void StartWriter()
{
    if (pipe(info_pipe) < 0)
    {
        DieWithError("Can\'t open the info pipe\n");
    }

    pthread_t thread;
    pthread_create(&thread, NULL, WriteInfo, NULL);
}

void sigfunc(int sig)
{
    if (sig != SIGINT && sig != SIGTERM)
    {
        return;
    }

    close(servClntSock);
    close(servHrdrSock);
    close(servObsrvSock);
    pthread_mutex_destroy(&mutex);
    close(info_pipe[0]);
    close(info_pipe[1]);
    printf("disconnected\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);

    unsigned int servClntPort;
    unsigned int servHrdrPort;
    unsigned int servObsrvPort;

    if (argc != 5) /* Test for correct number of arguments */
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
    servObsrvPort = atoi(argv[4]);

    servClntSock = createSocket(servClntPort, servAddr);
    servHrdrSock = createSocket(servHrdrPort, servAddr);
    servObsrvSock = createSocket(servObsrvPort, servAddr);

    /* Block until receive message from the hairdresser */
    int recvMsgSize;
    int h;
    unsigned int clntLen;
    clntLen = sizeof(hrdrAddr);
    if ((recvMsgSize = recvfrom(servHrdrSock, &h, sizeof(int), 0, (struct sockaddr *)&hrdrAddr, &clntLen)) < 0)
    {
        DieWithError("recvfrom() failed");
    }

    setObservers();
    pthread_mutex_init(&mutex, NULL);
    StartWriter();

    char str[150];
    sprintf(str, "Hairdresser's is open\n");
    write(info_pipe[1], str, strlen(str));

    for (;;)
    {
        HandleUDPClient();
    }
}
