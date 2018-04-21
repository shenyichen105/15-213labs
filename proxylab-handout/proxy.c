#include <stdio.h>
#include "csapp.h"


/*

proxy server with LRU based caching
passed autograding

Future improvement:

1. now the single semaphore is implemented for the entire cache as reader-fist.
So it will block every thread from the entire cache when any thread is
trying to read and write. Maybe we can implement one semaphare per cached page to
improve cocurrency.
Also we can try writer first style.

2. Some system calls of cache operation methods needs to edited to prevent exiting the server
process upon failing

*/

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1024000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

typedef struct cache_object{
    int size;
    struct cache_object* prev;
    struct cache_object* next;
    char* requestbuf;
    char* responsebuf;
} cache_object;

typedef struct {
    int total_size;
    int read_cnt;
    sem_t mutex;
    sem_t w;
    cache_object* head;
    cache_object* tail;
} cache_t;

//declare meta data for cache as global
static cache_t* meta_cache;

/*initialize meta cache*/
void meta_cache_init(cache_t* cm){
    cm->total_size = 0;
    cm->read_cnt= 0;
    cm->head = NULL;
    cm->tail = NULL;
    Sem_init(&cm->mutex,0, 1);
    Sem_init(&cm->w,0, 1);
}

/*initialize a cache object for web page*/
void cache_object_init(cache_object* co, char* requestbuf, char* responsebuf, size_t size){
    co->size = size;
    co->prev = NULL;
    co->next = NULL;
    co->requestbuf = requestbuf;
    co->responsebuf = responsebuf;
}

/*recursively free everything for a cache block*/
void Free_cache(cache_object* co){
    /*need to destroy semiphore*/

    Free(co->requestbuf);
    Free(co->responsebuf);
    Free(co);
}

/*evict least recent used cache*/
void cache_evict(cache_t* meta_cache){
    cache_object* prev_tail;
    //remove the tail the queue (least recent policy)
    prev_tail = meta_cache->tail;

    if (prev_tail){
        if (prev_tail->prev){
            //if there is more than one cache block
            prev_tail->prev->next = NULL;
            meta_cache->tail = prev_tail->prev;
        }else{
            meta_cache->head = NULL;
            meta_cache->tail = NULL;
        }
        meta_cache->total_size -= prev_tail->size;
        Free_cache(prev_tail);
    }
}

//add the new object to the head of queue;
void cache_add_head(cache_t* meta_cache, cache_object* co){
    cache_object* old_head;

    old_head = meta_cache->head;
    meta_cache->head = co;

    if (old_head){
        co->next = old_head;
        old_head->prev = co;
    }else{
        meta_cache->tail = co;
    }
    co->prev = NULL;
}

/*add a new cache block*/
void cache_add(cache_t* meta_cache, cache_object* co){
    /*if there is no space, then evict till open space*/
    P(&meta_cache->w);
    while((meta_cache->total_size) + co->size > MAX_CACHE_SIZE){
        cache_evict(meta_cache);
    }
    cache_add_head(meta_cache, co);
    meta_cache->total_size += co->size;
    V(&meta_cache->w);
}

/*has to be thread safe, reader first style*/
/*move a cache to head of queue */
void cache_promote(cache_t* meta_cache, cache_object* co){
    cache_object* old_next;
    cache_object* old_prev;

    P(&meta_cache->w);
    if (!co->prev){
        V(&meta_cache->w);
        return;
    }
    //delete the object in the queue
    old_prev= co->prev;
    old_next = co->next;
    old_prev->next = old_next;

    if (old_next){
        old_next->prev = old_prev;
    }else{
        meta_cache->tail = old_prev;
    }
    //place the new object to the tail of queue;
    cache_add_head(meta_cache, co);

    V(&meta_cache->w);
}

/*has to be thread safe, reader first style*/
/* fetch a cache by matching the client request, if not found, return NULL */
cache_object* cache_fetch(cache_t* meta_cache, char* client_request){

    cache_object* cur_cache;
    cache_object* found_cache;

    P(&meta_cache->mutex);
    meta_cache->read_cnt++;
    if (meta_cache->read_cnt == 1){
    /* First in */
        P(&meta_cache->w);
    }
    V(&meta_cache->mutex);

    cur_cache = meta_cache->head;
    found_cache = NULL;

    while (cur_cache){
        if (strcasecmp(cur_cache->requestbuf, client_request)==0){
            //found
            found_cache = cur_cache;
            break;
        }
        cur_cache = cur_cache -> next;
    }

    P(&meta_cache->mutex);
    meta_cache->read_cnt--;
    if (meta_cache->read_cnt == 0){
    /* Last out */
        V(&meta_cache->w);
    }
    V(&meta_cache->mutex);

    //if cache hit then move the cache object to the head of queue
    if (found_cache){
        cache_promote(meta_cache, found_cache);
    }
    return found_cache;
}

/* write html error page to client if anything goes wrong in the middle*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){

    char buf[MAXLINE], body[MAXBUF];
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The proxy server</em>\r\n", body);
    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_server_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_server_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_server_writen(fd, buf, strlen(buf));
    Rio_server_writen(fd, body, strlen(body));
}




/* parse all the http request text*/
int parse_request_headers(char* buf, char* headername, char* headerdata){
    char* p;
    //check if there is
    if ((p = strstr(buf, ": "))){
        memcpy(headername, buf, p-buf);
        headername[p-buf] = '\0';
        strcpy(headerdata, p+2);
        return 0;
    }else{
        return 1;
    }
}


/* read and parse all the headers from client http request.*/
int process_request_headers(char* buf, char* headername, char* headerdata, char* hostname, int clientfd, char* requestbuf, rio_t* rio_p){

    int host_flag = 0;
    if (Rio_server_readlineb(rio_p, buf, MAXLINE) < 0){
        clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server write Error, failed to read request header from client server:");
        return -1;
    }

    while (strcmp(buf, "\r\n")){
        if (parse_request_headers(buf, headername, headerdata)){
            clienterror(clientfd, "", "400", "Bad Request",
                            "the request has an invalid header");
            return -1;
        }

        //headers that need to be rewritten
        if (!strcasecmp(headername, "User-Agent") ||
            !strcasecmp(headername, "Connection") ||
            !strcasecmp(headername, "Proxy-Connection")){

            if (Rio_server_readlineb(rio_p, buf, MAXLINE) < 0){
                clienterror(clientfd, "", "500", "Internal Server error",
                "Proxy server write Error, failed to read request header from client server:");
                return -1;
            }

            continue;
        }

        if (!strcasecmp(headername, "host")){
            host_flag = 1;
        }
        //forward the headers to the remote server
        sprintf(requestbuf, "%s%s", requestbuf, buf);

        if (Rio_server_readlineb(rio_p, buf, MAXLINE) < 0){
            clienterror(clientfd, "", "500", "Internal Server error",
            "Proxy server write Error, cannot read request header from client server:");
            return -1;
        }
    }

    //forward the customized headers to the remote server
    sprintf(requestbuf, "%s%s",  requestbuf, user_agent_hdr);
    sprintf(requestbuf, "%s%s: %s\r\n",  requestbuf, "Connection", "Close");
    sprintf(requestbuf, "%s%s: %s\r\n",  requestbuf, "Proxy-Connection", "Close");

    if (!host_flag){
        sprintf(requestbuf, "%s%s: %s\r\n",  requestbuf, "Host", hostname);
    }
    return 0;
}

/* read and parse all the headers from server http response. return content length*/
int parse_response_headers(char* buf, char* responsebuf, char* headername, char* headerdata, int clientfd, rio_t* rio_p){

    if (Rio_server_readlineb(rio_p, buf, MAXLINE)<0){
        clienterror(clientfd, buf, "500", "Internal Server error",
                            "Proxy server write Error, failed to read request header from remote server:");
        return -1;
    }

    responsebuf = strcat(responsebuf, buf);

    int content_length = 0;

    while (strcmp(buf, "\r\n")){
        if (parse_request_headers(buf, headername, headerdata)){
            return -1;
        }
        //ignore everything except
        if (!strcasecmp(headername, "Content-length")){
            content_length = atoi(headerdata);
        }
        //forward the headers to the remote server
        if (Rio_server_writen(clientfd, buf, strlen(buf)) <0){
            clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server write Error, failed to write request header to client server:");
            return -1;
        }

        if (Rio_server_readlineb(rio_p, buf, MAXLINE)<0){
            clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server write Error, failed to read request header from remote server:");
            return -1;
        }
        responsebuf = strcat(responsebuf, buf);
    }
    //write the empty line
    if (Rio_server_writen(clientfd, buf, strlen(buf))<0){
        clienterror(clientfd, buf, "", "Internal Server error",
                            "Proxy server write Error, failed to write request beader to client server:");
        return -1;
    }

    return content_length;
}


/* parse the uri into host name and path*/
int uri_parser(char* uri, char* hostname, char* port, char* path){
    //----need to implement----
    char *temp_body;
    char header[MAXLINE];
    char *leftover;
    char *p, *q, *r;
    int offset;

    //ignore the  header
    if ((p = strstr(uri, "//"))){
        offset = p - uri;
        memcpy(header, uri, offset);
        header[offset] = '\0';
        // if not http header, then return error
        if (strcmp(header, "http:")){
            return -1;
        }
        temp_body = p + 2;
    }else{
        temp_body = uri;
    }
    //get the hostname and path
    if ((q = strchr(temp_body, '/'))){
        offset = q - temp_body;
        memcpy(hostname, temp_body, offset);
        hostname[offset]= '\0';
        //assign left over to
        leftover = q + 1;
        if (strlen(leftover) > 0){
            strcpy(path+1, leftover);
        }
    }else{
        strcpy(hostname, temp_body);
    }
    //get port number if any, default 90
    if ((r = strstr(hostname, ":")) && (*(r+1) != '\0')){
        *r= '\0';
        strcpy(port, r+1);
    }else{
        strcpy(port, "80");
    }
    return 0;
}

/*check if http version is a valid one */
int http_version_checker(char* version){
    //if no version is specified then forward default version HTTP/1.0
    if (strlen(version)== 0){
        strcpy(version, "HTTP/1.0");
        return 0;
    }else{
        //check if the version is valid
        if (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1")) {
            return -1;
        }
        return 0;
    }
}

/* process the request and open an connection to the host server*/
int process_request_body(char* method, char* uri, char* version, char* hostname, char* port, char* requestbuf, int clientfd){
    char path[MAXLINE];
    //char stderr_buff[MAXBUF];

    //assign default value
    hostname[0] = '\0';
    port[0] = '\0';
    strcpy(path, "/");

    //parse input
    if (uri_parser(uri, hostname, port, path)){
        clienterror(clientfd, uri, "400", "Bad request",
                    "Proxy does not support non-http request");
        return -1;
    }
    //open a new connection
    //setbuf(stderr, stderr_buf);

    if (http_version_checker(version)){
        clienterror(clientfd, version, "400", "Bad request",
                            "Your client has issued a malformed or illegal request");
        return -1;
    }
    //forward the request body to the remote server
    sprintf(requestbuf, "%s %s %s\r\n", method, path, version);
    return 0;
}

/*utility function appending bytes from a buffer to some exiting larger buffer*/

char* memcat(char* dest, size_t *destsize, const char* src, const size_t src_size){

    char* attach_point;
    attach_point = dest + (*destsize);
    memcpy(attach_point, src, src_size);
    (*destsize) += src_size;
    return dest;
}


/* read the response from the remote server and write exactly back to the client*/

int forward_server_response(char* requestbuf, char* headername, char* headerdata, int clientfd, int forwardfd){

    rio_t rio;
    rio_readinitb(&rio, forwardfd);
    int rc;
    int content_length, remaining;
    char buf[MAXLINE];

    char* responsebuf;
    size_t bufsize;

    //writing to a new big response buf for cache
    if (!(responsebuf= (char*)Malloc(MAX_OBJECT_SIZE*sizeof(char)))){
        clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server Malloc Error");
        return -1;
    }

    /*forward response status as is*/
    if (Rio_server_readlineb(&rio, buf, MAXLINE) <0){
        clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server read Error, failed to read response from remote server");
        return -1;
    }

    responsebuf = strcat(responsebuf, buf);

    if (Rio_server_writen(clientfd, buf, strlen(buf))<0){
        clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server write Error, failed to write request to client server");
        return -1;
    }

    /*parse remaining header, get content length*/
    if ((content_length = parse_response_headers(buf, responsebuf, headername, headerdata, clientfd, &rio))<0){
        clienterror(clientfd, "", "400", "Bad Response",
                            "the remote server sent a bad http response");
        return -1;
    }

    //since header is a string so can use string method to determine length.
    bufsize += strlen(responsebuf);

    remaining = content_length;

    while (remaining > 0){
        if (remaining < MAXLINE){
            rc = Rio_readnb(&rio, buf, remaining);
            //cant use strcat here as the content might not be text/string
            responsebuf = memcat(responsebuf, &bufsize, buf, remaining);

        }else{
            rc = Rio_readnb(&rio, buf, MAXLINE);
            //cant use strcat here as the content might not be text/string
            responsebuf = memcat(responsebuf, &bufsize, buf, MAXLINE);
        }


        if (rc <0){
            return -1;
        }

        remaining = remaining - rc;

        if (rc >0){
            if (Rio_server_writen(clientfd, buf, rc) < 0){
                return -1;
            }
            printf("%s\n", buf);
            printf("%d\n", remaining);
        }
    }

    /*add the response to the cache if possible; (+1) for possible null teminator*/

    int total_size;
    total_size = bufsize + 1;

    if (total_size > MAX_OBJECT_SIZE){
        //dont add cache, then just free request and response
        Free(responsebuf);
        Free(requestbuf);
    }else{
        //add cache, save request and response


        if (!(responsebuf = (char*) Realloc(responsebuf, sizeof(char)*(total_size)))){
            clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server Realloc Error");
            return -1;
        }

        cache_object* co;
        if (!(co = (cache_object*) Malloc(sizeof(cache_object)))){
            clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server Malloc Error");
            return -1;

        }
        cache_object_init(co, requestbuf, responsebuf, total_size);
        cache_add(meta_cache, co);
    }

    return 0;
}






/*main rountine for the server*/
void main_rountine(int clientfd){

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE];
    char headername[MAXLINE], headerdata[MAXLINE];

    char* requestbuf;
    if (!(requestbuf= (char*) Calloc(MAXBUF, MAXBUF*sizeof(char)))){
        clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server Calloc Error");
        return;
    }


    int  forwardfd;

    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, clientfd);

    if (Rio_server_readlineb(&rio, buf, MAXLINE) <0){
        clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server read Error, failed to read request from client server:");
        return;
    }

    /* read body */
    sscanf(buf, "%s %s %s", method, uri, version);


    if (strcasecmp(method, "GET")) {
        clienterror(clientfd, method, "501", "Not Implemented",
                            "Proxy does not implement this method");
        return;
    }

    /* process request bodies */
    if (process_request_body(method, uri, version, hostname, port, requestbuf, clientfd) == -1){
        return;
    }

    /* process headers */
    if (process_request_headers(buf, headername, headerdata, hostname, clientfd, requestbuf, &rio)== -1){
        return;
    }

    sprintf(requestbuf, "%s\r\n", requestbuf);
    requestbuf = (char*) Realloc(requestbuf, sizeof(char)*((strlen(requestbuf)+1)));

    /*search at cache first*/
    cache_object* cached_response;
    cached_response = cache_fetch(meta_cache, requestbuf);

    /*if cache hits ........*/
    if (cached_response){
        if (Rio_server_writen(clientfd, cached_response->responsebuf, cached_response->size)<0){
            clienterror(clientfd, "", "500", "Internal Server error",
                            "Proxy server write Error, failed to write request to client server:");
            return;
        }
        return;
    }

    /* if cache miss........*/
    if ((forwardfd = Open_clientfd(hostname , port)) < 0){
        clienterror(clientfd, hostname, "400", "Bad request",
                            "Cannot establish connection with remote server using this hostname");
        return ;
    }

    if (Rio_server_writen(forwardfd, requestbuf, strlen(requestbuf))<0){
        clienterror(clientfd, requestbuf, "500", "Internal Server error",
                            "Proxy server write Error, cannot write request to remote server:");
        Close(forwardfd);
        return;
    }
    if (forward_server_response(requestbuf, headername, headerdata, clientfd, forwardfd) == -1){
        Close(forwardfd);
        return;
    }
    Close(forwardfd);
    return;
}


/* Thread routine */
void *thread(void *vargp){

    int clientfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    main_rountine(clientfd);
    Close(clientfd);
    return NULL;
}



int main(int argc, char **argv)
{
    char* port;
    int listenfd, *clientfdp;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    /* ignore sig pipe*/
    signal(SIGPIPE, SIG_IGN);
    pthread_t tid;

    /*initialize the cache*/
    meta_cache = (cache_t*) Malloc(sizeof(cache_t));
    meta_cache_init(meta_cache);

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = argv[1];
    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        if (!(clientfdp = Malloc(sizeof(int)))){
            continue;
        }
        if ((*clientfdp= Accept(listenfd, (SA *)&clientaddr, &clientlen)) == -1){
            continue;
        }
        /* spawning a thread to deal with the client connection*/
        if (Pthread_server_create(&tid, NULL, thread, clientfdp)){
            continue;
        }
    }

    free(meta_cache);
    return 0;
}