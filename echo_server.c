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
#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>


#define PORT "9999"
#define BUF_SIZE 4096

char * log_filename;
char data_path[1024];

// ------------------------------------Function Definitions --------------------------------------------------
int close_socket(int sock);
void handleDate(char *date);
void handlePOST(int fd);
int handleGET(int fd, Request *request);
int handleHEAD(int fd, Request *request);
void writeLog(char *file_name, char *message);
void getMimeType(char *http_uri, char *mimeType);
int sendError(int fd, int error_code, Request *request);
void *get_in_addr(struct sockaddr *sa);

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

    // Open file
    FILE * fp;
    fp = fopen("POSTlog.txt", "a");

    // Print timestamp
    char timestamp[128];
    handleDate(timestamp);

    // Put together full message
    char *message = "post side effect";
    char full_message[1024];
    sprintf(full_message, "[%s]: %s\n", timestamp, message);

    // print to file
    fprintf(fp, "%s\n", full_message);
    fclose(fp);

    // Send Response to client
    sprintf(buff, "HTTP/1.1 200 OK\r\n");
    sprintf(buff, "%sServer: Liso/1.0\r\n", buff);
    sprintf(buff, "%sDate: %s\r\n", buff, date);
    sprintf(buff, "%sContent-Length: 0\r\n", buff);
    sprintf(buff, "%sContent-Type: text/html\r\n\r\n", buff);
    send(fd, buff, strlen(buff), 0);
    fprintf(stdout, "Sent POST to Client!\n");

}

// -------------------------------------- GET --------------------------------------------------
int handleGET(int fd, Request *request){

    // append uri to path
    char full_path[255];
    char * default_path = "/index.html";
    if (!strcasecmp(request->http_uri,"/")){
        sprintf(full_path, "%s%s", data_path, default_path);
    } else {
        sprintf(full_path, "%s%s", data_path, request->http_uri);
    }
    fprintf(stdout, "Fetching File From...");
    fprintf(stdout, full_path);
    fprintf(stdout, "\n");


    // check if file exists
    int fp;
    fp = open(full_path, O_RDONLY);
    if (fp < 0){
        sendError(fd, 404, request);
        return -1;
    }
    fprintf(stdout, "File Exists!\n");

    //  check readable permissions
    fprintf(stdout, "File Permissions are Appropriate!\n");

    // send headers
    fprintf(stdout, "Sending Headers...\n");
    handleHEAD(fd, request);

    // continuously read from file
    fprintf(stdout, "Sending Body...\n");

    // use mmap() to get the pointer to the file address in memory --> send from there
    struct stat fileDetails;
    stat(full_path, &fileDetails);
    int filesize;
    filesize = fileDetails.st_size;
    char *address;
    address = mmap(0, filesize, PROT_READ, MAP_PRIVATE, fp, 0);
    close(fp);
    send(fd, address, filesize, 0);
    munmap(address, filesize);
    return 1;

//    char file_buf[1024];
//    int nbytes;
//    while(nbytes = read(fp, file_buf, strlen(file_buf)) > 0){
//        send(fd,file_buf,strlen(file_buf),0);
//    }
//    fprintf(stdout, "Sent GET Response to client\n");
//    fclose(fp);


}

// -------------------------------------- HEAD --------------------------------------------------
int handleHEAD(int fd, Request *request){

    //get current date
    char date[1024];
    handleDate(date);

    //initialize buffer
    char response_buff[8192];

    //determine content type
    char content_type[1024];
    getMimeType(request->http_uri, content_type);

    // append uri to path
    char full_path[255];
    char * default_path = "/index.html";
    if (!strcasecmp(request->http_uri,"/")){
        sprintf(full_path, "%s%s", data_path, default_path);
    } else {
        sprintf(full_path, "%s%s", data_path, request->http_uri);
    }
    fprintf(stdout, "Fetching File From...");
    fprintf(stdout, full_path);
    fprintf(stdout, "\n");

    // check if file exists
    int fp;
    fp = open(full_path, O_RDONLY);
    if (fp < 0){
        sendError(fd, 404, request);
        return -1;
    }

    // Get file information
    int content_length;
    struct stat fileDetails;

    stat(full_path, &fileDetails);
    content_length = fileDetails.st_size;

    // last modified
    char lastModified[128];
    strftime(lastModified, 64, "%a, %d %b %Y %H:%M:%S %Z", localtime(&(fileDetails.st_mtime)));

    //format output
    sprintf(response_buff, "HTTP/1.1 200 OK\r\n");
    sprintf(response_buff, "%sDate: %s\r\n",response_buff, date);
    sprintf(response_buff, "%sServer: Liso/1.0\r\n", response_buff);
    sprintf(response_buff, "%sConnection: keep-alive\r\n", response_buff);
    sprintf(response_buff, "%sLast-Modified: %s\r\n", response_buff, lastModified);
    sprintf(response_buff, "%sContent-Length: %d\r\n", response_buff, content_length);
    sprintf(response_buff, "%sContent-Type: %s\r\n\r\n", response_buff, content_type);

    fprintf(stdout, "Response...\n");
    fprintf(stdout, response_buff);

    send(fd, response_buff, strlen(response_buff), 0);

    close(fp);
    fprintf(stdout, "Sent HEAD to Client!\n");
    return 1;


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
    char timestamp[128];
    handleDate(timestamp);

    // Put together full message
    char full_message[1024];
    sprintf(full_message, "[%s]: %s\n", timestamp, message);

    // print to file
    fprintf(fp, "%s\n", full_message);

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

    // build buffer and send to client
    sprintf(ebuf, "%s %d %s\r\n", request->http_version, error_code, output);
    sprintf(ebuf, "%sServer: Liso/1.0\r\n", ebuf);
    sprintf(ebuf, "%sDate: %s\r\n", ebuf, date);
    sprintf(ebuf, "%sContent-Length: %ld\r\n", ebuf, strlen(output));
    sprintf(ebuf, "%sContent-Type: text/html\r\n\r\n", ebuf);
    send(fd, ebuf, strlen(ebuf), 0);
    fprintf(stdout, "Sent ERROR to Client!\n");
    writeLog(log_filename, "Sent ERROR Response");
}

// ------------------------------------ Get_in_addr --------------------------------------------------

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

    // *********************** Code below relies heavily on Beej's Guide **********************
    // ************** http://beej.us/guide/bgnet/output/html/multipage/index.html *************

    char* portnum = argv[1];
    log_filename = argv[3];
    sprintf(data_path, "%s","www");

    fd_set all_fds;    // master file descriptor list
    fd_set temp_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int sock;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[8192];    // buffer for client data
    int readret;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&all_fds);    // clear the master and temp sets
    FD_ZERO(&temp_fds);

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
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sock, p->ai_addr, p->ai_addrlen) < 0) {
            close(sock);
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
    if (listen(sock, 10) == -1) {
        perror("listen");
        exit(3);
    }

    fprintf(stdout, "----------Server Started---------\n");
    writeLog(log_filename, "Server Started");

    // add the listener to the master set
    FD_SET(sock, &all_fds);

    // keep track of the biggest file descriptor
    fdmax = sock; // so far, it's this one

    // main loop
    for(;;) {
        temp_fds = all_fds; // copy it
        if (select(fdmax+1, &temp_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &temp_fds)) { // we got one!!
                if (i == sock) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(sock,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &all_fds); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                                       "socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                        writeLog(log_filename, "New Client Connection");
                    }

                    // ********************* Code above relies heavily on Beej's Guide ************************
                    // ************** http://beej.us/guide/bgnet/output/html/multipage/index.html *************

                } else {
                    // handle data from a client
                    if ((readret = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (readret == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                            writeLog(log_filename, "Socket hung up");
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &all_fds); // remove from master set
                    } else {

                        // PARSE HTTP
                        fprintf(stdout, "INCOMING REQUEST...\n");
                        fprintf(stdout,buf);
                        Request *request = parse(buf,readret,i);
                        fprintf(stdout, "PARSED REQUEST...\n");
                        printf("Http Method %s\n",request->http_method);
                        printf("Http Version %s\n",request->http_version);
                        printf("Http Uri %s\n",request->http_uri);
                        int index;
                        for(index = 0;index < request->header_count;index++){
                            printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
                        }
                        fprintf(stdout, "Finished Parsing...\n\n");

                        // CHECK HTTP VERSION
                        char ver[4];
                        strncpy( ver, request->http_version+5, 3 );
                        fprintf(stdout, "VERSION = %s\n",ver);
                        if (strcasecmp(ver, "1.1")){
                            sendError(i,505,request);
                            close(i);
                            FD_CLR(i, &all_fds);
                        }

                        // FIGURE OUT WHICH METHOD TO CALL
                        int method;
                        if (!strcasecmp(request->http_method,"GET")){
                            method = 1;
                        } else if (!strcasecmp(request->http_method,"HEAD")){
                            method = 2;
                        } else if (!strcasecmp(request->http_method,"POST")){
                            method = 3;
                        }
                        fprintf(stdout, "Finished Matching...\n\n");

                        // CALL METHOD
                        int retVal;
                        switch (method) {

                            case 1:
                                if (retVal = handleGET(i, request) < 0){
                                    close(i);
                                    FD_CLR(i, &all_fds);
                                };
                                writeLog(log_filename, "Sent GET Response");
                                break;

                            case 2:
                                fprintf(stdout, "HEAD...\n\n");
                                if (retVal = handleHEAD(i, request) < 0){
                                    close(i);
                                    FD_CLR(i, &all_fds);
                                };
                                writeLog(log_filename, "Sent HEAD Response");
                                break;

                            case 3:
                                fprintf(stdout, "POST...\n\n");
                                handlePOST(i);
                                writeLog(log_filename, "Sent POST Response");
                                break;

                            default:
                                sendError(i,501,request);
                                fprintf(stdout, "Missed Match\n");
                                break;

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
