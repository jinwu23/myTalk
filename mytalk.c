#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <talk.h>
#include <pwd.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>

#define DEFAULTBACKLOG 1
#define LOCAL 0
#define REMOTE 1
#define BUFFERSIZE 1024
#define YNBUFFSIZE 10


void chat(int socket_fd){
    int len, mlen, done = 0;
    char buff[BUFFERSIZE] = {0};
    struct pollfd fds[REMOTE + 1];

    fds[LOCAL].fd = STDIN_FILENO;
    fds[LOCAL].events = POLLIN;
    fds[LOCAL].revents = 0;
    fds[REMOTE].fd = socket_fd;
    fds[REMOTE].events = POLLIN;
    fds[REMOTE].revents = 0;

    do{
        /* poll the inputs */
        if(poll(fds, 2, -1) == -1){
            perror("poll");
            stop_windowing();
            exit(EXIT_FAILURE);
        }
        /* input came from stdin */
        if(fds[LOCAL].revents & POLLIN){
            /* update input buffer */
            update_input_buffer();
            /* check if we can send a whole line */
            if(has_whole_line()){
                if((len = read_from_input(buff, BUFFERSIZE)) == ERR){
                    fprint_to_output("read_from_input error\n");
                    fflush(stdout);
                    stop_windowing();
                    exit(-3);
                }
                if(send(socket_fd, buff, len, 0) == -1){
                    perror("send");
                    stop_windowing();
                    exit(EXIT_FAILURE);
                }
            }
        }
        /* input came from other guy */
        if(fds[REMOTE].revents & POLLIN){
            /* reciveve and check if they have disconnected on us */
            if((mlen = recv(socket_fd, buff, BUFFERSIZE, 0)) == -1){
                perror("recv");
                stop_windowing();
                exit(EXIT_FAILURE);
            }
            /* they disconnected on us */
            if(mlen == 0){
                fprint_to_output("Connection closed. ^C to terminate.");
                pause();
            }
            if(write_to_output(buff, mlen) == ERR){
                fprint_to_output("write_to_output error\n");
                fflush(stdout);
                stop_windowing();
                exit(-3);
            }
        }
        /* check if we have disconnected */
        if(has_hit_eof()){
            done = 1;
        } 
    } while (!done);
    
}

int main(int argc, char *argv[]){
    int vflag = 0;
    int aflag = 0;
    int Nflag = 0;
    int c, i;
    char *hostname = NULL;
    char *portname = NULL;
    int socket_fd, friend_fd;
    struct sockaddr_in my_sa, friend_sa;
    socklen_t friend_sa_size;
    struct hostent *hostent;
    uid_t uid;
    struct passwd *pwd;
    char buff[BUFFERSIZE] = {0};
    char input[YNBUFFSIZE] = {0};

    /* parsing command line arguments */
    /* checks argument count, prints usage error */
    if(argc < 2){
        printf("usage: invalid arguments\n");
        exit(-3);
    }

    /* calls getopt to count v, a, and N, will exit if too many same optional argument */
    while((c = getopt(argc, argv, "vaN")) != -1){
        switch(c){
            case 'v':
                vflag += 1;
                break;
            case 'a':
                if(aflag == 1){
                    printf("usage: [ -a ] [ -N ] [ -v ] [ hostname ] port\n");
                    exit(-3);
                }
                aflag = 1;
                break;
            case 'N':
                if(Nflag == 1){
                    printf("usage: [ -a ] [ -N ] [ -v ] [ hostname ] port\n");
                    exit(-3);
                }
                Nflag = 1;
                break;
            default:
                printf("Invalid Argument\n");
                exit(-3);
        }
    }

    /* loops through non-optional arguments for names */
    for(i = optind; i < argc; i++){
        /* if there are two remaining arguments on command line, i points to hostname*/
        if(i == argc - 2){
            hostname = argv[i];
        }
        /* i points to port */
        else if(i == argc - 1){
            portname = argv[i];
        }
        /* too many non optional arguments */
        else{
            printf("usage: [ -a ] [ -N ] [ -v ] [ hostname ] port\n");
            exit(-3);
        }
    }

    /* sets verbosity */
    set_verbosity(vflag);

    /* server side */
    if(hostname == NULL){

        /* create a socket */
        if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
            perror("socket");
            exit(EXIT_FAILURE);
        }

        /* attatch an address to it with bind */
        my_sa.sin_family = AF_INET;
        my_sa.sin_port = htons(atoi(portname));
        my_sa.sin_addr.s_addr = htonl(INADDR_ANY);
        if(bind(socket_fd, (struct sockaddr*)&my_sa, sizeof(my_sa)) == -1){
            perror("bind");
            exit(EXIT_FAILURE);
        }

        /* wait for a connection with listen */
        if(listen(socket_fd, DEFAULTBACKLOG) == -1){
            perror("listen");
            exit(EXIT_FAILURE);
        }

        /* accept a connection with accept, blocks the program */
        friend_sa_size = sizeof(friend_sa);
        if((friend_fd =  accept(socket_fd, (struct sockaddr *) &friend_sa, &friend_sa_size)) == -1){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        /* recieve name */
        if(recv(friend_fd, buff, BUFFERSIZE, 0) == -1){
            perror("recv");
            exit(EXIT_FAILURE);
        }

        /* get address */
        hostent = gethostbyaddr(&(friend_sa.sin_addr), sizeof(friend_sa.sin_addr), AF_INET);


        /* accept connection without asking */
        if(aflag == 0){

            /* check if we actuall want to connect with client */
            printf("Mytalk request from %s@%s. Accept (y/n)?", buff, hostent -> h_name);
            fflush(stdout);

            scanf("%s", input);
            /* lowercases input */
            for(i = 0; i < YNBUFFSIZE; i++){
                input[i] = tolower(input[i]);
            }
            /* check for yes or no */
            if(!strcmp(input, "yes") || !strcmp(input, "y")){
                /* send ok to client */
                if(send(friend_fd, "ok" , 2, 0) == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }   
            }
            /* no */
            else{
                /* send no to client */
                if(send(friend_fd, "no" , 2, 0) == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
                return 0;
            }
        }
        else{
            /* send ok to client */
            if(send(friend_fd, "ok" , 2, 0) == -1){
                perror("send");
                exit(EXIT_FAILURE);
            }              
        }

        /* send and receive with send() and recv() until done */
        if(Nflag == 0){
            start_windowing();
        }
        
        chat(friend_fd);

        /* close remaining sockets */
        if(close(socket_fd) == -1){
            perror("close");
            exit(EXIT_FAILURE);
        }

        /* close remaining sockets */
        if(close(friend_fd) == -1){
            perror("close");
            exit(EXIT_FAILURE);
        }       

    }


    /* client side */
    else{

        /* look up peer address */
        if((hostent = gethostbyname(hostname)) == NULL){
            perror("gethostbyname");
            exit(EXIT_FAILURE);
        }

        /* create a socket */
        if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
            perror("socket");
            exit(EXIT_FAILURE);
        }

        /*connect socket */
        my_sa.sin_family = AF_INET;
        my_sa.sin_port = htons(atoi(portname));
        my_sa.sin_addr.s_addr = *(uint32_t*)hostent -> h_addr_list[0];
        if(connect(socket_fd, (struct sockaddr *) &my_sa, sizeof(my_sa)) == -1){
            perror("connect");
            exit(EXIT_FAILURE);
        }

        /* find out your name and send it to server */
        /* can this fail? I couldnt find anything so no error check */
        uid = getuid();
        if((pwd = getpwuid(uid)) == NULL){
            perror("getpwduid");
            exit(EXIT_FAILURE);
        }

        printf("Waiting for response from %s\n", hostname);
        fflush(stdout);

        /* send name to server */
        if(send(socket_fd, pwd -> pw_name, sizeof(pwd -> pw_name), 0) == -1){
            perror("send");
            exit(EXIT_FAILURE);
        }

        /* wait for server ok */
        if(recv(socket_fd, buff, BUFFERSIZE, 0) == -1){
            perror("recv");
            exit(EXIT_FAILURE);
        }

        /* declined connection :( be sad */
        if(strcmp(buff, "ok") != 0){
            printf("%s declined connection\n", hostname);
            fflush(stdout);
            return 0;
        }
        
        /* we are connected! */
        /* send an recieve with send() and recv() until done */
        if(Nflag == 0){
            start_windowing();
        }

        chat(socket_fd);

        /* closes window */
        stop_windowing();

        /* close remaining sockets */
        if(close(socket_fd) == -1){
            perror("close");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
