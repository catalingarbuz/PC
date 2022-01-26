#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"
#include "parson.c"


void JSON_serialization(char* username, char* password, char** result) {
	JSON_Value *root_value = json_value_init_object();
	JSON_Object *object = json_value_get_object(root_value);
	json_object_set_string(object, "username", username);
	json_object_set_string(object, "password", password);
	free(*result);
    *result = json_serialize_to_string_pretty(root_value);
}

void registerAuthPrompt(char **user, char **JsonRes) {
   char *password = calloc(USERNAMELEN, sizeof(char));
   printf("username="); scanf("%s", *user);
   printf("password="); scanf("%s", password); 
   JSON_serialization(*user, password, &(*JsonRes));
   free(password);
}

void register_querry(int sockfd) {
	char *username = calloc(USERNAMELEN, sizeof(char));
	char *result = calloc(LINELEN, sizeof(char));
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));

    registerAuthPrompt(&username, &result);
    
    char *message;
    message = compute_postj_request("34.118.48.238:8080", 
    	                          "/api/v1/tema/auth/register", 
                                  "application/json", result,
      	                           NULL, 0);

    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    // Extract "HTTP/1.1 ..." antet from response
    response[EXTRACTANTET] = '\0';
    
    // the acount was created
    if (!strcmp(response, "HTTP/1.1 201")) {
       printf("[SUCCES] Acount with %s username was created!\n\n", username);
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }

    free(message);
    free(response);
    free(response_copy);
    free(username);
    free(result);
    return;   
}

char* getCookie(char *text) {
   char *cookie;
   char *p;
   char *first10 = calloc(LINELEN, sizeof(char));
   p = strtok(text, "\n");
   // iterate while Set-Cookie or NULL isn't found
   while (p != NULL) {
   	 memset(first10, 0, LINELEN);
     strncpy(first10, p, 10);
     if(!strcmp(first10, "Set-Cookie")) {
       cookie = p+12;	
       strtok(cookie, ";");
       break;
     }
   	 
   	 p = strtok(NULL, "\n");
   }

   free(first10);
   return cookie;
}

char* login_querry(int sockfd) {
	char *username = calloc(USERNAMELEN, sizeof(char));
	char *result = calloc(LINELEN, sizeof(char));
	char *response = calloc(BUFLEN, sizeof(char));
	char *mem = response;
    char *response_copy = calloc(BUFLEN, sizeof(char));
    char *cookie = NULL; //= calloc(BUFLEN, sizeof(char));
    registerAuthPrompt(&username, &result);
    
    char *message;
    message = compute_postj_request("34.118.48.238:8080", 
    	                          "/api/v1/tema/auth/login", 
                                  "application/json", result,
      	                           NULL, 0);

    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    // Extract "HTTP/1.1 ..." antet from response
    response[EXTRACTANTET] = '\0';
    
    // the acount was created
    if (!strcmp(response, "HTTP/1.1 200")) {
       cookie = getCookie(response_copy);
       printf("[SUCCES] Session cookie: %s\n\n", cookie);
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }
    
    free(message);
    free(mem);
    free(response);
    free(response_copy);
    free(username);
    free(result);
    if (cookie != NULL) {
      return cookie;   
    }
    return "0";
}

char* extractJWT(char* jsontoken) {
	char *jwt;
	//token start from 11 char
	jwt = jsontoken + 10;
	//remove last " and bracket
	jwt[strlen(jwt) - 2] = '\0';
	return jwt;
}

char* enter_library(int sockfd, char* sCookie) {
	
	char *message;
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));
    char *result;
    //save cookie
	char** cookie = malloc(sizeof(char*) * 2);
    for (int i = 0; i < 2; i++) {
    	cookie[i] = malloc(sizeof(char) * LINELEN);
    }

    strcpy(cookie[1], sCookie);

 	message = compute_get_request("34.118.48.238:8080",
 	                             "/api/v1/tema/library/access",
                                     NULL, cookie, 1);
 	// Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    response[EXTRACTANTET] = '\0';

    if (!strcmp(response, "HTTP/1.1 200")) {
       printf("[SUCCES] %s \n\n", 
       basic_extract_json_response(response_copy));
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }

    if (!strcmp(response, "HTTP/1.1 200")) {
    	result = extractJWT(basic_extract_json_response(response_copy));
    }

    //Free data and return result
    free(response_copy);
    free(response);
    free(message);
    for (int i = 0; i < 2; i++) {
      free(cookie[i]);
    }
    free(cookie);
    return result;
 }

 void getBooks(char* jwt, char* sCookie, int sockfd) {
    char *message;
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));
	char** cookie = malloc(sizeof(char*) * 2);
    for (int i = 0; i < 2; i++) {
    	cookie[i] = malloc(sizeof(char) * LINELEN);
    }
    
    if (jwt != NULL) {
      sprintf(cookie[0], "Authorization: Bearer %s", jwt);
    }
    if (sCookie != NULL) {
      strcpy(cookie[1], sCookie);
    }
    
    message = compute_get_request("34.118.48.238:8080",
 	                             "/api/v1/tema/library/books",
                                     NULL, cookie, 2);
    
    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    response[EXTRACTANTET] = '\0';

    if (!strcmp(response, "HTTP/1.1 200")) {
       printf("[SUCCES] %s \n\n", 
       strstr(response_copy, "["));
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }

    free(response);
    free(response_copy);
    free(message);
    for (int i = 0; i < 2; i++) {
      free(cookie[i]);
    }
    free(cookie);
 }

 void getBook(char* jwt, char* sCookie, int sockfd) {
 	int id;

 	// Prompt for id
    printf("id="); scanf("%d", &id);
    printf("\n");
    char *url = calloc(LINELEN, sizeof(char));
    char *message;
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));
	char** cookie = malloc(sizeof(char*) * 2);
    for (int i = 0; i < 2; i++) {
    	cookie[i] = malloc(sizeof(char) * LINELEN);
    }
    
    if (jwt != NULL) {
      sprintf(cookie[0], "Authorization: Bearer %s", jwt);
    }
    if (sCookie != NULL) {
      strcpy(cookie[1], sCookie);
    }
    
    sprintf(url, "/api/v1/tema/library/books/%d", id);
    message = compute_get_request("34.118.48.238:8080",url,
                                     NULL, cookie, 2);
    
    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    response[EXTRACTANTET] = '\0';

    if (!strcmp(response, "HTTP/1.1 200")) {
       printf("[SUCCES] %s \n\n", 
       basic_extract_json_response(response_copy));
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }
    
    free(url);
    free(response);
    free(response_copy);
    free(message);
    for (int i = 0; i < 2; i++) {
      free(cookie[i]);
    }
    free(cookie);
 }

void book_serialization(char* title, char* author, char* genre, 
	                    char* page_count, char* publisher, char** result) {
	JSON_Value *root_value = json_value_init_object();
	JSON_Object *object = json_value_get_object(root_value);
	title[strlen(title)-1] = '\0';
	author[strlen(author)-1] = '\0';
	genre[strlen(genre)-1] = '\0';
	page_count[strlen(page_count)-1] = '\0';
	publisher[strlen(publisher)-1] = '\0';
	json_object_set_string(object, "title", title);
	json_object_set_string(object, "author", author);
	json_object_set_string(object, "genre", genre);
	json_object_set_string(object, "page_count", page_count);
	json_object_set_string(object, "publisher", publisher);
	free(*result);
    *result = json_serialize_to_string_pretty(root_value);
}

 void addBookPrompt(char **JsonRes) {
   getchar();
   char *title = calloc(50, sizeof(char));
   char *author = calloc(50, sizeof(char));
   char *genre = calloc(50, sizeof(char));
   char *page_count = calloc(50, sizeof(char));
   char *publisher = calloc(50, sizeof(char));
   printf("title="); fgets(title, 50, stdin);
   printf("author="); fgets(author, 50, stdin);
   printf("genre="); fgets(genre, 50, stdin);
   printf("publisher="); fgets(publisher, 50, stdin);
   printf("page_count="); fgets(page_count, 50, stdin);
   book_serialization(title, author, genre, page_count,
                      publisher, &(*JsonRes));
   free(title);
   free(author);
   free(genre);
   free(page_count);
   free(publisher);
}

 void addBook(char* jwt, char* sCookie, int sockfd) {

    char *result = calloc(LINELEN, sizeof(char));
 	// Prompt for addbook
    addBookPrompt(&result);
   
    char *message;
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));

	char** cookie = malloc(sizeof(char*) * 2);
    for (int i = 0; i < 2; i++) {
    	cookie[i] = malloc(sizeof(char) * LINELEN);
    }
    
    if (jwt != NULL) {
      sprintf(cookie[0], "Authorization: Bearer %s", jwt);
    }
    if (sCookie != NULL) {
      strcpy(cookie[1], sCookie);
    }
    
    
    message = compute_post_request("34.118.48.238:8080",
    	                          "/api/v1/tema/library/books",
                                    "application/json", result, cookie, 2);
    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
   // strcpy(response_copy, response);
    response[EXTRACTANTET] = '\0';

    if (!strcmp(response, "HTTP/1.1 200")) {
       printf("[SUCCES] book added!\n\n");
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }
     
    free(result);
    free(response);
    free(response_copy);
    free(message);
    for (int i = 0; i < 2; i++) {
      free(cookie[i]);
    }
    free(cookie);
 }

 void deleteBook(char* jwt, char* sCookie, int sockfd) {
 	int id;

 	// Prompt for id
    printf("id="); scanf("%d", &id);
    printf("\n");
    char *url = calloc(LINELEN, sizeof(char));
    char *message;
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));
	char** cookie = malloc(sizeof(char*) * 2);
    for (int i = 0; i < 2; i++) {
    	cookie[i] = malloc(sizeof(char) * LINELEN);
    }
    
    if (jwt != NULL) {
      sprintf(cookie[0], "Authorization: Bearer %s", jwt);
    }
    if (sCookie != NULL) {
      strcpy(cookie[1], sCookie);
    }
    
    sprintf(url, "/api/v1/tema/library/books/%d", id);
    message = compute_del_request("34.118.48.238:8080",url,
                                     NULL, cookie, 2);

    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    response[EXTRACTANTET] = '\0';

    if (!strcmp(response, "HTTP/1.1 200")) {
       printf("[SUCCES] book deleted!\n\n");
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }

     free(url);
     free(response);
     free(response_copy);
     free(message);
     for (int i = 0; i < 2; i++) {
      free(cookie[i]);
     }
     free(cookie);
 }

 void logout(char* sCookie, int sockfd) {
    char *message;
	char *response = calloc(BUFLEN, sizeof(char));
    char *response_copy = calloc(BUFLEN, sizeof(char));
	char** cookie = malloc(sizeof(char*) * 2);
    for (int i = 0; i < 2; i++) {
    	cookie[i] = malloc(sizeof(char) * LINELEN);
    }

    if (sCookie != NULL) {
      strcpy(cookie[1], sCookie);
    }
    
    message = compute_get_request("34.118.48.238:8080",
    	                          "/api/v1/tema/auth/logout",
                                     NULL, cookie, 1);

    // Send message to server
    send_to_server(sockfd, message);
    // Receive server response
    response = receive_from_server(sockfd);
    strcpy(response_copy, response);
    response[EXTRACTANTET] = '\0';

    if (!strcmp(response, "HTTP/1.1 200")) {
       printf("[SUCCES] logged out\n\n");
     // Too Many requests
     } else if (!strcmp(response, "HTTP/1.1 429")) {
       printf("[FAIL] Too Many requests\n\n");
     // Error, print json payload
     } else {
     	printf("%s\n\n", basic_extract_json_response(response_copy));
     }

     free(response);
     free(response_copy);
     free(message);
     for (int i = 0; i < 2; i++) {
      free(cookie[i]);
     }
     free(cookie);
 }


int main(int argc, char *argv[])
{
	char *input = calloc(LINELEN, sizeof(char));
	char *sessionCookie = calloc(LINELEN, sizeof(char));
	char *jwtToken = calloc(LINELEN, sizeof(char));
    int sockfd;
    
    sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    
    while (1) {
    	sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    	scanf("%s", input);
    	// Register 
    	if (!strcmp(input, "register")) {
           register_querry(sockfd);
    	}

    	// Authentification
    	if (!strcmp(input, "login")) {
    		strcpy(sessionCookie, login_querry(sockfd));
    	}

    	//enter library
    	if (!(strcmp(input, "enter_library"))) {
    		strcpy(jwtToken, enter_library(sockfd, sessionCookie));
    	}

    	// GET books
    	if (!(strcmp(input, "get_books"))) {
    		getBooks(jwtToken, sessionCookie, sockfd);
    	}

        // Get info about a specified book
    	if (!(strcmp(input, "get_book"))) {
           getBook(jwtToken, sessionCookie, sockfd);
    	}
        // add a book
    	if(!(strcmp(input, "add_book"))) {
    		addBook(jwtToken, sessionCookie, sockfd);
    	}
        //delete a book
    	if(!(strcmp(input, "delete_book"))) {
    		deleteBook(jwtToken, sessionCookie, sockfd);
    	}
        
        //logout 
        if (!(strcmp(input, "logout"))) {
        	logout(sessionCookie, sockfd);
        	memset(sessionCookie, 0, LINELEN);
        }
    	// Exit condition
    	if (!strcmp(input, "exit")) {
    		break;
    	}
    	close_connection(sockfd);
    }
    
    free(sessionCookie);
    free(jwtToken);
    free(input);
    
    close_connection(sockfd);
 
    return 0;
}
