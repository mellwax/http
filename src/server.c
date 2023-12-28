/**
 * @file server.c
 * @author Kristijan Todorovic, 11806442
 * @date 21.12.2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/socket.h>

static const char *program_name;

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
                if (endptr[0] != '\0') {
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
        }
    }
    if (p_count > 1 || i_count > 1) {
        USAGE("options cannot be called multiple times");
    }
    if (argc - (optind - 1) < 2) {
        USAGE("no document root specified");
    }
    *doc_root = argv[optind];
}

int main(int argc, char **argv) {
    program_name = argv[0];
    int port = 8080;
    char *index = "index.html";
    char *doc_root;

    parse_args(argc, argv, &port, &index, &doc_root);

    fprintf(stdout, "port: %d\n", port);
    fprintf(stdout, "index: %s\n", index);
    fprintf(stdout, "doc_root: %s\n", doc_root);

    return EXIT_SUCCESS;
}
