#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h> //in order to check if location is longer than PATH_MAX
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> //for finding size of get file and checking if something is a directory
#include <regex.h>
#include "asgn2_helper_funcs.h"
#include <errno.h>

//------------------static defines------------------

//set max buffer size to be able to hold request line and header fields
#define BUFFER_SIZE 2048
//mini function used with regex
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

//----------------------regular expressions --------------------------------

//regular expression to parse request line
static char *regex_req_line = "([A-Z]{1,8}) +/([a-zA-Z0-9._]{1,63}) +(HTTP/[0-9]*.[0-9]*)\r\n";

//regular expression to parse header fields
static char *regex_header_field = "([a-zA-Z0-9.-]{1,128}): ([\x20-\x7E]{0,128})\r\n";

//regular expression to get first part of message
static char *regex_message = "\r\n\r\n([\x20-\x7E\r\n]*)";

//request line variables
char method[8];
char uri[64];
char version[8];
//header field variables
char key[128];
char value[128];
//message holder
char message[BUFFER_SIZE];

//-----------------------Helper functions----------------------------

//parses the request line of the command and sets values for 'method', 'uri' and 'version'
int parse_req_line(char *s) {
    //initialize all needed regex variables
    regex_t regex;
    regmatch_t pmatch[4];
    regoff_t len;
    int matches = 0;
    //compile regex
    if (regcomp(&regex, regex_req_line, REG_NEWLINE | REG_EXTENDED)) {
        exit(EXIT_FAILURE);
    }
    for (int i = 0;; i++) {
        //once there were no matches found exit loop
        if (regexec(&regex, s, (ARRAY_SIZE(pmatch)), pmatch, 0))
            return matches;

        //loop through pmatch array
        for (int j = 0; j < (int) ARRAY_SIZE(pmatch); ++j) {
            if (pmatch[j].rm_so == -1)
                break; //exit if error
            len = pmatch[j].rm_eo - pmatch[j].rm_so; //find the length of the substring

            //set all needed variables using the substrings that were obtained
            if (j == 1) {
                snprintf(method, len + 1, "%.*s", len, s + pmatch[j].rm_so);
                //printf("\nmethod = %s\n", method);
            } else if (j == 2) {
                snprintf(uri, len + 1, "%.*s", len, s + pmatch[j].rm_so);
                //printf("uri = %s\n", uri);
            } else if (j == 3) {
                //uri is getting cleared here for some reason
                snprintf(version, len + 1, "%.*s", len, s + pmatch[j].rm_so);
                //printf("version = %s\n", version);
            }
        }
        //increment pointer to get to start of next substring
        matches++;
        s += pmatch[0].rm_eo;
    }
    //free any memory that regex allocated
    regfree(&regex);
}

//function to parse header fields
int parse_header_fields(char *s) {
    regex_t regex;
    regmatch_t pmatch[3];
    regoff_t len;

    if (regcomp(&regex, regex_header_field, REG_NEWLINE | REG_EXTENDED)) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0;; i++) {
        if (regexec(&regex, s, (ARRAY_SIZE(pmatch)), pmatch, 0))
            break;

        for (int j = 0; j < (int) ARRAY_SIZE(pmatch); ++j) {
            if (pmatch[j].rm_so == -1)
                break;
            len = pmatch[j].rm_eo - pmatch[j].rm_so;

            if (j == 1) {
                snprintf(key, len + 1, "%.*s", len, s + pmatch[j].rm_so);
                //printf("\nkey = %s\n", key);
            } else if (j == 2) {
                snprintf(value, len + 1, "%.*s", len, s + pmatch[j].rm_so);
                //printf("value = %s\n", value);
            }
        }
        s += pmatch[0].rm_eo;
        //if the key is content-length return its value
        if (strcmp("Content-Length", key) == 0) {
            regfree(&regex);
            return atoi(value);
        }
    }
    return 0;
    regfree(&regex);
}

//function to get first part of message
int parse_message(char *s) {
    regex_t regex;
    regmatch_t pmatch[2];
    regoff_t len;

    if (regcomp(&regex, regex_message, REG_NEWLINE | REG_EXTENDED)) {
        exit(EXIT_FAILURE);
    }

    for (int i = 0;; i++) {
        if (regexec(&regex, s, (ARRAY_SIZE(pmatch)), pmatch, 0))
            break;

        for (int j = 0; j < (int) ARRAY_SIZE(pmatch); ++j) {
            if (pmatch[j].rm_so == -1)
                break;
            len = pmatch[j].rm_eo - pmatch[j].rm_so;

            if (j == 1) {
                snprintf(message, len + 1, "%.*s", len, s + pmatch[j].rm_so);
            }
        }
        s += pmatch[0].rm_eo;
    }
    regfree(&regex);
    return len;
}
//function for printing errors
void fatal_error(const char *msg) {
    fprintf(stderr, "%s", msg);
    exit(1);
}

//function to handle get requests
void get(char location[], int socket) {


    //open the uri to get file descriptor
    int descriptor = open(location, O_RDONLY);

    //if file couldnt be opened
    if (descriptor == -1) {
        close(descriptor);
        write_n_bytes(
            socket, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n", 56);
        return;
    }

    //get info about file
    struct stat file_info;

    // Use fstat to get file information
    if (fstat(descriptor, &file_info) == -1) { //if fstat fails
        close(descriptor);
        write_n_bytes(
            socket, "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n", 56);
        return;
    }
    // Check if the file is a directory
    if (S_ISDIR(file_info.st_mode)) {
        close(descriptor);
        write_n_bytes(
            socket, "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n", 56);
        return;
    }
    //get the size of the file
    int file_size = file_info.st_size;

    // Determine the length of the formatted string including the length of the file
    int length = snprintf(NULL, 0, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", file_size);

    // Allocate memory for the formatted string
    char *message = (char *) malloc(length + 1); // +1 for the null terminator

    // Format the string
    snprintf(message, length + 1, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", file_size);
    //write the string and free it
    write_n_bytes(socket, message, length);
    free(message);
    //write the contents of the file to the socket
    pass_n_bytes(descriptor, socket, file_size);

    //close file descriptor
    close(descriptor);
}

//Put function
void put(char location[], int socket, int length, char message[], int message_len) {

    //try to open uri
    int descriptor = open(location, O_WRONLY | O_TRUNC);
    int made = 0;

    //if file couldnt be opened
    if (descriptor == -1) {
        //try to create the file
        descriptor = open(location, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        made = 1;
        //if file still couldnt be created then there was an error
        if (descriptor == -1) {
            close(descriptor);
            write_n_bytes(
                socket, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n", 56);
            return;
        }
    }
    //send correct message to client
    if (!made) //if file wasnt made produce a different message than if it was made
    {
        // Determine the length of the formatted string including the length of the file
        int length = snprintf(NULL, 0, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");

        // Allocate memory for the formatted string
        char *response = (char *) malloc(length + 1); // +1 for the null terminator

        // Format the string
        snprintf(response, length + 1, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
        write_n_bytes(socket, response, length);
    } else {
        // Determine the length of the formatted string including the length of the file
        int length
            = snprintf(NULL, 0, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n");

        // Allocate memory for the formatted string
        char *response = (char *) malloc(length + 1); // +1 for the null terminator

        // Format the string
        snprintf(
            response, length + 1, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n");
        write_n_bytes(socket, response, length);
    }
    //write the message then write read the rest of the command and write it
    write_n_bytes(descriptor, message, message_len);
    pass_n_bytes(socket, descriptor, length - message_len);
}

//-----------------------main---------------------------------------------------------
int main(int argc, char *argv[]) {

    //initialize the socket and open the port
    Listener_Socket listener_socket;
    int port = atoi(argv[1]);
    //make sure correct number of args was given and port was valid
    if (argc > 2 || !argv[1] || port == 0) {
        fatal_error("Inlvalid command line argument(s).\n");
    }
    //printf("port = %d\n", port);
    //make sure server was able to bind to given port
    if (listener_init(&listener_socket, port) < 0) {
        fatal_error("Operation failed. Could not bind to port\n");
    }

    //initialize buffer with room for a null terminator
    char buffer[BUFFER_SIZE + 1];

    //main loop
    while (1) {

        //get the socket descriptor
        int socket = listener_accept(&listener_socket);

        //read from stdin/socket into buffer
        ssize_t bytes_read = read_until(socket, buffer, BUFFER_SIZE, "\r\n\r\n");
        printf("\nbytes read = %zd\n", bytes_read);
        //bytes_read is being set to -1 even though buffer is being filled
        if (bytes_read == -1) {
            write_n_bytes(socket,
                "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server "
                "Error\n",
                80);
        } else if (bytes_read == 0) {
            write_n_bytes(
                socket, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
        } else {
            //add a null terminator to the end of buffer
            buffer[bytes_read] = '\0';
            //copy buffer to a pointer in order to parse it
            char *str = (char *) malloc(BUFFER_SIZE);
            strcpy(str, buffer);
            //parse the request line
            int req_line_matches = parse_req_line(str);
            //make sure it is a valid request
            //printf("\nmethod = %s  version = %s  uri = %s\n", method, version, uri);
            if (req_line_matches == 0) //make sure request line was able to be parsed
            {
                write_n_bytes(socket,
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
            } else if (!(strcmp(method, "GET") == 0)
                       && !(strcmp(method, "PUT") == 0)) //make sure its a get or put request
            {
                write_n_bytes(socket,
                    "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n",
                    68);
            } else if (strlen(version) > 8) //check if the version was parsed to be correct length
            {
                write_n_bytes(socket,
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
            } else if (!(strcmp(version, "HTTP/1.1") == 0)) //check if version is correct
            {
                write_n_bytes(socket,
                    "HTTP/1.1 505 Version Not Supported\r\nContent-Length: 22\r\n\r\nVersion Not "
                    "Supported\n",
                    80);
            } else if ((strcmp(method, "GET") == 0)) //if its a get request
            {
                //call get with the location of the uri
                get(uri, socket);
            } else if ((strcmp(method, "PUT") == 0)) //if its a put request
            {
                int length = parse_header_fields(str);
                int message_len = parse_message(str);
                put(uri, socket, length, message, message_len);
            }
            //make sure to fully read the rest of the command
            do {
                bytes_read = read_n_bytes(socket, buffer, BUFFER_SIZE);
            } while (bytes_read > 0);

            //close socket connection and free memory and clear buffers
            memset(buffer, 0, BUFFER_SIZE);
            memset(method, 0, 8);
            memset(uri, 0, 64);
            memset(version, 0, 8);
            memset(message, 0, BUFFER_SIZE);
            free(str);
        }

        //close socket
        close(socket);
    }

    return 0;
}
