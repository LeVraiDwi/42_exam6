#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

//nc -N localhost 6667 < txt

int main(int ac, char **av)
{
    int port;
    if (ac < 2)
    {
        printf("Error\n");
        exit(1);
    }
    port = atoi(av[1]);
    int sockD = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port); // use some unused port number
    servAddr.sin_addr.s_addr = htonl(2130706433);
    int connectStatus = connect(sockD, (struct sockaddr*)&servAddr,sizeof(servAddr));
    if (connectStatus == -1) {
        printf("Error...\n");
    }
    else {
        int t = 1000;
        char *strData = "salut\nsadasd\ndas";
        send(sockD, strData, strlen(strData), 0);
    }
    return 0;
}