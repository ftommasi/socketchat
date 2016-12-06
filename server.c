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
#include <netinet/ip.h>
#include <arpa/inet.h>

#define MY_SOCK_PATH "./somepath"
#define LISTEN_BACKLOG 50

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct Node {
    int cfd;
    char* name;
    struct Node* next;
} Node;

Node* head;

struct forwarder_args {
    int cfd;
    pthread_mutex_t* mutex;
};


void dump_list(){
  Node* curr = head->next;
  int node_count =0;
  printf("head-> ");
  while(curr!= NULL ){
    printf("| %s -> ",curr->name);
    curr = curr->next;
    node_count++;
  }
  printf("\nfound %d nodes\ndone.\n",node_count);
}

void* forwarder(void* args) {
    struct forwarder_args* my_args = (struct forwarder_args*) args;
    int cfd = my_args->cfd;
    

    FILE* socket = fdopen(cfd, "r");
    if (socket == NULL) {
        handle_error("fdopen");
    }

    Node* self = head;
    while (self != NULL && self->cfd != cfd) {
        self = self->next;
    }

    char buf[255];
    while (fgets(buf, 255, socket) != NULL) {
        
        if (pthread_mutex_lock(my_args->mutex)) {
            handle_error("lock error");
        }

        if (strncmp("quit\n", buf, 5) == 0) {
            // run quit code below
            break;
        } 
        else if (strncmp("name ", buf, 5) == 0) {
            // change name
            // save names
            char* oldname = (char*) malloc(strlen(self->name) + 1);
            if (oldname == NULL) {
                handle_error("ond name malloc error");
            }
            strncpy(oldname, self->name, strlen(self->name));
            char* newname = (char*) malloc(strlen(buf) + 1);
            if (newname == NULL) {
                handle_error("new name malloc error");
            }
            strncpy(newname, buf + 5, strlen(buf) - 5 - 1);
            char* message = " has changed their name to ";

            if (strlen(newname) != 0) {
                // replace name
                free(self->name);
                self->name = newname;

                // send change messages
                Node* curr = head->next;
                while (curr != NULL) {
                    if (curr->cfd != cfd) {
                        if (
                            write(curr->cfd, oldname, strlen(oldname)) == -1 ||
                            write(curr->cfd, message, strlen(message)) == -1 ||
                            write(curr->cfd, newname, strlen(newname)) == -1 ||
                            write(curr->cfd, "\n", 1) == -1
                        ) {
                            handle_error("name write failed");
                        }
                    }
                    curr = curr->next;
                }

    dump_list();
            }    

            if (pthread_mutex_unlock(my_args->mutex)) {
                handle_error("name mutex unlock");
            }
        } else {
            // forward message
            Node* curr = head->next;
            while (curr != NULL) {
                if (curr->cfd != cfd) {
                    if (
                        write(curr->cfd, self->name, strlen(self->name)) == -1 ||
                        write(curr->cfd, ": ", 2) == -1 ||
                        write(curr->cfd, buf, strlen(buf)) == -1
                    ) {
                        handle_error("forward write failed");
                    }
                }
                curr = curr->next;
            }

            if (pthread_mutex_unlock(my_args->mutex)) {
                handle_error("forward mutex unlock");
            }
        }
    }

    // handle quit or socket break
    // write has quit message
    Node* curr = head->next;
    while (curr != NULL) {
        if (curr->cfd != cfd) {
            if (
                write(curr->cfd, self->name, strlen(self->name)) == -1 ||
                write(curr->cfd, " has quit\n", 10) == -1
            ) {
                handle_error("quit write failed");
            }
        }
        curr = curr->next;
    }

    // remove node
    curr = head;
    while (curr != NULL && curr->next != NULL && curr->next->cfd != cfd) {
        curr = curr->next;
    }
    
    Node* to_delete = curr->next;
    curr->next = to_delete->next;
    free(to_delete->name);
    free(to_delete);

    // close socket
    if (fclose(socket)) {
        handle_error("fclose");
    }

    if (pthread_mutex_unlock(my_args->mutex)) {
        handle_error("quit mutex unlock");
    }
    return NULL;
}

char verify_args(char* argv[]){
  //verify IP & socket number
  return 1;
}

void usage_error_print(){
  printf("Usage client <IP address> <port number>\n");
}


int main(int argc, char *argv[])
{
    
    if(argc < 3 || argc > 3){
      usage_error_print();
      return -1;
    }
    if(!verify_args(argv)){
      usage_error_print();
      return -1;
    }
    char IP_address[256];
    strcpy(&IP_address,argv[1]);
    int port_num = atoi(argv[2]); 
    head = (struct Node*) malloc(sizeof(Node));
    if (head == NULL) {
        handle_error("error mallocing head");
    }
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    int sfd, cfd;
    struct sockaddr_in my_addr, peer_addr;
    socklen_t peer_addr_size;

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        handle_error("socket");

    memset(&my_addr, 0, sizeof(struct sockaddr_in));
                        /* Clear structure */
    my_addr.sin_family = AF_INET;
    //strncpy(my_addr.sun_path, MY_SOCK_PATH,     sizeof(my_addr.sun_path) - 1);
    my_addr.sin_port = htons(port_num);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    //inet_aton("127.0.0.1",&my_addr.sin_addr);
    if (bind(sfd, (struct sockaddr *) &my_addr,
            sizeof(struct sockaddr_in)) == -1)
        handle_error("bind");

    if (listen(sfd, LISTEN_BACKLOG) == -1)
        handle_error("listen");

    peer_addr_size = sizeof(struct sockaddr_in);

    for(;;) {
        cfd = accept(sfd, (struct sockaddr *) &peer_addr,
                     &peer_addr_size);
        if (cfd == -1)
            handle_error("accept");

        Node* curr = head;
        while (curr!= NULL && curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = (struct Node*) malloc(sizeof(Node));
        if (curr->next == NULL) {
            handle_error("error mallocing node");
        }
        curr = curr->next;
        curr->cfd = cfd;
        curr->name = (char*) malloc(8);
        if (curr->name == NULL) {
            handle_error("error mallocing initial name");
        }
        strncpy(curr->name, "unnamed\0", 8);
        dump_list();
        pthread_t id;
        struct forwarder_args arg;
        arg.cfd = cfd;
        arg.mutex = &mutex;

        if (pthread_create(&id, NULL, forwarder, (void*) &arg) != 0) {
            handle_error("pthread_create error for forwarder");
        }
        dump_list();

    }

    if (unlink(MY_SOCK_PATH)) {
        handle_error("unlink");
    }

}
