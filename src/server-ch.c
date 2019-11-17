#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>  // for open
#include <unistd.h> // for close
#include <pthread.h>
#include <netdb.h>

#define NUM_PLAYERS 2
#define IP_ADDRESS "127.0.0.1" // not currently used

struct player_connection
{
    int player_number;
    int connection_socket;
};

char client_message[2000];
char buffer[1024];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int num_connections = 0;
pthread_t thread_id[NUM_PLAYERS];
struct player_connection players[NUM_PLAYERS];
int finish_order[NUM_PLAYERS];
int current_finished = 0;

void *handle_connection(void *arg)
{
    struct player_connection *p = (struct player_connection *)arg;
    int newSocket = p->connection_socket;
    recv(newSocket, client_message, 2000, 0);

    // Locked Section
    pthread_mutex_lock(&lock);
    finish_order[current_finished] = p->player_number;
    current_finished++;
    pthread_mutex_unlock(&lock);

    char finished_place[10];
    snprintf(finished_place, 10, "%d", current_finished);
    send(newSocket, finished_place, 10, 0);
    char message[100] = "\nposition finished\n";
    send(newSocket, message, 100, 0);

    printf("Exit handle_connection \n");
    pthread_exit(NULL);
    return 0;
}

void close_connections()
{
    int i;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        close(players[i].connection_socket);
    }
}

int start_game()
{
    int bytes_sent;
    char *start_message = "Initiating game start\n";
    int start_message_len = 23;

    int i;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        int new_socket = players[i].connection_socket;
        bytes_sent = send(new_socket, start_message, start_message_len, 0);
        if (bytes_sent == -1)
        {
            printf("ERROR: sending to connection %d produced local error", i);
            exit(1);
        }

        // Threaded listen for clients to finish the game
        if (pthread_create(&thread_id[i], NULL, handle_connection, &players[i]) != 0)
            printf("Failed to create thread\n");
    }

    for (i = 0; i < NUM_PLAYERS; i++)
    {
        pthread_join(thread_id[i], NULL);
    }
    return 0;
}

void *listen_for_connections(char *server_ip, int server_port)
{
    int server_socket, new_socket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;
    //Create the socket.
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    // Configure settings of the server address struct
    serverAddr.sin_family = AF_INET;
    //Set port number using htons to use proper byte order
    serverAddr.sin_port = htons(server_port);
    //Set IP address
    serverAddr.sin_addr.s_addr = inet_addr(server_ip);
    //Set all bits of the padding field to 0
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
    //Bind the address struct to the socket
    bind(server_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    //Listen on the socket with 50 connection queue
    if (listen(server_socket, 50) == 0)
        printf("Listening\n");
    else
        printf("Error on listen\n");

    int i = 0;
    while (1)
    {
        // Should not occurr
        if (i > NUM_PLAYERS)
            break;

        //Accept call creates a new socket for the incoming connection
        addr_size = sizeof(serverStorage);
        new_socket = accept(server_socket, (struct sockaddr *)&serverStorage, &addr_size);

        char hoststr[NI_MAXHOST];
        char portstr[NI_MAXSERV];

        getnameinfo((struct sockaddr *)&serverStorage, addr_size, hoststr, sizeof(hoststr), portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);

        printf("New connection from %s %s\n", hoststr, portstr);

        // Create new socket connection when a client connects
        struct player_connection new_player;
        new_player.player_number = i + 1;
        new_player.connection_socket = new_socket;
        players[i] = new_player;

        // if (pthread_create(&thread_id[i], NULL, handle_connection, &new_player) != 0)
        //     printf("Failed to create thread\n");
        // else {
        num_connections++;
        if (num_connections == NUM_PLAYERS)
        {
            break;
        }
        // }
        i++;
    }
    printf("Starting game with %d connections\n", num_connections);
    start_game();
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "ERROR, no port or ip provided\n");
		fprintf(stderr, "Usage: executable <ip> <port>\n");
        exit(1);
    }

	char *server_ip = argv[1];
    int server_port;

    server_port = atoi(argv[2]);

    listen_for_connections(server_ip, server_port);

    close_connections();
    return 0;
}
