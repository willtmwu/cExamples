// don't forget to have a dependencies folder with http-parser in it. In the same directory
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <event.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <time.h>

#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>

#include "dependencies/http-parser/http_parser.h"

#define MSG_BUFF_LEN	1024

#define SERVER_ID 			"Server: mirrord/s4291330\r\n"
#define SERVER_CONNECTION 	"Connection: close\r\n"

typedef enum {
	HTTP_200_OK,
	HTTP_400_BAD,
	HTTP_403_FORBIDDEN,
	HTTP_404_NOT_FOUND,
	HTTP_405_METHOD,
	HTTP_444_NO_RESPONSE,
	HTTP_500_SERVER
} response_type;

typedef struct {
	int ai_family;
	char* address;
	char* port;
	int can_daemonise;
	FILE *access_log;
	char* directory;
} webserver_initialisation;

typedef struct {
	int ipv4_fd;
	int ipv6_fd;
} connection_fd;

typedef struct {
	char* remote_addr;
	char *file_path;
	int request_method;
	char* req_date; 
	int complete;	
} request_details;

typedef struct {
	response_type response_enum;
	size_t content_length;
	char* last_modified;
} response_details;

typedef struct {
	struct event		rd_ev;
	struct event		wr_ev;
	struct evbuffer		*req_buf;
	struct evbuffer		*res_buf;

	int has_daemonise;
	FILE *access_log;
	char* directory;
	
	http_parser_settings parser_settings;
	http_parser *parser;
	
	request_details req_data;
	response_details res_data;
} webserver_connection_stream;

// Initialisation parameters
int webserver_init(webserver_initialisation*, connection_fd*);
int parse_params(int, char**,webserver_initialisation*);
__dead void usage(void);

// Event callback functions
void webserver_accept(int, short, void *);
void webserver_read(int, short, void *);
void webserver_write(int, short, void *);
void webserver_close(webserver_connection_stream *);

// http callback functions
int webserver_url_callback(http_parser*, const char*, size_t);
int webserver_headers_complete_callback (http_parser*);

// Helper functions
int check_stream_memory(webserver_connection_stream*);
int build_response_message(struct evbuffer*, webserver_connection_stream*);
int build_err_response_message(struct evbuffer*, webserver_connection_stream*);
int check_invalid_filepath(int, char**, char*);
int log_access(webserver_connection_stream*);

int 
main(int argc, char** argv)
{
	struct event event_ipv4;
	struct event event_ipv6;
	
	webserver_initialisation* server_config;
	connection_fd* connection_details;
	
	server_config = malloc(sizeof(*server_config));
	connection_details = malloc(sizeof(*connection_details));
	
	parse_params(argc, argv, server_config);		
	webserver_init(server_config, connection_details); 

	if(server_config->can_daemonise){
		if(daemon(1,0) == 0){
			#ifdef DEBUG
			printf("\nProgram has become a daemon\n");
			#endif
		}else{
			perror("Error [unable to daemonise]");
			exit(1);
		}
	}

	#ifdef DEBUG
	printf("Accepting with IPv4[%d] and IPv6[%d]\n", 
		connection_details->ipv4_fd, connection_details->ipv6_fd);
	fflush(stdout);
	#endif
	
	event_init();
	if(connection_details->ipv4_fd>0) { 
		event_set(&event_ipv4, connection_details->ipv4_fd, 
			EV_READ | EV_PERSIST, webserver_accept, server_config); 
		event_add(&event_ipv4, NULL);
	}
	
	if(connection_details->ipv6_fd>0) { 
		event_set(&event_ipv6, connection_details->ipv6_fd, 
			EV_READ | EV_PERSIST, webserver_accept, server_config); 
		event_add(&event_ipv6, NULL);
	}
	
	event_dispatch();	
	return 0;
}

/*
* Use the passed in server_config information to generate socket file descriptors 
*/
int 
webserver_init(webserver_initialisation* server_config, connection_fd* connection_details)
{
	struct addrinfo hints, *res_itr, *res;
	int errno, fd, optVal;
	optVal = 1;
	
	// Set default value at -2, to allow error checking for returning -1 from socket functions
	connection_details->ipv4_fd=-2;
	connection_details->ipv6_fd=-2;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family 	= server_config->ai_family;
    hints.ai_socktype 	= SOCK_STREAM;
    hints.ai_protocol 	= IPPROTO_TCP;
    hints.ai_flags 		= AI_PASSIVE | AI_ADDRCONFIG;
	
	if((errno = getaddrinfo(server_config->address, server_config->port, &hints, &res)) != 0){
		errx(1, "%s", gai_strerror(errno)); 
        exit(1);
    }
		
	fd = -1;
	for(res_itr = res; res_itr!=NULL; res_itr = res_itr->ai_next){
		fd = socket(res_itr->ai_family, res_itr->ai_socktype, res_itr->ai_protocol); 
		
		if(fd == -1){ 
			perror("Error [socket]");
			continue;
		}
		
		if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal,sizeof(int)) == -1){
            perror("Error [setsockopt]");
            exit(1);
        }

        if(bind(fd, res_itr->ai_addr, res_itr->ai_addrlen) == -1){
            close(fd);
            perror("Error [bind]");
            continue;
        } else {
			if(res_itr->ai_family == AF_INET){
				connection_details->ipv4_fd=fd;
			}
		
			if(res_itr->ai_family == AF_INET6){
				connection_details->ipv6_fd=fd;
			}	
		}	
	}
	freeaddrinfo(res); 

	if(connection_details->ipv4_fd == -1){
        perror("Error [IPv4 bind]");
        exit(1);
    }
	
	if(connection_details->ipv6_fd == -1){
        perror("Error [IPv4 bind]");
        exit(1);
    }
   
    if(listen(connection_details->ipv4_fd, SOMAXCONN) == -1){
        perror("Error [IPv4 listen]");
        exit(1);
    }

	if(listen(connection_details->ipv6_fd, SOMAXCONN) == -1){
        perror("Error [IPv6 listen]");
        exit(1);
    }
	
	return fd;
}

void 
webserver_accept(int sockfd, short events, void *info)
{
	int fd;
	char numeric_addr[INET6_ADDRSTRLEN];
	const char *remote_addr_name;
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	
	webserver_connection_stream* client_stream;	
	webserver_initialisation* server_config = (webserver_initialisation*) info;
		
	fd = accept(sockfd,(struct sockaddr*)&client_addr,&addr_len);
	if(fd == -1){
		switch(errno){
			case EINTR:
			case EWOULDBLOCK:
			case ECONNABORTED:
				return;
			default:
				perror("Error [accept connection]");
				exit(1);
		}
	}
	
	client_stream = malloc(sizeof(*client_stream));
	if(client_stream == NULL){
		warn("Unable to allocate memory for a client");
		close(fd);
		return;
	}
	
	client_stream->req_buf = evbuffer_new();
	client_stream->res_buf = evbuffer_new();
	client_stream->parser = malloc(sizeof(http_parser));
	(client_stream->req_data).remote_addr = malloc(sizeof(char)*(INET6_ADDRSTRLEN+1)); 
	(client_stream->req_data).req_date = malloc(sizeof(char)*51);
	(client_stream->res_data).last_modified = malloc(sizeof(char)*51);
	
	if(check_stream_memory(client_stream)){
		warn("Unable to allocate memory for a client");
		close(fd);
		return;
	}
	
	if(client_addr.ss_family == AF_INET){
		remote_addr_name = inet_ntop(client_addr.ss_family, 
			&((struct sockaddr_in*)&client_addr)->sin_addr.s_addr,numeric_addr,sizeof(numeric_addr));
		#ifdef DEBUG
		printf("IPv4 client from %s\n", remote_addr_name);
		#endif
	}else if(client_addr.ss_family == AF_INET6){
		remote_addr_name = inet_ntop(client_addr.ss_family, 
			&((struct sockaddr_in6*)&client_addr)->sin6_addr,numeric_addr,sizeof(numeric_addr));
		#ifdef DEBUG
		printf("IPv6 client from %s\n", remote_addr_name);	
		#endif
	}
	memcpy((client_stream->req_data).remote_addr, 
		(char*) remote_addr_name, strlen(remote_addr_name));
	(client_stream->req_data).remote_addr[strlen(remote_addr_name)] = '\0';
		
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	strftime((client_stream->req_data).req_date, 50, "%a, %d %b %Y %T %Z", t);
	
	// Copy over some of the server_config parameters
	client_stream->has_daemonise = server_config->can_daemonise;
	client_stream->access_log = server_config->access_log;
	client_stream->directory = server_config->directory;
	
	// Setting up for http-parsing
	http_parser_settings_init(&client_stream->parser_settings);
	client_stream->parser_settings.on_url = &webserver_url_callback;
	client_stream->parser_settings.on_headers_complete = &webserver_headers_complete_callback;
		
	http_parser_init(client_stream->parser, HTTP_REQUEST);
	(client_stream->parser)->data = &(client_stream->req_data);
	(client_stream->req_data).complete = 0;
	(client_stream->req_data).request_method = -1;
	(client_stream->req_data).file_path = NULL;
	(client_stream->res_data).content_length = 0;	
	
	event_set(&client_stream->rd_ev, fd, EV_READ | EV_PERSIST, webserver_read, client_stream);	
	event_set(&client_stream->wr_ev, fd, EV_WRITE, webserver_write, client_stream);
	event_add(&client_stream->rd_ev, NULL);
}

void 
webserver_read(int fd, short events, void *stream)
{
	#ifdef DEBUG
	printf("Accepting request from a client...\n");
	#endif
	int status;
	char msg_buf[MSG_BUFF_LEN+1];
	size_t recv_len, nparsed;
	
	webserver_connection_stream* client_stream = (webserver_connection_stream*) stream;
	status = evbuffer_read(client_stream->req_buf, fd, MSG_BUFF_LEN);
	
	switch(status){
		case -1:
			switch(errno){
				case EINTR:
				case EAGAIN:
					return;
				case ECONNRESET:
					break;
				default:
					warn("[Client reading]");
					break;
			}
			/* FALLTHROUGH */
		case 0:
			(client_stream->res_data).response_enum = HTTP_444_NO_RESPONSE;
			log_access(stream);
			webserver_close(client_stream);
			return;
		default:
			memset(msg_buf, 0, sizeof(char)*MSG_BUFF_LEN);
			recv_len = sizeof(char)*status;
			
			evbuffer_remove(client_stream->req_buf, msg_buf, recv_len);
			msg_buf[status] = 0;
			
			nparsed = http_parser_execute(client_stream->parser, 
				&(client_stream->parser_settings), msg_buf, recv_len);
					
			if(nparsed != recv_len && !(client_stream->req_data).complete){
				if((client_stream->req_data).request_method == -1){
					#ifdef DEBUG
					printf("HTTP BAD REQUEST FORMAT\n");
					fflush(stdout);
					#endif
					(client_stream->res_data).response_enum = HTTP_400_BAD;
				}else{
					#ifdef DEBUG
					printf("INVALID HTTP METHOD [%s]\n", 
						http_method_str((client_stream->req_data).request_method));
					fflush(stdout);
					#endif
					(client_stream->res_data).response_enum = HTTP_405_METHOD;
				}
				
				build_err_response_message(client_stream->res_buf, client_stream);
				event_add(&client_stream->wr_ev, NULL);
			}else{
				if((client_stream->req_data).complete == 1){
					build_response_message(client_stream->res_buf, client_stream);
					event_add(&client_stream->wr_ev, NULL);
				}
			}
			break;
	}
}

/*
* Copy and store the requested filepath 
* Return a value of 1 when an unexpected request method is encountered
*/
int 
webserver_url_callback(http_parser* parser, const char* at, size_t length)
{
	request_details* req_data = (request_details*) parser->data;
	req_data->request_method=parser->method;
	req_data->file_path = malloc(length+1);
	memcpy(req_data->file_path, at, length);
	req_data->file_path[length] = '\0';
	
	if(parser->method == HTTP_HEAD || parser->method == HTTP_GET){
		return 0;
	} else {
		return 1;
	}
}

/**
* Finishes the callbacks and returns 1
* A return value of 1 specifies that no body is expected
*/
int 
webserver_headers_complete_callback (http_parser* parser)
{
	request_details* data = (request_details*) parser->data;
	data->complete = 1;
	return 1;
}

int 
log_access(webserver_connection_stream* client_stream)
{
	int response_code = 0; 
	char* log_message = NULL;
	
	request_details* req_data = &client_stream->req_data;
	response_details* res_data = &client_stream->res_data;
	
	switch((client_stream->res_data).response_enum){
		case HTTP_200_OK:
			response_code = 200;
		break;
		
		case HTTP_400_BAD:
			response_code = 400;
		break;
		
		case HTTP_403_FORBIDDEN:
			response_code = 403;
		break;
		
		case HTTP_404_NOT_FOUND:
			response_code = 404;
		break;
		
		case HTTP_405_METHOD:
			response_code = 405;
		break;
		
		case HTTP_444_NO_RESPONSE:
			response_code = 444;
		break;
		
		case HTTP_500_SERVER:
			response_code = 500;
		break;
	}
	
	char* request_method_str = (req_data->request_method == -1) ? "-" : http_method_str(req_data->request_method);
	char * file_path_str = (req_data->file_path == NULL) ? "-" : (req_data->file_path);
	if(res_data->content_length == -1){ 
		res_data->content_length = 0;
	}
	
	asprintf(&log_message, "%s [%s] \"%s %s\" %d %d\r\n", 
		req_data->remote_addr, req_data->req_date, 
		request_method_str, file_path_str,
		response_code, res_data->content_length);
	
	if(!client_stream->has_daemonise){
		printf("%s", log_message);
		fflush(stdout);
	}else if(client_stream->access_log != NULL){
		fwrite(log_message, sizeof(char), strlen(log_message), client_stream->access_log);
		fflush(client_stream->access_log);
	}
	
	free(log_message);
	return 0;
}

/**
*	Handle responses for only
*	400 Bad Request
*	405 Method Not Allowed
*	500 Internal Server Error
*/
int 
build_err_response_message(struct evbuffer* res_buf, webserver_connection_stream* client_stream)
{
	char* server_response_code = "HTTP/1.0 200 OK\r\n";
	char* request_date = NULL;
	
	request_details* req_data = &client_stream->req_data;
	response_details* res_data = &client_stream->res_data;
	
	switch((client_stream->res_data).response_enum){
		case HTTP_400_BAD:
			server_response_code = "HTTP/1.0 400 Bad Request\r\n";
		break;
		
		case HTTP_405_METHOD:
			server_response_code = "HTTP/1.0 405 Method Not Allowed\r\n";
		break;
		
		case HTTP_500_SERVER:
			server_response_code = "HTTP/1.0 500 Internal Server Error\r\n";
		break;
	}
	
	//Flush and clean out the res_buff first 
	evbuffer_drain(res_buf, EVBUFFER_LENGTH(res_buf));
	
	// Status Line
	evbuffer_add(res_buf, server_response_code, strlen(server_response_code));
	
	// General Headers
	asprintf(&request_date, "Date: %s\r\n", req_data->req_date);
	evbuffer_add(res_buf, request_date, strlen(request_date)); 
	evbuffer_add(res_buf, SERVER_CONNECTION, strlen(SERVER_CONNECTION)); 
	
	// Response Headers
	evbuffer_add(res_buf, SERVER_ID, strlen(SERVER_ID)); 
	
	// Empty Line
	evbuffer_add(res_buf, "\r\n", 2);

	free(request_date);
	
	log_access(client_stream);
	
	return 0;
}

/**
*	Handle responses for GET and HEAD
*	200 OK
*	403 Forbidden
*	404 Not Found
*/
int 
build_response_message(struct evbuffer* res_buf, webserver_connection_stream* client_stream)
{
	char* server_response_code = "HTTP/1.0 200 OK\r\n";
	char* request_date = NULL;
	char* content_length = NULL;
	char* last_modified = NULL;
	char* full_filepath = NULL;
	char* invalid_patterns[] = {"~/", "./", "../"};
	struct evbuffer* res_body_buff;
	struct stat file_attr;
	
	request_details* req_data = &client_stream->req_data;
	response_details* res_data = &client_stream->res_data;
	res_data->response_enum = HTTP_200_OK;
	
	res_body_buff= evbuffer_new();
	if(res_body_buff == NULL){
		warn("Unable to allocate memory for response body");
		free(res_body_buff);
		res_data->response_enum = HTTP_500_SERVER;
		build_err_response_message(res_buf, client_stream);
		return 1;
	}
	
	asprintf(&full_filepath, "%s%s", client_stream->directory, req_data->file_path);

	#ifdef DEBUG	
	printf("Building response now...checking data\n");
	printf("From [%s]\n", req_data->remote_addr);
	printf("For [%s]\n", full_filepath);
	printf("Method [%s]\n", http_method_str(req_data->request_method));
	printf("Time [%s]\n", req_data->req_date);
	fflush(stdout);
	#endif
	
	res_data->content_length = -1;
	if(check_invalid_filepath(3, invalid_patterns, req_data->file_path)){
		server_response_code = "HTTP/1.0 404 Not Found\r\n";
		res_data->response_enum = HTTP_404_NOT_FOUND;
	}else if(access( full_filepath, F_OK ) != -1){
		stat(full_filepath, &file_attr);
		strftime(res_data->last_modified, 50, "%a, %d %b %Y %T %Z", 
			localtime(&(file_attr.st_ctime)));
		if (access(full_filepath, R_OK ) != 0) {
			server_response_code = "HTTP/1.0 403 Forbidden\r\n";
			res_data->response_enum = HTTP_403_FORBIDDEN;
		} else {
			FILE* response_body = fopen(full_filepath, "r");
			int res_fd = -1, status = 0;
			res_fd = fileno(response_body);
			do{
				status = evbuffer_read(res_body_buff, res_fd, MSG_BUFF_LEN);
			} while (status > 0);
			fclose(response_body);
			res_data->content_length = EVBUFFER_LENGTH(res_body_buff); 
		}
	}else{
		server_response_code = "HTTP/1.0 404 Not Found\r\n";
		res_data->response_enum = HTTP_404_NOT_FOUND;
	}
	
	// Status Line
	evbuffer_add(res_buf, server_response_code, strlen(server_response_code));
	
	// General Headers
	asprintf(&request_date, "Date: %s\r\n", req_data->req_date);
	evbuffer_add(res_buf, request_date, strlen(request_date)); 
	evbuffer_add(res_buf, SERVER_CONNECTION, strlen(SERVER_CONNECTION)); 
	
	// Response Headers
	evbuffer_add(res_buf, SERVER_ID, strlen(SERVER_ID)); 
	
	// Entity Headers
	if(res_data->content_length != -1){
		asprintf(&content_length, "Content-Length: %d\r\n", res_data->content_length);
		evbuffer_add(res_buf, content_length, strlen(content_length)); 
		asprintf(&last_modified, "Last-Modified: %s\r\n", res_data->last_modified);
		evbuffer_add(res_buf, last_modified, strlen(last_modified)); 
	} 
	
	// Empty Line
	evbuffer_add(res_buf, "\r\n", 2);
		
	// GET Body
	if(req_data->request_method == HTTP_GET){
		int len;
		len = evbuffer_add_buffer(res_buf, res_body_buff);
		if(len == -1){
			perror("Error [Buffer append]");
			free(res_body_buff);
			res_body_buff = NULL;
			res_data->response_enum = HTTP_500_SERVER;
			build_err_response_message(res_buf, client_stream);
		}else{
			log_access(client_stream);
			#ifdef DEBUG
			printf("Response built [%d] bytes\n", EVBUFFER_LENGTH(res_buf));
			#endif
			evbuffer_free(res_body_buff);
		}
	} 
		
	free(full_filepath);
	free(content_length);
	free(last_modified);
	free(request_date);
			
	return 1;
}

/**
* Write the resulting http response back to the client, 
* in small message packets to ensure concurrency
*/
void 
webserver_write(int fd, short events, void *stream)
{
	int len_rmv, len_write;
	char message_packet[MSG_BUFF_LEN];
	webserver_connection_stream* client_stream = (webserver_connection_stream*) stream;
	
	len_rmv = evbuffer_remove(client_stream->res_buf, message_packet, MSG_BUFF_LEN);
	len_write = write(fd, message_packet, len_rmv);
	fsync(fd);
	
	if(len_write == -1){
		switch(errno){
			case EINTR:
			case EAGAIN:
				event_add(&client_stream->wr_ev, NULL);
				return;
			default:
				perror("[Writing client response]");
				webserver_close(client_stream);
				return;
		}
	}

	#ifdef DEBUG
	printf("Wrote [%d] bytes back to client [%d]\n", len_write, fd);
	#endif
	
	if(EVBUFFER_LENGTH(client_stream->res_buf) > 0){
		#ifdef DEBUG
		printf("[%d] remaining for client [%d]\n", EVBUFFER_LENGTH(client_stream->res_buf), fd);
		#endif
		event_add(&client_stream->wr_ev, NULL);
	}else{
		webserver_close(client_stream);
	}
	
	fflush(stdout);
}

/**
* Close the socket connection
*/
void 
webserver_close(webserver_connection_stream* client_stream)
{
	#ifdef DEBUG
	printf("Closing client connection [%d]\n", EVENT_FD(&client_stream->rd_ev));
	#endif
	
	evbuffer_free(client_stream->req_buf);
	evbuffer_free(client_stream->res_buf);
	event_del(&client_stream->wr_ev);
	event_del(&client_stream->rd_ev);
	close(EVENT_FD(&client_stream->rd_ev));
	
	// Free everything, except the params referenced from server_config
	request_details req_data = client_stream->req_data;
	response_details res_data = client_stream->res_data;
	
	free(req_data.remote_addr); 
	free(req_data.file_path);
	free(req_data.req_date);
	free(res_data.last_modified);
	free(client_stream->parser);
	free(client_stream);
	fflush(stdout);
}

/**
* Parse and set the server_config struct, with default or program intialisation parameters. 
*/
int 
parse_params(int argc, char** argv, webserver_initialisation* server_config)
{
	int param;
	server_config->ai_family=PF_UNSPEC;
	server_config->address = NULL; 
	server_config->port = "http"; 
	server_config->can_daemonise = 1; 
	server_config->access_log = NULL; 
	server_config->directory = "./"; 
	while((param = getopt(argc, argv, "46da:l:p:")) != -1){  
		switch (param) { 
			case '4': 
				server_config->ai_family=AF_INET;
				break; 
			case '6': 
				server_config->ai_family=AF_INET6;
				break;
			case 'l':
				server_config->address=optarg;
				break;
			case 'p':
				server_config->port = optarg;
				break;
			case 'd': 
				server_config->can_daemonise=0;
				break;
			case 'a':
				if(access(optarg, W_OK ) != 0){
					perror("Error [access.log param]");
					usage();
				}else{
					server_config->access_log=fopen(optarg, "w");
					break;
				}			
			default: 
				usage();
		} 
	} 	
	
	argc -= optind;
    argv += optind;
	
	if(argc == 1){
		DIR* dir = opendir(argv[0]);
		if(dir){
			closedir(dir);
			server_config->directory=argv[0];
		}else{
			perror("Error [directory param]");
			usage();
		}
    }else{
		usage();
	}
	return 0;
}

/* 
* Return 1 if invalid patterns in the filepath are found
*/
int 
check_invalid_filepath(int num_invalid_patterns, char** invalid_patterns, char* filepath)
{
	int i;
	char* ptr = NULL;
	
	if(strcmp("/", filepath) == 0){
		return 1;
	}
	
	for(i = 0; i<num_invalid_patterns; i++){
		ptr = strstr(filepath, invalid_patterns[i]);
		
		if(ptr!=NULL){
			return 1;
		}		
	}

	return 0;
}

int 
check_stream_memory(webserver_connection_stream* client_stream)
{
	if( client_stream->req_buf == NULL ||
		client_stream->res_buf == NULL ||
		client_stream->parser == NULL ||
		(client_stream->req_data).remote_addr == NULL ||
		(client_stream->req_data).req_date == NULL ||
		(client_stream->res_data).last_modified == NULL	){
		return 1;
	}
	return 0;
}

/*
* The usage function
*/
__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-46d] [-a access.log] [-l address] [-p port] directory\n", __progname);
	exit(1);
}