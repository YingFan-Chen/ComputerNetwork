#include <bits/stdc++.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <unistd.h>

using namespace std;
#define server_port 8080
#define max_clients 1000

class client{
    public:
        int sock, file_size;
        bool login;
        FILE* fp;
        string account, chatfriend;
        client(int _sock) : sock(_sock){
            login = false;
            fp = nullptr;
            file_size = 0;
        }
        inline bool operator== (const client &i) const{
            return sock == i.sock;
        }
};

class http{
    public:
        string method, path;
        vector<pair<string, string>> value;
        http(){}
};

static int callback1(void *data, int argc, char **argv, char **azColName){
    for(int i = 0; i < argc; i ++){
        strncat((char*) data, argv[i], 1024);
    }
    return 0;
}

static int callback2(void *data, int argc, char **argv, char **azColName){
    for(int i = 0; i < argc; i ++){
        strncat((char*) data, argv[i], 1024);
        strncat((char*) data, " ", 1024);
    }
    return 0;
}

static int callback3(void *data, int argc, char **argv, char **azColName){
    if(argc != 0){
        for(int i = 0; i < argc; i += 2){
            strncat((char*) data, "<p>", 4096 << 3);
            strncat((char*) data, argv[i], 4096 << 3);
            strncat((char*) data, " : ", 4096 << 3);
            strncat((char*) data, argv[i + 1], 4096 << 3);
            strncat((char*) data, "</p>", 4096 << 3);
        }
    }    
    return 0;
}

sqlite3* set_environment();
void set_address(struct sockaddr_in &);
int set_master_socket(struct sockaddr*);
void poll(vector<client> &, int, struct sockaddr*, sqlite3* db);
void client_operation(client &, struct sockaddr*, vector<client> &, sqlite3* db);
void header(char*, int, string);
void openfile(client&, char*, char*);
void closefile(client&);
void getHTTP(http &, string &);
void Login(client &);
void sendfile(client &);
void Icon(client &);
bool checkPassword(http &, sqlite3*, client &);
void Home(client &);
void Add(client &);
void Delete(client &);
void Chat(client &);
void Addfriend(client &, http &, sqlite3*);
void Chatlist(client &, sqlite3*);
void Deletelist(client &, sqlite3*);
void Deletefriend(client &, http &, sqlite3*);
void Chatroom(client &, http &);
void Chathistory(client &, sqlite3*);
void Chatmessage(client &, http &, sqlite3*);


int main(){
    struct sockaddr_in address;
    sqlite3 *db;
    int master_sock;
    vector<client> clients;

    db = set_environment();

    set_address(address);

    master_sock = set_master_socket((struct sockaddr *) &address);

    while(true){
        poll(clients, master_sock, (struct sockaddr *) &address, db);
    }

}

sqlite3* set_environment(){
    sqlite3 *db;
    char *err;
    int rc;

    mkdir("./server_dir/", 0755);

    rc = sqlite3_open("./server_dir/client_database.db", &db);
    if(rc){
        fprintf(stderr, "Fail to open the database : %s\n", sqlite3_errmsg(db));
    }else{
        fprintf(stderr, "Open database successfully....\n");
    }

    rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS accounts(account varchar(128), password varchar(128));", NULL, NULL, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to create account table : %s\n", err);
    }else{
        fprintf(stderr, "Create account table successfully....\n");
    }

    rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS relations(client varchar(128), friends varchar(128), chatroom varchar(1024));", NULL, NULL, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to create relation table : %s\n", err);
    }else{
        fprintf(stderr, "Create relation table successfully....\n");
    }

    return db;
}

void set_address(struct sockaddr_in &address){
    address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(server_port);
}

int set_master_socket(struct sockaddr *address){
    int master_sock;
    int opt = 1;

    if((master_sock = socket(AF_INET , SOCK_STREAM , 0)) == 0){
        perror("Fail to socket ");
    }else{
        fprintf(stderr, "Socket successfully....\n");
    }

    if(setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
        perror("Fail to setsockopt ");
    }else{
        fprintf(stderr, "Setsockopt successfully....\n");
    }

    if(bind(master_sock, address, sizeof(*address)) < 0){  
        perror("Fail to bind ");
    }else{
        fprintf(stderr, "Bind successfully....\n");
    }

    if(listen(master_sock, max_clients) < 0){
        perror("Fail to listen ");
    }else{
        fprintf(stderr, "Listen successfully....\n");
    }

    return master_sock;
}

void poll(vector<client> &clients, int master_sock, struct sockaddr *address, sqlite3 *db){
    int max_fd, new_sock;
    int addr_len = sizeof(*address);
    fd_set readfds;

    FD_ZERO(&readfds);

    FD_SET(master_sock, &readfds);
    max_fd = master_sock;

    for(auto &i : clients){
        FD_SET(i.sock, &readfds);
        max_fd = max(max_fd, i.sock);
    }

    if(select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0 and errno != EINTR){
        perror("Fail to select ");
    }else{
        //fprintf(stderr, "Select successfully....\n");
    }

    if(FD_ISSET(master_sock, &readfds)){
        if((new_sock = accept(master_sock, address, (socklen_t*) &addr_len)) < 0){
            perror("Fail to accept.... ");
        }else{
            clients.push_back(client{new_sock});
            fprintf(stderr, "Accept successfully....\n");
        }
    }

    for(auto &i : clients){
        if(FD_ISSET(i.sock, &readfds)){
            client_operation(i, address, clients, db);
        }
    }
}

void client_operation(client &client_obj, struct sockaddr *address, vector<client> &clients, sqlite3* db){
    int valread;
    int addr_len = sizeof(*address), sock = client_obj.sock;
    char read_buffer[4096];

    memset(read_buffer, 0, 4096);
    valread = recv(client_obj.sock, read_buffer, 4096, 0);

    //This means someone disconnected
    if(valread == 0){
        /*getpeername(client_obj.sock , (struct sockaddr*)&address , (socklen_t*) &addr_len); 
        close(client_obj.sock);
        auto iter= find(clients.begin(), clients.end(), client_obj);
        clients.erase(iter);*/
    }
    else{
        fprintf(stderr, "%s\n\n", read_buffer);
        http HTTP;
        string response = read_buffer;
        getHTTP(HTTP, response);

        if(client_obj.login == false){
            if(HTTP.path == "/favicon.ico"){
                Icon(client_obj);
            }else if(HTTP.path == "/home.html"){
                if(checkPassword(HTTP, db, client_obj)) Home(client_obj);
                else Login(client_obj);
            }else{
                Login(client_obj);
            }
        }
        else if(client_obj.login){
            if(HTTP.path == "/favicon.ico"){
                Icon(client_obj);
            }else if(HTTP.path == "/home.html" and HTTP.value.empty()){
                Home(client_obj);
            }else if(HTTP.path == "/home.html?operation=add.html"){
                Add(client_obj);
            }else if(HTTP.path == "/home.html?operation=delete.html"){
                Delete(client_obj);
            }else if(HTTP.path == "/home.html?operation=chat.html"){
                Chat(client_obj);
            }else if(HTTP.path == "/chatlist"){
                Chatlist(client_obj, db);
            }else if(HTTP.path == "/deletelist"){
                Deletelist(client_obj, db);
            }else if(HTTP.path == "/home.html" and HTTP.value[0].first == "addfriend"){
                Addfriend(client_obj, HTTP, db);
            }else if(HTTP.path == "/home.html" and HTTP.value[0].first == "deletefriend"){
                Deletefriend(client_obj, HTTP, db);
            }else if(HTTP.path == "/chatroom.html"){
                Chatroom(client_obj, HTTP);
            }else if(HTTP.path == "/chathistory"){
                Chathistory(client_obj, db);
            }else if(HTTP.path == "/home.html" and HTTP.value[0].first == "chatmessage"){
                Chatmessage(client_obj, HTTP, db);
            }else{
                Home(client_obj);
            }
        }
    }
}

void header(char *http_header, int file_size, string file_type){
    char *header_template = "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %d\r\n\r\n";
    
    if(file_type == "text/html"){
        sprintf(http_header, header_template, file_type, file_size);
    }else if(file_type == "image/*"){
        sprintf(http_header, header_template, file_type, file_size);
    }else{
        sprintf(http_header, header_template, file_type, file_size);
    }
}
 
void openfile(client &client_obj, char* file_name, char* mode){
    struct stat sb;
    if(stat(file_name, &sb) == -1){
        perror("Fail to stat ");
    }

    client_obj.fp = fopen(file_name, mode);
    client_obj.file_size = (int) sb.st_size;
}

void closefile(client &client_obj){
    fclose(client_obj.fp);
    client_obj.fp = nullptr;
    client_obj.file_size = 0;
}

void getHTTP(http &http_obj, string &response){
    int i;
    int n = response.size();
    string tmp;
    for(i = 0; i < n; i ++){
        if(response[i] == ' ') break;
        tmp += response[i];
    }
    http_obj.method = tmp;
    tmp.clear();
    i ++;
    for(; i < n; i ++){
        if(response[i] == ' ') break;
        tmp += response[i];
    }
    http_obj.path = tmp;
    i = response.find("\r\n\r\n");
    i += 4;
    tmp.clear();
    pair<string, string> value;
    for(; i < n; i ++){
        if(response[i] == '='){
            value.first = tmp;
            tmp.clear();
        }else if(response[i] == '&'){
            value.second = tmp;
            http_obj.value.push_back(value);
            tmp.clear();
        }else{
            tmp += response[i];
            if(i == n - 1){
                value.second = tmp;
                http_obj.value.push_back(value);
                tmp.clear();
            }
        }
    }
}

void Login(client &client_obj){
    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./login.html", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "text/html");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

void sendfile(client &client_obj){
    int valread;
    char write_buffer[4096];
    memset(write_buffer, 0, 4096);
    while(valread = fread(write_buffer, sizeof(char), 4096, client_obj.fp)){
        send(client_obj.sock, write_buffer, valread, 0);
        memset(write_buffer, 0, 4096);
    }
}

void Icon(client &client_obj){
    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./icon.png", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "image/*");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

bool checkPassword(http &HTTP, sqlite3* db, client &client_obj){
    char sql_command[1024], sql_back[1024];
    char *err;
    int rc;

    sprintf(sql_command, "SELECT password FROM accounts WHERE account='%s';", HTTP.value[0].second.c_str());
    memset(sql_back, 0, 1024);
    rc = sqlite3_exec(db, sql_command, callback1, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to check password : %s\n", err);
    }else{
        if(strlen(sql_back)){
            string password = sql_back;
            if(password == HTTP.value[1].second){
                client_obj.login = true;
                client_obj.account = HTTP.value[0].second;
                return true;
            }
        }else{
            memset(sql_command, 0, 1024);
            sprintf(sql_command, "INSERT INTO accounts (account, password) VALUES('%s', '%s');", HTTP.value[0].second.c_str(), HTTP.value[1].second.c_str());
            rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
            if(rc != SQLITE_OK){
                fprintf(stderr, "Fail to insert account : %s\n", err);
            }else{
                client_obj.login = true;
                client_obj.account = HTTP.value[0].second;
                return true;
            }
        }
    }
    return false;
}

void Home(client &client_obj){
    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./home.html", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "text/html");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

void Add(client &client_obj){
    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./add.html", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "text/html");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

void Delete(client &client_obj){
    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./delete.html", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "text/html");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

void Chat(client &client_obj){
    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./chat.html", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "text/html");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

void Addfriend(client &client_obj, http &HTTP, sqlite3* db){
    int rc;
    char sql_command[1024], sql_back[1024];
    char *err;

    memset(sql_command, 0, 1024);
    sprintf(sql_command, "SELECT account FROM accounts WHERE account='%s';", HTTP.value[0].second.c_str());
    memset(sql_back, 0, 1024);
    rc = sqlite3_exec(db, sql_command, callback1, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to select account : %s\n", err);
    }else if(strlen(sql_back)){
        char name1[64], name2[64];
        strncpy(name1, client_obj.account.c_str(), 64);
        strncpy(name2, HTTP.value[0].second.c_str(), 64);
        
        memset(sql_command, 0, 1024);
        sprintf(sql_command, "INSERT INTO relations (client, friends, chatroom) VALUES('%s', '%s', '%s_%s');", name1, name2, name1, name2);
        rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
        memset(sql_command, 0, 1024);
        if(rc != SQLITE_OK){
            fprintf(stderr, "Fail to insert relation1 : %s\n", err);
        }
        sprintf(sql_command, "INSERT INTO relations (client, friends, chatroom) VALUES('%s', '%s', '%s_%s');", name2, name1, name1, name2);
        rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
        if(rc != SQLITE_OK){
            fprintf(stderr, "Fail to insert relation2 : %s\n", err);
        }

        memset(sql_command, 0, 1024);
        sprintf(sql_command, "CREATE TABLE IF NOT EXISTS %s_%s (sender varchar(255), content varchar(255));", name1, name2);
        rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
        if(rc != SQLITE_OK){
            fprintf(stderr, "Fail to create chatroom : %s\n", err);
        }
    }
    Home(client_obj);
}

void Chatlist(client &client_obj, sqlite3* db){
    int rc;
    char sql_command[1024], sql_back[1024];
    char *err;

    memset(sql_command, 0, 1024);
    sprintf(sql_command, "SELECT friends FROM relations WHERE client='%s';", client_obj.account.c_str());
    memset(sql_back, 0, 1024);
    rc = sqlite3_exec(db, sql_command, callback2, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to select friends : %s\n", err);
    }

    string data = sql_back, tmp = "";
    char tmp_buffer[1024], write_buffer[1024 << 3], http_header[256];
    memset(write_buffer, 0, 1024 << 3);
    for(int i = 0; i < data.size(); i ++){
        if(data[i] == ' '){
            memset(tmp_buffer, 0, 1024);
            sprintf(tmp_buffer, "<input type='radio' name='chatfriend' value='%s'> %s<br>", tmp.c_str(), tmp.c_str());
            strncat(write_buffer, tmp_buffer, 1024 << 3);
            tmp = "";
        }else{
            tmp += data[i];
        }
    }

    header(http_header, strlen(write_buffer), "*/*");
    send(client_obj.sock, http_header, strlen(http_header), 0);
    send(client_obj.sock, write_buffer, strlen(write_buffer), 0);
}

void Deletelist(client &client_obj, sqlite3* db){
    int rc;
    char sql_command[1024], sql_back[1024];
    char *err;

    memset(sql_command, 0, 1024);
    sprintf(sql_command, "SELECT friends FROM relations WHERE client='%s';", client_obj.account.c_str());
    memset(sql_back, 0, 1024);
    rc = sqlite3_exec(db, sql_command, callback2, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to select friends : %s\n", err);
    }

    string data = sql_back, tmp = "";
    char tmp_buffer[1024], write_buffer[1024 << 3], http_header[256];
    memset(write_buffer, 0, 1024 << 3);
    for(int i = 0; i < data.size(); i ++){
        if(data[i] == ' '){
            memset(tmp_buffer, 0, 1024);
            sprintf(tmp_buffer, "<input type='radio' name='deletefriend' value='%s'> %s<br>", tmp.c_str(), tmp.c_str());
            strncat(write_buffer, tmp_buffer, 1024 << 3);
            tmp = "";
        }else{
            tmp += data[i];
        }
    }

    header(http_header, strlen(write_buffer), "*/*");
    send(client_obj.sock, http_header, strlen(http_header), 0);
    send(client_obj.sock, write_buffer, strlen(write_buffer), 0);
}

void Deletefriend(client &client_obj, http &HTTP, sqlite3* db){
    int rc;
    char sql_command[1024], sql_back[1024];
    char *err;

    memset(sql_command, 0, 1024);
    sprintf(sql_command, "SELECT chatroom FROM relations WHERE client='%s' AND friends='%s';", client_obj.account.c_str(), HTTP.value[0].second.c_str());
    memset(sql_back, 0, 1024);
    rc = sqlite3_exec(db, sql_command, callback1, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to select chatroom : %s\n", err);
    }else{
        if(strlen(sql_back)){
            memset(sql_command, 0, 1024);
            sprintf(sql_command, "DELETE FROM relations WHERE chatroom='%s';", sql_back);
            rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
            if(rc != SQLITE_OK){
                fprintf(stderr, "Fail to delete relation : %s\n", err);
            }

            memset(sql_command, 0, 1024);
            sprintf(sql_command, "DROP TABLE %s;", sql_back);
            rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
            if(rc != SQLITE_OK){
                fprintf(stderr, "Fail to drop table : %s\n", err);
            }
        }
    }
    Home(client_obj);
}

void Chatroom(client &client_obj, http &HTTP){
    client_obj.chatfriend = HTTP.value[0].second;

    if(client_obj.fp == nullptr) {
        openfile(client_obj, "./chatroom.html", "r");
        char http_header[256];
        header(http_header, client_obj.file_size, "text/html");
        send(client_obj.sock, http_header, strlen(http_header), 0);
    }

    sendfile(client_obj);

    closefile(client_obj);
}

void Chathistory(client &client_obj, sqlite3* db){
    int rc;
    char sql_command[1024], sql_back[4096 << 3], http_header[256];
    char *err;

    memset(sql_command, 0, 1024);
    sprintf(sql_command, "SELECT chatroom FROM relations WHERE client='%s' AND friends='%s';", client_obj.account.c_str(), client_obj.chatfriend.c_str());
    memset(sql_back, 0, sizeof(sql_back));
    rc = sqlite3_exec(db, sql_command, callback1, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to select chatroom : %s\n", err);
    }else{
        if(strlen(sql_back)){
            memset(sql_command, 0, 1024);
            sprintf(sql_command, "SELECT sender,content FROM %s;", sql_back);
            memset(sql_back, 0, sizeof(sql_back));
            rc = sqlite3_exec(db, sql_command, callback3, (void*) sql_back, &err);
            if(rc != SQLITE_OK){
                fprintf(stderr, "Fail to select chat content : %s\n", err);
            }else{
                header(http_header, strlen(sql_back), "*/*");
                send(client_obj.sock, http_header, strlen(http_header), 0);
                send(client_obj.sock, sql_back, strlen(sql_back), 0);
            }
        }
    }
}

void Chatmessage(client &client_obj, http &HTTP, sqlite3* db){
    int rc;
    char sql_command[1024], sql_back[1024];
    char *err;

    memset(sql_command, 0, 1024);
    sprintf(sql_command, "SELECT chatroom FROM relations WHERE client='%s' AND friends='%s';", client_obj.account.c_str(), client_obj.chatfriend.c_str());
    memset(sql_back, 0, 1024);
    rc = sqlite3_exec(db, sql_command, callback1, (void*) sql_back, &err);
    if(rc != SQLITE_OK){
        fprintf(stderr, "Fail to select chatroom : %s\n", err);
    }else{
        if(strlen(sql_back)){
            memset(sql_command, 0, 1024);
            sprintf(sql_command, "INSERT INTO %s (sender, content) VALUES('%s', '%s');", sql_back, client_obj.account.c_str(), HTTP.value[0].second.c_str());
            rc = sqlite3_exec(db, sql_command, NULL, NULL, &err);
            if(rc != SQLITE_OK){
                fprintf(stderr, "Fail to insert chat content : %s\n", err);
            }
        }
    }
    client_obj.chatfriend = "";
    Home(client_obj);
}