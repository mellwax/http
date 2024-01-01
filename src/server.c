/**
 * @file server.c
 * @author Kristijan Todorovic, 11806442
 * @date 21.12.2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#define HTTP_version "HTTP/1.1"

static const char *program_name;
volatile sig_atomic_t quit = 0;

static void handle_signal(int signal) {
    quit = 1;
}

static void ERROR_EXIT(char *message) {
    fprintf(stderr, "[%s]: %s\n", program_name, message);
    exit(EXIT_FAILURE);
}

static void USAGE(const char *message) {
    fprintf(stderr, "[%s]: %s\nUSAGE: %s [-p PORT] [-i INDEX] DOC_ROOT\n", program_name, message, program_name);
    exit(EXIT_FAILURE);
}

static void parse_args(int argc, char **argv, int *port, char **index, char **doc_root) {
    int c;
    int p_count = 0, i_count = 0;

    while ((c = getopt(argc, argv, "p:i:")) != -1) {
        switch (c) {
            case 'p': {
                char *endptr;
                long l = strtol(optarg, &endptr, 10);
                if (*endptr != '\0') {
                    USAGE("port has to be a number");
                }
                if (l < 0) {
                    USAGE("no negative port allowed");
                }
                *port = (int) l;
                p_count++;
                break;
            }
            case 'i':
                *index = optarg;
                i_count++;
                break;
            case '?':
                USAGE("illegal option");
                break;
            default:
                USAGE("unknown option");
                break;
        }
    }
    if (p_count > 1 || i_count > 1) {
        USAGE("options cannot be set multiple times");
    }
    if (argc - (optind - 1) < 2) {
        USAGE("no document root specified");
    }
    *doc_root = argv[optind];
}

static off_t get_file_size(FILE *file) {
    struct stat st;

    if (fstat(fileno(file), &st) != 0) {
        return -1;
    }

    return st.st_size;
}

static void send_invalid_response(FILE *connection, int response_status, char *response_status_msg) {
    char response[1024];
    snprintf(response, sizeof(response), "HTTP/1.1 %d %s\r\nConnection: close\r\n\r\n", response_status,
             response_status_msg);
    fputs(response, connection);
    fflush(connection);
}

static void send_response(FILE *connection, FILE *requested_file) {
    char header[512];
    char date_time[128];

    time_t curr_time;
    struct tm *time_info;
    time(&curr_time);
    time_info = gmtime(&curr_time);

    strftime(date_time, sizeof(date_time), "%a, %d %b %Y %H:%M:%S GMT", time_info);
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nDate: %s\r\nContent-Length: %jd\r\nConnection: close\r\n\r\n",
             date_time, (intmax_t) get_file_size(requested_file));

    fputs(header, connection);

    char buffer[2048];
    while (fgets(buffer, sizeof(buffer), requested_file) != NULL) {
        fputs(buffer, connection);
    }
    fputs("\n", connection);

    fflush(connection);
}

int main(int argc, char **argv) {
    program_name = argv[0];
    int port = 8080;
    char *index = "index.html";
    char *doc_root;

    parse_args(argc, argv, &port, &index, &doc_root);

    struct sigaction sa = {.sa_handler = handle_signal};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int res = getaddrinfo(NULL, port_str, &hints, &ai);
    if (res != 0) {
        freeaddrinfo(ai);
        ERROR_EXIT("error getting address info");
    }

    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(ai);
        ERROR_EXIT("error creating socket file descriptor");
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        freeaddrinfo(ai);
        close(sockfd);
        ERROR_EXIT("error setting socket options");
    }

    if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        freeaddrinfo(ai);
        close(sockfd);
        ERROR_EXIT("error binding socket");
    }

    if (listen(sockfd, 5) < 0) {
        freeaddrinfo(ai);
        close(sockfd);
        ERROR_EXIT("error while listening for socket connections");
    }

    freeaddrinfo(ai);

    while (!quit) {
        fprintf(stdout, "waiting for connection\n");
        int connfd = accept(sockfd, NULL, NULL);
        if (connfd < 0) {
            if (close(sockfd) != 0) {
                ERROR_EXIT("error closing socket file descriptor");
            }
            if (errno == EINTR) {
                return EXIT_SUCCESS;
            }
            ERROR_EXIT("error accepting new connection");
        }


        FILE *connection = fdopen(connfd, "r+");
        if (connection == NULL) {
            if (close(sockfd) != 0) {
                ERROR_EXIT("error closing socket file descriptor");
            }
            ERROR_EXIT("error opening connection file");
        }

        char buffer[1024];

        fgets(buffer, sizeof(buffer), connection);
        fprintf(stderr, "buffer:\n%s\n", buffer);

        char *buffer_cpy = malloc(sizeof(buffer));
        if (buffer_cpy == NULL) {
            send_invalid_response(connection, 507, "(Insufficient Storage)");
            goto connection_closing;
        }
        char *buffer_cpy_ptr = buffer_cpy;

        strcpy(buffer_cpy, buffer);

        char *method = strsep(&buffer_cpy, " ");
        char *requested_path = strsep(&buffer_cpy, " ");
        char *protocol = buffer_cpy;

        if (protocol == NULL || strncmp(protocol, HTTP_version, sizeof(HTTP_version) - 1) != 0) {
            send_invalid_response(connection, 400, "(Bad Request)");
            goto connection_closing;
        }

        if (strncmp(method, "GET", sizeof("GET")) != 0) {
            send_invalid_response(connection, 501, "(Not implemented)");
            goto connection_closing;
        }

        if (requested_path[strlen(requested_path) - 1] == '/') {
            strcat(requested_path, index);
        }

        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s%s", doc_root, requested_path);

        FILE *requested_file = fopen(file_path, "r");
        if (requested_file == NULL) {
            send_invalid_response(connection, 404, "(Not Found)");
        } else {
            send_response(connection, requested_file);
        }

        connection_closing:
        fprintf(stderr, "method: %s\n", method);
        fprintf(stderr, "path: %s\n", requested_path);
        fprintf(stderr, "protocol: %s\n", protocol);

        free(buffer_cpy_ptr);

        if (requested_file != NULL) {
            if (fclose(requested_file) != 0) {
                if (fclose(connection) != 0) {
                    if (close(sockfd) != 0) {
                        ERROR_EXIT("error closing socket file descriptor");
                    }
                    ERROR_EXIT("error closing connection");
                }
                ERROR_EXIT("error closing requested file");
            }
        }

        fprintf(stdout, "closing connection\n");
        if (fclose(connection) != 0) {
            if (close(sockfd) != 0) {
                ERROR_EXIT("error closing socket file descriptor");
            }
            ERROR_EXIT("error closing connection");
        }
    }

    if (close(sockfd) != 0) {
        ERROR_EXIT("error closing socket file descriptor");
    }

    return EXIT_SUCCESS;
}
