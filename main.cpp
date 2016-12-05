#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <iostream>
#include <fstream>
#include <vector>

#define DROPLET_IP "138.68.40.50"
#define JETSON_PORT 80
#define LAMBDA_PORT 10000

#define PUT_AND_ID_REQ "<put_and_id>"
#define PUT_REQ        "<put>"
#define GET_REQ        "<get>"

#define RETRIEVE_RESP       "<ret>"
#define NO_RETRIEVE_RESP    "<no_ret>"
#define INSERT_VALID_RESP   "<insert>"
#define INSERT_INVALID_RESP "<no_insert>"
#define FINISH_GET_RESP     "<get_done>"
#define FINISH_PUT_RESP     "<put_done>"

#define DB_IDX_NAME "wardrobe.dbidx"
#define DB_ID_NAME  "wardrobe.dbid"

// Global Variable Database
std::ofstream * database_id;
std::ofstream * database_idx;

int check_database(char * clothing_id)
{
    // Iterate through database
    std::ifstream db_id("wardrobe.dbid");
    std::ifstream db_idx("wardrobe.dbidx");
    for( std::string line_id; getline( db_id, line_id ); )
    {
        std::string line_idx;
        getline(db_idx, line_idx);
        const char * clothing_id_db = line_id.c_str();
        std::cout << "id:" << line_id << "idx:" << line_idx << std::endl; 
        if(!strcmp(clothing_id, clothing_id_db))
        {
            std::cout << "match found! :" << line_idx << std::endl;
            db_id.close();
            db_idx.close();
            return atoi(line_idx.c_str());
        }
        else
        {
            std::cout << "not match" << std::endl;
        }
    }
    db_id.close();
    db_idx.close();
    return -1;
}

int insert_database(char * clothing_id)
{
    // Iterate through database
    
    std::fstream db_id("wardrobe.dbid");
    std::fstream db_idx("wardrobe.dbidx");
    std::vector<int> vec;
    // Check if identifier already in use
    for( std::string line_id; getline( db_id, line_id ); )
    {
        std::string line_idx;
        getline(db_idx, line_idx);
        const char * clothing_id_db = line_id.c_str();
        std::cout << "id:" << line_id << "idx:" << line_idx << std::endl; 
        if(!strcmp(clothing_id, clothing_id_db))
        {
            std::cout << "match found! article already exists in database..." << line_idx << std::endl;
            db_id.close();
            db_idx.close();
            return -1;
        }
        int idx = atoi(line_idx.c_str());
        vec.push_back(idx);
    }

    // Calculate insertion index
    int n = vec.size();
    for (int i = 0; i < n; i++)
    {
        int val = vec[i];
        if ((val < 0) or (val >= vec.size()))
            continue;
        int curval = vec[i], nextval = vec[vec[i]];
        while (curval != nextval)
        {
            vec[curval] = curval;
            curval = nextval;
            if ((curval < 0) or (curval >= vec.size()))
                continue;
            nextval = vec[nextval];
        } 
    }
    int insert_idx = vec.size();
    for (int i=0; i < vec.size(); i++){
        if (vec[i] != i){
            insert_idx = i;
            break;
        }
    }
   
    // Perform insertion 
    db_id.close();
    db_idx.close();
    std::cout << "Inserting:" << clothing_id << "|at:" << insert_idx << std::endl;
    db_id.open("wardrobe.dbid", std::ios::app);
    db_idx.open("wardrobe.dbidx", std::ios::app);
    db_id << clothing_id << std::endl;
    db_idx << insert_idx << std::endl;

    db_id.close();
    db_idx.close();
    return insert_idx;
    // Check what next available index is
}

int remove_database(char * clothing_id)
{
    // Check if identifier in database
    std::fstream db_id("wardrobe.dbid");
    std::fstream db_idx("wardrobe.dbidx"); 
    std::fstream db_id_copy("wardrobe_copy.dbid", std::fstream::out);
    std::fstream db_idx_copy("wardrobe_copy.dbidx", std::fstream::out);

    int remove = -1;
    std::cout << "Removing:" << clothing_id << std::endl;
    for( std::string line_id; getline( db_id, line_id ); )
    {
        std::string line_idx;
        getline(db_idx, line_idx);
        const char * clothing_id_db = line_id.c_str();
        std::cout << "id:" << line_id << "idx:" << line_idx << std::endl; 
        if(!strcmp(clothing_id, clothing_id_db))
        {
            std::cout << "match found! removing" << line_id << std::endl;
            remove = 1;
        }
        else
        {
            db_id_copy << line_id << std::endl;
            db_idx_copy << line_idx << std::endl;
        }
    }

    db_id.close();
    db_idx.close();
    db_id_copy.close();
    db_idx_copy.close();
    rename("wardrobe_copy.dbid", "wardrobe.dbid");
    rename("wardrobe_copy.dbidx", "wardrobe.dbidx");
    return remove;
}

int main(int argc, char *argv[])
{
    // Open Wardrobe File
    database_id = new std::ofstream("wardrobe.dbid", std::ofstream::out | std::ofstream::in);
    database_idx = new std::ofstream("wardrobe.dbidx", std::ofstream::out | std::ofstream::in);

    // Declare File Descriptors 
    int listenfd = 0;
    int jetson_fd = 0;
    struct sockaddr_in serv_addr; 

    // Create Socket to Listen For Jetson
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    inet_aton( DROPLET_IP, &serv_addr.sin_addr );
    serv_addr.sin_port = htons( JETSON_PORT ); 

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));   
    
    // Wait for Jetson to Connect
    listen(listenfd, 1);
    std::cout << "Droplet IP: " << DROPLET_IP << std::endl << "Listening on Port: " << JETSON_PORT << std::endl;
    std::cout << "Waiting for Jetson to Establish Connection..." << std::endl;
    jetson_fd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
    
    // Jetson connects
    std::cout << "Connection with Jetson Established.\n" << std::endl;
    close(listenfd);
    
    // Listen for Lambda Function Requests
    listenfd = 0;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    inet_aton( DROPLET_IP, &serv_addr.sin_addr );
    serv_addr.sin_port = htons( LAMBDA_PORT );  

    // Initialize Buffers for communication
    char read_buffer[1024];
    char feedback[1024];
    
    int bytes_read;
    int feedback_read;
    
    // Listening
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    while(1)
    {
        // Wait for Lambda Function Requests
        listen(listenfd, 1); 
        int lambda_fd = 0;
        std::cout << "Waiting for Lambda Function Requests" << std::endl;
        std::cout << "Listening on Port: " << LAMBDA_PORT << std::endl << "Droplet IP: " << DROPLET_IP << std::endl;
        lambda_fd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
        // Lambda Function Connects
        std::cout << "Connection with Lambda Function Established" << std::endl << std::endl;

        // Zero out buffers
        memset(read_buffer, 0, sizeof(read_buffer)); 
        memset(feedback, 0, sizeof(feedback));

        bytes_read = read(lambda_fd, read_buffer, sizeof(read_buffer));
        std::cout << "recv "<< bytes_read << "bytes:" << read_buffer << std::endl;

        // Determine Action Requested by Jetson
        if(!strncmp(PUT_AND_ID_REQ, read_buffer, strlen(PUT_AND_ID_REQ)))
        {
            // Idenitfy and Store Piece of Clothing
            std::cout << "Performing Identification" << std::endl;
            
            // Send index to jetson
            write(jetson_fd, "<id>", strlen("<id>"));
        }
        else if(!strncmp(PUT_REQ, read_buffer, strlen(PUT_REQ)))
        {
            // Store Piece of Clothing
            std::cout << "Performing Put Request" << std::endl;
            char * clothing_id = read_buffer + strlen(PUT_REQ);
            int index = insert_database(clothing_id);
            if (index >= 0)
            {
                // Valid Clothing ID
                // Confirm valid ID with lambda
                write(lambda_fd, INSERT_VALID_RESP, strlen(INSERT_VALID_RESP));
            
                // Send index to jetson
                sprintf(feedback, "<put>%d", index);
                write(jetson_fd, feedback, strlen(feedback));
            } else {
                // Invalid Clothing ID, already exists
                // Inform lambda function invalid clothing id (already in database)
                write(lambda_fd, INSERT_INVALID_RESP, strlen(INSERT_INVALID_RESP));
            }
            
        }
        else if(!strncmp(GET_REQ, read_buffer, strlen(GET_REQ)))
        {
            // Retrieve Piece of Clothing
            std::cout << "Executing Get Request" << std::endl;
            char * clothing_id = read_buffer + strlen(GET_REQ);
            std::cout << "Checking if Wardrobe Contains Item:"<< "\"" << clothing_id << "\"" << std::endl;
            int index = check_database( clothing_id );

            if (index >= 0 )
            {
                // Article exists in wardrobe
                // Confirm we can retrieve
                write(lambda_fd, RETRIEVE_RESP, strlen(RETRIEVE_RESP));
                // Request Retrieval from Jetson
                sprintf(feedback, "<get>%d", index);
                write(jetson_fd, feedback, strlen(feedback));
                remove_database( clothing_id);
            }
            else
            {
                // Article does not exist in wardrobe
                // Tell Lambda article not present
                write(lambda_fd, NO_RETRIEVE_RESP, strlen(NO_RETRIEVE_RESP));
            }
        }

        // Lambda Function Serviced, Continue Listening for more requests
        std::cout << "\nLambda Function Serviced\n" << std::endl;
        close(lambda_fd);
    }

    // Close File Descriptors and Delete Allocated Objects
    close(jetson_fd);
    database_id->close();
    database_id->close();
    delete database_id;
    delete database_idx;
}
