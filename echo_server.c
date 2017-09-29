/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "parse.h"
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>

#define PORT "9999"
#define BUF_SIZE 4096

char * log_filename;
char data_path[1024];

// -------------------------------------- CLOSE SOCKET --------------------------------------------------


int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}


// -------------------------------------- POST --------------------------------------------------
void handlePOST(int fd){

    // initialize buffers
    char buff[1024];
    char date[1024];
    handleDate(date);

    sprintf(buff, "HTTP/1.1 200 OK\r\n");
    sprintf(buff, "%sServer: Liso/1.0\r\n", buff);
    sprintf(buff, "%sDate: %s\r\n", buff, date);
    sprintf(buff, "%sContent-Length: 0\r\n", buff);
    sprintf(buff, "%sContent-Type: text/html\r\n\r\n", buff);
    send(fd, buff, strlen(buff), 0);
    fprintf(stdout, "Sent POST to Client!\n");

}

// -------------------------------------- GET --------------------------------------------------
void handleGET(int fd, Request *request){
    // check if file exists --> fopen()
         //check if its a file (not a directory) --> stat.S_IFMT
         //check readable permissions --> stat.S_IRUSR
         //get the file (stat(filename, fileDetails))
         //build headers from fileDetails
         //for body, continuously fread() or fget() (check how they represent end of file)
         //initialize buf = char buf[1024]
         //while (nbytes = fread(buf) > 0))
         //send(fd, buf, sizeof(buf), 0)

    // append uri to path
    char full_path[255];
    char * default_path = "/index.html";
    if (!strcasecmp(request->http_uri,"/")){
        sprintf(full_path, "%s%s", data_path, default_path);
    } else {
        sprintf(full_path, "%s%s", data_path, request->http_uri);
    }


    // check if file exists
    FILE * fp;
    fp = fopen(request->http_uri, "r");
    if (!fp){
        sendError(fd, 404, request);
        return;
    }

    //  check readable permissions

    // send headers
    handleHEAD(fd, request);

    // continuously read from file
    char file_buf[1024];
    int nbytes;
    while(nbytes = fread(file_buf, sizeof(file_buf),1,fp) > 0){
        send(fd,file_buf,strlen(file_buf),0);
    }

}

// -------------------------------------- HEAD --------------------------------------------------
void handleHEAD(int fd, Request *request){

    //get current date
    char date[1024];
    handleDate(date);

    //initialize buffer
    char response_buff[8192];


    //determine content type
    char content_type[1024];
    getMimeType(request->http_uri, content_type);

    // Get file information
    int content_length;
    struct stat fileDetails;
//    fileDetails = malloc(sizeof(struct stat));

    stat(request->http_uri, &fileDetails);
//    char *filename = "index.html";
//    stat(filename, &fileDetails);
    content_length = fileDetails.st_size;

    // last modified
//    time_t lastmod_time;
//    lastmod_time = &fileDetails.st_mtime;
//    time ( &lastmod_time );
//
//    struct tm *lastmod_time2;
//    lastmod_time2 = localtime ( &lastmod_time );
    char lastModified[500];
    struct tm *temp;
    temp = gmtime(&fileDetails.st_mtime);
    strftime(lastModified, 64, "%a, %d %b %Y %H:%M:%S %Z", &temp);

    //format output
    sprintf(response_buff, "HTTP/1.1 200 OK\r\n");
    sprintf(response_buff, "%sDate: %s\r\n",response_buff, date);
    sprintf(response_buff, "%sServer: Liso/1.0\r\n", response_buff);
    sprintf(response_buff, "%sLast-Modified: %s\r\n", response_buff, lastModified);
    sprintf(response_buff, "%sContent-Length: %ld\r\n", response_buff, content_length);
    sprintf(response_buff, "%sContent-Type: %s\r\n\r\n", response_buff, content_type);
    fprintf(stdout, response_buff);
    send(fd, response_buff, strlen(response_buff), 0);
    fprintf(stdout, "Sent HEAD to Client!\n");

}


// -------------------------------------- Get the Date --------------------------------------------------
void handleDate(char *date){
        struct tm tm;
        time_t now;
        now = time(0);
        tm = *gmtime(&now);
        strftime(date, BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

// -------------------------------------- WRITE TO LOG --------------------------------------------------
void writeLog(char *file_name, char *message){

    // Open file
    FILE * fp;
    fp = fopen(file_name, "a");

    // Print timestamp
    struct tm tm;
    time_t now;
    now = time(0);
    tm = *gmtime(&now);

    // print to file
    fprintf(fp, "%s\n", message);

    fclose(fp);
}

// -------------------------------------- GET MIME TYPE --------------------------------------------------

void getMimeType(char *http_uri, char *mimeType) {

    //splitting the uri to get the file name
    if(strstr(http_uri, ".html"))
        strcpy(mimeType, "text/html");
    else if (strstr(http_uri, ".png"))
        strcpy(mimeType, "image/png");
    else if (strstr(http_uri, ".png"))
        strcpy(mimeType, "image/png");
    else if (strstr(http_uri, ".jpg"))
        strcpy(mimeType, "image/jpeg");
    else if (strstr(http_uri, ".gif"))
        strcpy(mimeType, "image/gif");
    else if (strstr(http_uri, ".css"))
        strcpy(mimeType, "text/css");
    else if (strstr(http_uri, ".js"))
        strcpy(mimeType, "application/javascript");
    else
        strcpy(mimeType, "text/plain");

}


// -------------------------------------- ERROR FUNCTION --------------------------------------------------
int sendError(int fd, int error_code, Request *request) {

    // initialize buffers and get date
    char ebuf[1024];
    char date[1024];
    char output[1024];
    handleDate(date);

    // get the message based on the id
    if (error_code == 400) {
        sprintf(output, "Bad Request. \n");
    } else if (error_code == 404) {
        sprintf(output, "Not Found. \n");
    } else if (error_code == 500) {
        sprintf(output, "Internal Server Error. \n");
    } else if (error_code == 501) {
        sprintf(output, "Not Implemented. \n");
    } else if (error_code == 505) {
        sprintf(output, "HTTP Version Not Supported. \n");
    } else {
        sprintf(output, "Unknown Error.\n");
    }

//    if(is_closed)
//        sprintf(output,"Connection is closed.\n");

    // build buffer and send to client
    sprintf(ebuf, "%s %d %s\r\n", request->http_version, error_code, output);
    sprintf(ebuf, "%sServer: Liso/1.0\r\n", ebuf);
    sprintf(ebuf, "%sDate: %s\r\n", ebuf, date);
    sprintf(ebuf, "%sContent-Length: %ld\r\n", ebuf, strlen(output));
    sprintf(ebuf, "%sContent-Type: text/html\r\n\r\n", ebuf);
    send(fd, ebuf, strlen(ebuf), 0);
    fprintf(stdout, "Sent ERROR to Client!\n");
}

// -------------------------------------- Get_in_addr --------------------------------------------------

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// -------------------------------------- MAIN --------------------------------------------------

int main(int argc, char **argv)
{
    // portnum, logfile, www (folder name)
    // portnum is fed directly into getaddrinfo
    // logfile is passed to writeLog
    // www is made to the beginning of a path variabble
    // if last character of www is '/' then add index.html

    // get command-line arguments
//    log_filename = "log.txt";
    writeLog(log_filename, "Test Test");

    char* portnum = argv[0];
    log_filename = argv[1];
    data_path = argv[2];

    fprintf(stdout, log_filename);
    fprintf(stdout, "\n");
    fprintf(stdout, data_path);

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[8192];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, portnum, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                                       "socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {

                        // PARSE HTTP
                        fprintf(stdout, "About to Parse\n");
                        fprintf(stdout,buf);
                        Request *request = parse(buf,nbytes,i);
                        printf("Http Method %s\n",request->http_method);
                        printf("Http Version %s\n",request->http_version);
                        printf("Http Uri %s\n",request->http_uri);
                        int index;
                        for(index = 0;index < request->header_count;index++){
                            printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
                        }
                        fprintf(stdout, "Finished Parsing\n");

                        // FIGURE OUT WHICH METHOD TO CALL
                        int method;
                        if (!strcasecmp(request->http_method,"GET")){
                            method = 1;
                        } else if (!strcasecmp(request->http_method,"HEAD")){
                            method = 2;
                        } else if (!strcasecmp(request->http_method,"POST")){
                            method = 3;
                        }
                        fprintf(stdout, "Finished Matching\n");

                        // CALL METHOD
                        switch (method) {

                            // GET
//                            case 1:
//                                handleGET(i);
//                                break;
//
                                // HEAD
                            case 2:
                                fprintf(stdout, "Handling the HEAD\n");
                                handleHEAD(i, request);
                                break;

                                // POST
                            case 3:
//                                readBody(&buf);
                                fprintf(stdout, "Handling the POST\n");
                                handlePOST(i);
//                                close_socket(i);
//                                FD_CLR(i,&master);
                                break;

                            default:
                                fprintf(stdout, "Missed Match\n");

                        }
                        }
                    } // END handle data from client
                } // END got new incoming connection
            } // END looping through file descriptors
        } // END for(;;)--and you thought it would never end!
        return 0;
    }


//    close_socket(listener);
//
//    return EXIT_SUCCESS;
//}
