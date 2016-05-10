#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_hdr = "Proxy-connection: close\r\n";
static const char *line_end = "\r\n";

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void* thread_func(void* argp);
int parse_uri(char* uri, char* host, char* path, char* port);

int main(int argc, char **argv)
{
    /* Init variables */
    int listenfd, *connfd;
    socklen_t clientlen;
    pthread_t tid;
    struct sockaddr_in clientaddr;


    /* Command line check */
    if (argc != 2) fprintf(stderr, "Proxy usage: %s <port>\n", argv[0]);


    /* Block pipe */
    Signal(SIGPIPE, SIG_IGN);


    /* Open connection */
    if ((listenfd = Open_listenfd(argv[1])) < 0) fprintf(stderr, "Listen error");

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));

        /* Accept connection */
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* Serve request in new thread */
        Pthread_create(&tid, NULL, thread_func, connfd);
    }

    return 0;
}

/* void* thread_func - responsible for serving client request
 * e.g. : GET http://www.cmu.edu:8080/hub/index.html HTTP/1.1
 * Method: GET
 * Host: www.cmu.edu
 * Port: 8080
 * Path: /hub/index.html
 * Version: HTTP/1.1 (HTTP/1.0)
 */

void* thread_func(void* argp)
{
    /* Init variables */
    char host[MAXLINE], path[MAXLINE], method[MAXLINE], port[MAXLINE], version[MAXLINE], URL[MAXLINE], usrbuf[MAXLINE], request[MAXLINE], buf[MAXLINE];
    char scanreq[MAXLINE], miscreq[MAXLINE], miscdata[MAXLINE];
    int proxy, response;
    int hostbool = 0;
    int client = *((int *)argp);
    rio_t rio_c, rio_s;

    /* First reap thread & free */
    Pthread_detach(Pthread_self());
    Free(argp);

    /* Begin reading client request */
    Rio_readinitb(&rio_c, client);

    /* Read & parse request */
    Rio_readlineb(&rio_c, usrbuf, MAXLINE);
    sscanf(usrbuf, "%s %s %s", method, URL, version);
    parse_uri(URL, host, port, path);

    sprintf(request, "GET %s HTTP/1.0\r\n", path);

    /* Begin reading client request headers */
    while (!strcmp(scanreq, "\r\n")) {
        Rio_readlineb(&rio_c, scanreq, MAXLINE);
        sscanf(scanreq, "%s %s", miscreq, miscdata);

        /* Set flag if client provides host */
        if (!strcmp(miscreq, "Host:")) {
            strcat(request, scanreq);
            hostbool = 1;
        }
        /* Block all other headers */
        if (strcmp(miscreq, "User-Agent:") &&
        strcmp(miscreq, "Connection:") &&
        strcmp(miscreq, "Proxy-Connection:"))
        strcat(request, scanreq);
    }

    /* Prepare headers to send */
    if (hostbool == 0) {
        /* No host provided */
        sprintf(request, "%sHost: %s\r\n", request, host);
    }
    strcat(request, user_agent_hdr);
    strcat(request, connection_hdr);
    strcat(request, proxy_hdr);
    strcat(request, line_end);


    /* Open connection & write requests */
    proxy = Open_clientfd(host, port);
    Rio_readinitb(&rio_s, proxy);

    Rio_writen(proxy, request, strlen(request));


    /* Write response back to client */
    while ((response = Rio_readnb(&rio_s, buf, MAXBUF)) > 0) {
        Rio_writen(client, buf, response);
    }


    /* Close proxy socket */
    Close(proxy);

    /* Close connection */
    Close(client);

    return 0;
}



/* parse_uri - Parse URL -> host, port, path
 * Called: parse_uri(URL, host, port, path);
 * URL: http://HOST.COM:PORT/FILENAME.ext
 * Host: HOST.COM, Port: PORT, Path: /FILENAME.ext
 */

int parse_uri(char* url, char* hostname, char* port, char* path)
{
    /* Init variables */
    char* port_start, *path_start;
    int port_length;

    /* Host start: url + 7 */
    port_start = strchr(url + 7, ':');
    path_start = strchr(url + 7, '/');

    /* No port specified */
    if (port_start == NULL) {
        strcpy(port, "80");
        strncpy(hostname, url + 7, strlen(url) - strlen(path_start) - 7);
        strncpy(path, path_start, strlen(url) - strlen(path_start));
    }
    else if (path_start == NULL) {
        strcpy(port, "80");
        strncpy(hostname, url + 7,strlen(url) - 7);
        strcpy(path, "/");
    }
    /* Port specified - DEFAULT 80 */
    else if (strlen(path_start) < strlen(port_start)) {
        port_length = strlen(port_start) - strlen(path_start) - 1;
        strncpy(port, port_start + 1, port_length);
        strncpy(hostname, url + 7, strlen(url) - strlen(port_start) - 7);
        strncpy(path, path_start, strlen(url) - strlen(path_start));
    }


    return 0;
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
