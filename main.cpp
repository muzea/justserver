#include <iostream>
#include <csignal>

#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>

sockaddr_in address;
int temp_socket;
enum HTTP_TYPE{
    HTTP_GET,
    HTTP_POST,
    HTTP_HEAD,
    HTTP_TOTAL
};

const int MAX_CONNECTIONS = 3;

using namespace std;

inline bool _str_match(const char* a,const char* b ){
    while ( *a && *b && *a == *b ){
        ++a;
        ++b;
    }
    return (*b == 0);
}

class HTTP_request{
    char* raw_data = nullptr;
    int socket = 0;
public:
    int method = HTTP_TOTAL;
    HTTP_request( char* _raw_data , ssize_t _raw_data_len,int socket ){
        this->raw_data = _raw_data;
        this->socket = socket;
    }

    inline const int get_socket(){
        return this->socket;
    }

    void parse( int lenMax = 1 ){
        if( _str_match( raw_data, "GET" ) ){
            this->method = HTTP_GET;
        }
        else if( _str_match( raw_data, "POST" ) ){
            this->method = HTTP_POST;
        }
    }

    ~HTTP_request(){
        if( this->raw_data ){
            delete[] raw_data;
        }
    }
};

class HTTP_response{
    int socket = 0;
    HTTP_response( const HTTP_response& source ){}
public:
    HTTP_response( int socket ){
        this->socket = socket;
    }
    ~HTTP_response(){

    }
    void end(){
        close(this->socket);
    }

    void sendHeader(const char* Status_code,const char* Content_Type, size_t TotalSize){
        if( this->socket ){
            char *headerBuffer = new char[512];
            char headerFormat[] =
                    "\r\nHTTP/1.1 %s"
                    "\r\nContent-Type: %s"
                    "\r\nServer: justserver"
                    "\r\nContent-Length: %i"
                    "\r\nDate: %s"
                    "\r\n"
            ;
            time_t rawtime;
            time ( &rawtime );
            sprintf( headerBuffer, headerFormat, Status_code, Content_Type, TotalSize,  ctime(&rawtime) );

            this->send_data( headerBuffer, strlen(headerBuffer) );
            delete[] headerBuffer;
        }
    }

    ssize_t send_data(const char* pData, size_t length){
        return send( this->socket, pData, length, 0);
    }
};

HTTP_request* buildRequest(const int socket){
    char* buffer = new char[500];

    ssize_t msgLen = 0;
    if ((msgLen = recv(socket, buffer, 500, 0)) == -1) {
        cerr<<"Error handling incoming request"<<endl;
        delete[] buffer;
        return NULL;
    }
    HTTP_request* pRequest = new HTTP_request(buffer, msgLen, socket);

    return pRequest;
}


void callForExit(int s){
    close( temp_socket );
    cout<<"Stopping justserver"<<endl;
}

int createSocket(){
    int current_socket = socket(AF_INET, SOCK_STREAM, 0);

    if ( current_socket == -1 ){
        cerr<<"Create socket failed"<<endl;
        exit(-1);
    }

    return current_socket;
}

void bindSocket(const int current_socket, const uint16_t port ){
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if ( bind(current_socket, (struct sockaddr *)&address, sizeof(address)) < 0 ){
        cerr<<"Bind to port failed"<<endl;
        exit(-1);
    }
}

void startListener(const int current_socket){
    if ( listen(current_socket, MAX_CONNECTIONS) < 0 ){
        cerr<<"Listen on port failed"<<endl;
        exit(-1);
    }
}

void handleHttpGET(HTTP_request* pRequest){
    HTTP_response* pResponse = new HTTP_response( pRequest->get_socket() );

    const char content[] = "Hello world!\n";
    size_t content_length = strlen( content );

    pResponse->sendHeader("200 OK", "text/plain", content_length );
    pResponse->send_data( content, content_length );
    pResponse->end();
    delete pResponse;
}

int receive(int socket){
    HTTP_request* pRequest = buildRequest( socket );
    if( pRequest ){
        pRequest->parse();
        switch ( pRequest->method ){
            case HTTP_GET:
                handleHttpGET(pRequest);
                break;
            case HTTP_POST:
            case HTTP_HEAD:
            default:
                break;
        }
        delete pRequest;
        return 0;
    }
    return -1;
}

void handle(const int socket){
    if (receive( socket ) < 0) {
        cerr<<"Receive failed"<<endl;
        exit(-1);
    }
}

void start(){
    temp_socket = createSocket();
    bindSocket( temp_socket, 8081 );
    startListener( temp_socket );

    signal(SIGQUIT, callForExit);
    signal(SIGKILL, callForExit);
    signal(SIGTERM, callForExit);

    sockaddr_storage connector;
    socklen_t addr_size = sizeof(connector);

    while ( true ){
        int connecting_socket = accept(temp_socket, (struct sockaddr *)&connector, &addr_size);
        if ( connecting_socket < 0 ) {
            cerr<<"Accepting sockets failed"<<endl;
            exit(-1);
        }

        handle(connecting_socket);
    }
}

int main(int argc, char* argv[]) {
    cout<<"Process id is "<<getpid()<<endl;
    start();
    return 0;
}