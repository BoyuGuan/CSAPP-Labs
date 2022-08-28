/*
 * @Author: Jack Guan cnboyuguan@gmail.com
 * @Date: 2022-08-15 00:43:27
 * @LastEditors: Jack Guan cnboyuguan@gmail.com
 * @LastEditTime: 2022-08-28 20:53:32
 * @FilePath: /guan/CSAPP/CSAPP-Labs/labs/proxylab/proxy.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
// #include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
static const char *connection_hdr = "Connection: close";
static const char *proxy_connection_hdr = "Proxy-Connection: close";


void handleClient(int connectFd);
void readHeaders(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *threadHandle(void *vargp);


int main(int argc, char **argv)
{
    int listenFd, *connectFdp;
    char clientHostName[512], clientPort[8]; 
    pthread_t pid;
    // check form
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    

    listenFd = Open_listenfd(argv[1]);
    
    socklen_t clientLen;
    struct sockaddr_storage clientAddr;
    while (1)
    {
        clientLen = sizeof(clientAddr);
        connectFdp = malloc(sizeof(int));
        *connectFdp = Accept(listenFd, (SA *)& clientAddr, &clientLen);
        Getnameinfo((SA *)&clientAddr, clientLen, clientHostName, 512, clientPort, 8, 0);
        // printf("Accept connection from (%s,%s)\n", clientHostName, clientPort);
        // handleClient(connectFd);
        Pthread_create(&pid, NULL, threadHandle, connectFdp);
    }
    
    // printf("%s", user_agent_hdr);
    return 0;
}

void *threadHandle(void *vargp){
    
    Pthread_detach(pthread_self());
    printf("\n########### new proxy########################\n\n");
    printf("thread id %lu is handling the request\n", pthread_self());

    int connectFd = *(int *)vargp;
    int i, n;
    char buf[MAXLINE], contentProxyToServer[MAXLINE], request[MAXLINE], contentBackToClinet[MAXLINE];
    char method[32], uri[1024], httpVersion[64], targetHostNameAndPort[128], fileName[128];
    strcpy(fileName, "");


    rio_t clientRio, targetRio;
    Rio_readinitb(&clientRio, connectFd);
    if (!Rio_readlineb(&clientRio, request, MAXLINE))  // GET requests
        return; // empty requests
    printf("%s", request);
    readHeaders(&clientRio);

    sscanf(request, "%s %s %s", method, uri, httpVersion);
    if (strcasecmp(method, "GET"))
    {
        printf("Proxy does not implement the method");
        return;
    }

    strcpy(httpVersion, "HTTP/1.0");
    char *p = strstr(uri, "http://");
    if (!p) {                     // if "http://" not in uri
        p = uri;
    } else{
        p += 7;         // p skip "http://" to host name
    }
    i = 0;
    while (*p != '/')
    {
        targetHostNameAndPort[i++] = *p;
        p += 1;
    }
    targetHostNameAndPort[i] = '\0';
    // finish the content from proxy to target server
    strcpy(fileName, p);
    sprintf(contentProxyToServer, "%s %s %s\r\n", method, fileName, httpVersion);
    sprintf(contentProxyToServer, "%sHost: %s\r\n", contentProxyToServer,targetHostNameAndPort);
    sprintf(contentProxyToServer, "%s%s\r\n", contentProxyToServer, user_agent_hdr);
    sprintf(contentProxyToServer, "%s%s\r\n", contentProxyToServer, connection_hdr);
    sprintf(contentProxyToServer, "%s%s\r\n\r\n", contentProxyToServer, proxy_connection_hdr);
    char targetServerName[64], targetPort[16];
    p = strstr(targetHostNameAndPort, ":");
    if(!p){
        // no port specifiction, then choose http port 80
        strcpy(targetServerName, targetHostNameAndPort);
        strcpy(targetPort, "80");
    }
    else{
        strcpy(targetPort, p+1);
        for ( i = 0; targetHostNameAndPort[i] != ':'; i++)
        {
            targetServerName[i] = targetHostNameAndPort[i];
        }
        targetServerName[i] = '\0';
    }

    printf("open a connection to target\ncontent is\n%s", contentProxyToServer);
    // open a connect from proxy to target
    int connectToTargetFd = Open_clientfd(targetServerName, targetPort);
    if (connectToTargetFd < 0){  // connect to server 
        clienterror(connectFd, method, "404", "connect failure", 
        "the proxy server connect to target server failure");
        return;
    }
    
    Rio_writen(connectToTargetFd, contentProxyToServer, strlen(contentProxyToServer));
    Rio_readinitb(&targetRio, connectToTargetFd);

    strcpy(contentBackToClinet, ""); // 先将其设置为空，避免出现之前的缓存没有分配的情况
    while( (n = Rio_readlineb(&targetRio, buf, MAXLINE)) != 0 ){
        // strcat(contentBackToClinet, buf);
        Rio_writen(connectFd, buf, n);

    }
    // printf("\ncontent back to client is: \n%s", contentBackToClinet);
    Close(connectToTargetFd);
    Close(connectFd);
    Free(vargp);
    return NULL;
    
}

void readHeaders(rio_t *rp){
    // char buf[MAXLINE];
    // Rio_readlineb(rp, buf, MAXLINE);
    // printf("%s", buf);
    // while (strcmp(buf, "\r\n"))
    // {
    //     Rio_readlineb(rp, buf, MAXLINE);
    //     printf("%s", buf);
    // }
    return ;   
}


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