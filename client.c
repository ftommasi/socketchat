// Christopher Lucas
// Fausto Tommasi
//
// code heavily modified from bind(2) example

#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define MY_SOCK_PATH "./somepath"
#define LISTEN_BACKLOG 50

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct writer_args {
    int sfd;
};

void* writer(void* args) {
    struct writer_args* my_args = (struct writer_args*) args;
    FILE* socket = fdopen(my_args->sfd, "r");

    char buf[255];
    while (fgets(buf, 255, socket) != NULL) {
        printf("%s", buf);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int sfd, cfd;
    struct sockaddr_un my_addr, peer_addr;
    socklen_t peer_addr_size;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1)
        handle_error("socket");

    memset(&my_addr, 0, sizeof(struct sockaddr_un));
                        /* Clear structure */
    my_addr.sun_family = AF_UNIX;
    strncpy(my_addr.sun_path, MY_SOCK_PATH,
            sizeof(my_addr.sun_path) - 1);

    if (connect(sfd, (struct sockaddr *) &my_addr,
            sizeof(struct sockaddr_un)) == -1)
        handle_error("connect");

    pthread_t id;
    struct writer_args arg;
    arg.sfd = sfd;
    if (pthread_create(&id, NULL, writer, (void*) &arg) != 0) {
        handle_error("pthread_create error");
    }

    FILE* socket = fdopen(sfd, "w");

    char buf[255];
    while (fgets(buf, 255, stdin) != NULL) {
        fprintf(socket, "%s", buf);
        fflush(socket);

        if (strncmp("quit\n", buf, 5) == 0) {
            fclose(socket);
            exit(0);
        }
    }
}
