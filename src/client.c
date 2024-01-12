/**
 * @file client.c
 * @author Kristijan Todorovic, 11806442
 * @date 21.12.2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#define URL_DELIMS ";/?:@=&"

static const char *program_name;

/**
 * @brief prints the error message to stderr and terminates the program with EXIT_FAILURE
 *
 * @param message error message to be printed
 */
static void ERROR_EXIT(const char *message) {
    fprintf(stderr, "[%s]: %s\n", program_name, message);
    exit(EXIT_FAILURE);
}

/**
 * @brief prints a usage message to stderr and terminates the program with EXIT_FAILURE
 *
 * @param message detailed message describing wrong usage
 */
static void USAGE(const char *message) {
    fprintf(stderr, "[%s]: %s\nUSAGE: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", program_name, message, program_name);
    exit(EXIT_FAILURE);
}

/**
 * @brief this function frees the url_path and host strings, which were malloced
 *
 * @param url_path malloced string to be freed
 * @param host malloced string to be freed
 */
static void free_strings(char *url_path, char *host) {
    free(url_path);
    free(host);
}

/**
 * @brief parses the arguments passed to this program.
 *
 * @details
 *      [-p PORT] specifies the port, on which the client socket shall connect to. The port must not be negative.
 *
 *      [-o FILE | -d DIR] specifies, where to write the output of this program.
 *          -o specifies a filename, which shall be written to
 *          -d specifies a directory, in which a file with the same name, as the requested file, shall be saved to
 *          if these options are omitted the output is written to stdout.
 *
 * @param argc argument count
 * @param argv argument values
 * @param port port number to be set
 * @param url_path requested url path to be set
 * @param host host name to be set
 * @param output output file to be set
 */
static void parse_args(int argc, char **argv, int *port, char **url_path, char **host, FILE **output) {
    int c;
    int p_count = 0, o_count = 0, d_count = 0;
    char *d_opt;

    while ((c = getopt(argc, argv, "p:o:d:")) != -1) {
        switch (c) {
            case 'p': {
                char *endptr;
                long l = strtol(optarg, &endptr, 10);
                if (*endptr != '\0') {
                    free_strings(*url_path, *host);
                    USAGE("port has to be a number");
                }
                if (l < 0) {
                    free_strings(*url_path, *host);
                    USAGE("no negative port allowed");
                }
                *port = (int) l;
                p_count++;
                break;
            }
            case 'o': {
                o_count++;
                FILE *file = fopen(optarg, "w+");
                if (file == NULL) {
                    free_strings(*url_path, *host);
                    ERROR_EXIT("error opening file specified by option -o");
                }
                *output = file;
                break;
            }
            case 'd':
                d_count++;
                d_opt = optarg;
                break;
            case '?':
                free_strings(*url_path, *host);
                USAGE("illegal option");
                break;
            default:
                free_strings(*url_path, *host);
                USAGE("unknown option");
                break;
        }
    }
    if (p_count > 1 || o_count > 1 || d_count > 1) {
        free_strings(*url_path, *host);
        USAGE("options cannot be set multiple times");
    }
    if (o_count + d_count > 1) {
        free_strings(*url_path, *host);
        USAGE("options o and d cannot be both set");
    }
    if (argc - (optind - 1) < 2) {
        free_strings(*url_path, *host);
        USAGE("no url specified");
    }
    char *url_tmp = malloc(sizeof(char) * 300);
    if (url_tmp == NULL) {
        free_strings(*url_path, *host);
        ERROR_EXIT("error allocating memory for string url_tmp in parse_args");
    }
    char *url_tmp_ptr = url_tmp;
    strncpy(url_tmp, argv[optind], 200);
    char *http = "http://";
    int http_len = (int) strlen(http);

    if (strncmp(url_tmp, http, http_len) != 0) {
        free_strings(*url_path, *host);
        free(url_tmp_ptr);
        USAGE("incorrect url");
    }

    url_tmp = &url_tmp[http_len];

    for (int i = 0; i <= strlen(URL_DELIMS); i++) {
        if (url_tmp[0] == URL_DELIMS[i]) {
            free_strings(*url_path, *host);
            free(url_tmp_ptr);
            ERROR_EXIT("invalid hostname");
        }
    }

    char url_tmp2[300];
    strcpy(url_tmp2, url_tmp);

    url_tmp = strsep(&url_tmp, URL_DELIMS);
    strcpy(*host, url_tmp);

    char *ptr = strpbrk(url_tmp2, URL_DELIMS);
    if (ptr == NULL) {
        ptr = "/";
    }
    if (*ptr != '/') {
        sprintf(*url_path, "/%s", ptr);
    } else {
        strcpy(*url_path, ptr);
    }

    if (d_count == 1) {
        char *ptr2 = strrchr(url_tmp2, '/');
        if (ptr2 == NULL || strlen(ptr2) == 1) {
            strcat(d_opt, "/index.html");
        } else {
            strcat(d_opt, ptr2);
        }

        FILE *file = fopen(d_opt, "w+");
        if (file == NULL) {
            free_strings(*url_path, *host);
            free(url_tmp_ptr);
            ERROR_EXIT("error opening file specified by -d");
        }
        *output = file;
    }
    free(url_tmp_ptr);
}

/**
 * @brief This function sends a HTTP GET request to the opened socket file.
 *
 * @param url_path the requested file path
 * @param host the host name
 * @param sockfile the socketfile, to which the request is sent
 *
 * @return  0, if flushing to sockfile was successful
 *          EOF, otherwise
 */
static int send_get_request(char *url_path, char *host, FILE *sockfile) {
    char get_request[512];
    snprintf(get_request, sizeof(get_request), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, host);

    fputs(get_request, sockfile);
    return fflush(sockfile);
}

/**
 * @brief this function closes or frees all resources, which this program has used.
 *
 * @param ai the addrinfo struct
 * @param output the output file
 * @param sockfile the socket file
 * @param url_path the requested file path string
 * @param host the host name string
 * @param response the response string
 */
static void close_free_resources(struct addrinfo *ai, FILE *output, FILE *sockfile, char *url_path,
        char *host, char *response) {

    freeaddrinfo(ai);
    free_strings(url_path, host);
    free(response);

    if (fclose(sockfile) != 0) {
        if (output != stdout) {
            if (fclose(output) != 0) {
                ERROR_EXIT("error closing output file");
            }
        }
        ERROR_EXIT("error closing socket file");
    }
    if (output != stdout) {
        if (fclose(output) != 0) {
            ERROR_EXIT("error closing output file");
        }
    }
}

/**
 * @brief The main function of this program, containing the functionality (sending a request for a file to a server,
 * and then writing the response, if successful to the specified option or stdout, and if unsuccessful to stderr).
 *
 * @details The arguments passed to this program are first parsed, then a socket to the specified host and port number
 * is opened. If successful, the request for the desired file is sent to the host and the response is written to either
 * the specified output file (-o or -d), or stdout, if the host returns a status code other than 200 OK, the response is
 * written to stderr.
 *
 * @param argc argument count
 * @param argv argument values
 *
 * @return  EXIT_SUCCESS, if no errors occurred during execution
 *
 *          2, if the HTTP response header is invalid
 *
 *          3, if the HTTP response status code is not 200
 *
 *          EXIT_FAILURE, otherwise
 */
int main(int argc, char **argv) {
    program_name = argv[0];
    int port = 80;
    FILE *output = stdout;

    char *url_path = malloc(sizeof(char) * 300);
    if (url_path == NULL) {
        ERROR_EXIT("allocating memory for url_path string failed");
    }

    char *host = malloc(sizeof(char) * 200);
    if (host == NULL) {
        free(url_path);
        ERROR_EXIT("allocating memory for host string failed");
    }

    parse_args(argc, argv, &port, &url_path, &host, &output);

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);
    int res = getaddrinfo(host, port_str, &hints, &ai);

    if (res != 0) {
        free_strings(url_path, host);
        freeaddrinfo(ai);

        ERROR_EXIT("error getting address info");
    }

    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0) {
        free_strings(url_path, host);
        freeaddrinfo(ai);

        ERROR_EXIT("error creating socket file descriptor");
    }

    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        free_strings(url_path, host);
        freeaddrinfo(ai);

        ERROR_EXIT("error connecting to socket");
    }

    FILE *sockfile = fdopen(sockfd, "r+");
    if (sockfile == NULL) {
        free_strings(url_path, host);
        freeaddrinfo(ai);
        if (close(sockfd) == -1) {
            ERROR_EXIT("error closing socket");
        }
        ERROR_EXIT("error connecting to socket");
    }

    if (send_get_request(url_path, host, sockfile) != 0) {
        close_free_resources(ai, output, sockfile, url_path, host, NULL);
        ERROR_EXIT("error while flushing get request");
    }

    char buffer[2048];
    char *response = malloc(sizeof(char) * 2048);
    strcpy(response, "");
    size_t response_size = 0;
    size_t response_cap = sizeof(char) * 2048;

    int status_code = -1;
    char *status_descr;
    int received_empty_line = 0;

    while (fgets(buffer, sizeof(buffer), sockfile) != NULL) {

        if (status_code == -1) {
            char *http_signature = "HTTP/1.1";
            if (strncmp(buffer, http_signature, strlen(http_signature)) == 0) {
                char copy[sizeof(buffer)];
                strcpy(copy, buffer);
                char *ptr = &copy[strlen(http_signature)];

                status_code = (int) strtol(ptr, &status_descr, 10);
                status_descr = &status_descr[1];
                if (status_code != 200) {
                    break;
                }
            }
        }

        if (received_empty_line == 0) {
            if (isspace(*buffer)) {
                received_empty_line++;
            }
            continue;
        }

        size_t buffer_size = strlen(buffer);
        if (response_size + buffer_size >= response_cap) {
            response_cap *= 2;
            char *new_response = realloc(response, response_cap);
            if (new_response == NULL) {
                close_free_resources(ai, output, sockfile, url_path, host, response);
                ERROR_EXIT("error reallocating memory for http response");
            }
            response = new_response;
        }
        response_size += buffer_size;
        strcat(response, buffer);
    }

    if (status_code == 200) {
        fputs(response, output);
        if (fflush(output) != 0) {
            close_free_resources(ai, output, sockfile, url_path, host, response);
            ERROR_EXIT("error flushing output stream");
        }
    } else if (status_code == -1 || status_code == 0) {
        fprintf(stderr, "Protocol error!\n");
        close_free_resources(ai, output, sockfile, url_path, host, response);
        return 2;
    } else {
        fprintf(stderr, "Error %d %s\n", status_code, status_descr);
        close_free_resources(ai, output, sockfile, url_path, host, response);
        return 3;
    }

    close_free_resources(ai, output, sockfile, url_path, host, response);

    return EXIT_SUCCESS;
}
