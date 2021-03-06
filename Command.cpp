//
// Created by shani on 15/12/2019.
//

#include <netinet/in.h>
#include <unistd.h>
#include "Command.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#include "Parser.h"

mutex mutex_lock;

using namespace std;

/**
 * Create a new variable.
 * @param tokens
 * @param index to jump
 * @return
 */
int DefineVar::execute(vector<string> tokens, int index) {

    //define new var
    auto *var = new Variable();
    var->setName(tokens[index+1]);

    //syntax: var name <-> sim "path"
    if (tokens[index+2] == "->") {
        var->setToSim();
        var->setSim(tokens[index +4]);
        s->addVarProg(var);
    } else if (tokens[index+2] == "<-") {
        var->setSim(tokens[index +4]);
        s->addVarProg(var);
        s->addVarSim(var);
    } else if (tokens[index+2] == "=") {
        s->addVarProg(var);
        Command *c = new SetVar();
        //call set var to set a value to the new variable
        c->execute(tokens, index + 1);
        return 3;
    }
    return 4;
}

/**
 * Set a value to an existing variable.
 * @param tokens
 * @param index to jump
 * @return
 */
int SetVar::execute(vector<string> tokens, int index) {
    string setToSim;
    //making an expression from the expression
    auto *inter1 = new Interpreter();
    Expression *e = inter1->interpret(tokens[index + 2]);

    //set new value to the corresponding var
    if (s->getProg().find(tokens[index]) == s->getProg().end()){
        throw "No var in program";
    }
    auto var = s->getProg().find(tokens[index]);
    var->second->setValue(e->calculate());

    mutex_lock.lock();
    //add string to list of commands to sent to simulator
    if (var->second->isToSim()) {
        setToSim = "set " + var->second->getSim() + " " + to_string(var->second->getValue()) + "\r\n";
        s->addNewCommandToSend(setToSim);
    }
    mutex_lock.unlock();
    return 2;
}

/**
 * Execute the scope commands if the 'if' condition is true.
 * @param tokens
 * @param index to jump
 * @return
 */
int IfCommand::execute(vector<string> tokens, int index) {

    //call parent execute function
    ConditionParser::execute(tokens,index);

    if (m_condition->calculate() > 0) {
        Parser::parse(m_scopeTokens);
    }

    return m_indexToJump;
}

/**
 *  Execute the scope commands while the 'while' condition is true.
 * @param tokens
 * @param index to jump
 * @return
 */
int LoopCommand::execute(vector<string> tokens, int index) {
    //call parent execute function
    ConditionParser::execute(tokens,index);

    while (m_condition->calculate() > 0) {
        Parser::parse(m_scopeTokens);
    }
    return m_indexToJump;
}

/**
 * Create a list of commands of the condition scope.
 * Create an expresion representing the condition.
 * @param tokens
 * @param index to jump
 * @return
 */
int ConditionParser::execute(vector<string> tokens, int index) {
    //create boolean expression from condition:
    Interpreter interpretConditionl;
    m_condition = interpretConditionl.interpret(tokens[index +1]);


    //crate new array of tokens to the scope
    vector<string>::const_iterator first = tokens.begin() + index + 3;
    unsigned i = 0;
    for (i = index + 3; i< tokens.size(); i++) {
        if (tokens[i] == "}")
            break;
    }
    vector<string>::const_iterator last = tokens.begin() + i;
    vector<string> scopeTokens(first, last);
    m_scopeTokens = scopeTokens;
    m_indexToJump = (int) i - index;

    return 0;
}

/**
 * Print the value of the variable if exist or a string.
 * @param tokens
 * @param index to jump
 * @return
 */
int PrintCommand::execute(vector<string> tokens, int index) {
    //check if it is a string or a variable
    if (tokens[index +1][0] == '"') {
        cout<< tokens[index+1].substr(1, tokens[index+1].length() - 2)<<endl;
    } else  {
        cout<< (s->getProg()[tokens[index + 1]])->getValue() <<endl;
    }
    return 1;
}

/**
 * Sleep over the given time.
 * @param tokens
 * @param index
 * @return
 */
int SleepCommand::execute(vector<string> tokens, int index) {
    //convert string to int
    int time = stoi(tokens[index +1]);
    this_thread::sleep_for(chrono::milliseconds(time));
    return 1;
}

/**
 * Listen and connect the simulator.
 * Open new thread, reads the information the simulator sends and updates in the symble table.
 * @param tokens
 * @param index
 * @return
 */
int ServerCommand::execute(vector<string> tokens, int index) {
    index++;
    int port = stoi(tokens[index]);
    //create socket
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        //error
        std::cerr << "Could not create a socket"<<std::endl;
        return -1;
    }

    //bind socket to IP address
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(socketfd, (struct sockaddr *) &address, sizeof(address)) == -1) {
        std::cerr<<"Server - Could not bind the socket to an IP"<<std::endl;
        return -2;
    }

    //making socket listen to the port
    if (listen(socketfd, 5) == -1) {
        std::cerr<<"Server - Error during listening command"<<std::endl;
        return -3;
    } else{
        std::cout<<"Server is now listening ..."<<std::endl;
    }

    // accepting a client
    int client_socket = accept(socketfd, (struct sockaddr *)&address, (socklen_t*)&address);
    if (client_socket == -1) {
        std::cerr<<"Error accepting client"<<std::endl;
        return -4;
    }
    cout << "connection successful" << endl;
    close(socketfd); //closing the listening socket
    //s->createServer(client_socket, this)
    thread t1([this, client_socket] { ServerCommand::readData(client_socket); });
    t1.detach();
    cout <<"scope ended"<<endl;
    return index;
}

/**
 *
 * @param socket
 */
void ServerCommand::readData(int socket) {
    cout <<"thread entered"<<endl;
    int i;
    double value;
    Singleton *single = Singleton::getInstance();
    char buffer[1] = {0};
    char curVar[100] = {0};
    vector<double> variables;
    while (single->getCommunicationStatus()) {
        read(socket, buffer, 1);
        for (i = 0; buffer[0] != ','; i++) {
            if (buffer[0] == '\n') {
                break;
            }
            curVar[i] = buffer[0];
            read(socket, buffer, 1);
        }
        value = stod(curVar);
        variables.push_back(value);
        fill(curVar, curVar + i, 0);
        if (buffer[0] == '\n') {
            updateData(variables);
            variables.clear();
        }
    }
    close(socket);
    single->serverClosed(); //telling main that the server is ready to be close
}

/**
 *
 * @param vars
 */
void ServerCommand::updateData(vector<double> vars) {
    unsigned i;
    Singleton *s = Singleton::getInstance();
    unordered_map<string, Variable*> map = s->getSim();
    auto iter = map.end();
    for (i = 0; i < vars.size(); i++) {
        switch (i) {
            case 0:
                iter = map.find("/instrumentation/airspeed-indicator/indicated-speed-kt");
                break;
            case 1:
                iter = map.find("/sim/time/warp");
                break;
            case 2:
                iter = map.find("/controls/switches/magnetos");
                break;
            case 3:
                iter = map.find("/instrumentation/heading-indicator/offset-deg");
                break;
            case 4:
                iter = map.find("/instrumentation/altimeter/indicated-altitude-ft");
                break;
            case 5:
                iter = map.find("/instrumentation/altimeter/pressure-alt-ft");
                break;
            case 6:
                iter = map.find("/instrumentation/attitude-indicator/indicated-pitch-deg");
                break;
            case 7:
                iter = map.find("/instrumentation/attitude-indicator/indicated-roll-deg");
                break;
            case 8:
                iter = map.find("/instrumentation/attitude-indicator/internal-pitch-deg");
                break;
            case 9:
                iter = map.find("/instrumentation/attitude-indicator/internal-roll-deg");
                break;
            case 10:
                iter = map.find("/instrumentation/encoder/indicated-altitude-ft");
                break;
            case 11:
                iter = map.find("/instrumentation/encoder/pressure-alt-ft");
                break;
            case 12:
                iter = map.find("/instrumentation/gps/indicated-altitude-ft");
                break;
            case 13:
                iter = map.find("/instrumentation/gps/indicated-ground-speed-kt");
                break;
            case 14:
                iter = map.find("/instrumentation/gps/indicated-vertical-speed");
                break;
            case 15:
                iter = map.find("/instrumentation/heading-indicator/indicated-heading-deg");
                break;
            case 16:
                iter = map.find("/instrumentation/magnetic-compass/indicated-heading-deg");
                break;
            case 17:
                iter = map.find("/instrumentation/slip-skid-ball/indicated-slip-skid");
                break;
            case 18:
                iter = map.find("/instrumentation/turn-indicator/indicated-turn-rate");
                break;
            case 19:
                iter = map.find("/instrumentation/vertical-speed-indicator/indicated-speed-fpm");
                break;
            case 20:
                iter = map.find("/controls/flight/aileron");
                break;
            case 21:
                iter = map.find("/controls/flight/elevator");
                break;
            case 22:
                iter = map.find("/controls/flight/rudder");
                break;
            case 23:
                iter = map.find("/controls/flight/flaps");
                break;
            case 24:
                iter = map.find("/controls/engines/engine/throttle");
                break;
            case 25:
                iter = map.find("/controls/engines/current-engine/throttle");
                break;
            case 26:
                iter = map.find("/controls/switches/master-avionics");
                break;
            case 27:
                iter = map.find("/controls/switches/starter");
                break;
            case 28:
                iter = map.find("/engines/active-engine/auto-start");
                break;
            case 29:
                iter = map.find("/controls/flight/speedbrake");
                break;
            case 30:
                iter = map.find("/sim/model/c172p/brake-parking");
                break;
            case 31:
                iter = map.find("/controls/engines/engine/primer");
                break;
            case 32:
                iter = map.find("/controls/engines/current-engine/mixture");
                break;
            case 33:
                iter = map.find("/controls/switches/master-bat");
                break;
            case 34:
                iter = map.find("/controls/switches/master-alt");
                break;
            case 35:
                iter = map.find("/engines/engine/rpm");
                break;
            default:
                cout << "didnt find var in simVar - updateData  i = " << i << endl;
                continue;
        }
        if (iter == map.end()) {
            continue;
        } else {
            if (!iter->second->isToSim()) {
                iter->second->setValue(vars[i]);
            }
        }
    }
}

/**
 * Connect to client.
 * Create new thread, and send set commands.
 * @param tokens
 * @param index
 * @return
 */
int ConnectControlClient::execute(vector<string> tokens, int index) {

    //create socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        //error
        std::cerr << "Could not create a socket"<<std::endl;
        return -1;
    }

    //We need to create a sockaddr obj to hold address of server
    sockaddr_in address{}; //in means IP4
    address.sin_family = AF_INET;//IP4
    address.sin_addr.s_addr = inet_addr(tokens[index+1].substr(1, tokens[index+1].length()-2).c_str());  //the localhost address
    address.sin_port = htons(stoi(tokens[index +2]));
    //we need to convert our number (both port & localhost)
    // to a number that the network understands.

    // Requesting a connection with the server on local host with port 8081
    int is_connect = connect(client_socket, (struct sockaddr *)&address, sizeof(address));
    if (is_connect == -1) {
        std::cerr << "Could not connect to host server"<<std::endl;
        return -2;
    } else {
        std::cout<<"Client is now connected to server" <<std::endl;
    }


    thread t1([client_socket] { ConnectControlClient::sendCommands(client_socket); });
    t1.detach();
    return 2;
}

/**
 * Send the simulator set commands while there is in queue.
 * @param client_socket
 */
void ConnectControlClient::sendCommands(int client_socket) {
    Singleton *s = Singleton::getInstance();
    const char* commandToSend;
    string message;

    while (s->getCommunicationStatus()) {
            mutex_lock.lock();
            //Checks whether there is a command in queue to send
            if (!s->getCommandsToSend().empty()) {

                message = (s->getCommandsToSend().front());
                commandToSend = message.c_str();
                s->removeFrontCommand();

                //send the command
                ssize_t is_sent = write(client_socket, commandToSend, message.length());
                if (is_sent == -1) {
                    std::cout << "Error sending message" << std::endl;
                }
            }
            mutex_lock.unlock();
    }
    close(client_socket);
    s->clientClosed();
}

int createFuncCommand::execute(vector<string> tokens, int index) {
    int bracketsCounter = 0;

    vector<string>::const_iterator first = tokens.begin() + index + 4;
    int i = 0;
    for (i = index + 4; (unsigned) i < tokens.size(); i++) {
        if (tokens[i] == "{") {
            bracketsCounter++;
        }
        if (tokens[i] == "}") {
            if (bracketsCounter != 0) {
                bracketsCounter--;
                continue;
            } else {
                break;
            }
        }
    }
    auto *var = new Variable();
    var->setName(tokens[index+2]);
    Command::s->addVarProg(var);

    auto *var2 = new Variable();
    var2->setName(tokens[index]);
    Command::s->addVarProg(var2);
    var2->setName(tokens[index+2]);

    vector<string>::const_iterator last = tokens.begin() + i;
    vector<string> scopeTokens(first, last);

    //creating the func as a command
    Parser::addCommand(tokens[index],new funcCommand(scopeTokens));
    return i - index;
}

funcCommand::funcCommand(const vector<string>& scope) {
    m_scope = scope;
}

int funcCommand::execute(vector<string> tokens, int index) {
    auto funcVar = Command::s->getProg().find(tokens[index])->second; //takeoff
    auto argument = Command::s->getProg().find(funcVar->getName())->second; // X
    double value = stod(tokens[index+1]);
    argument->setValue(value);
    Parser::parse(this->m_scope);

    return 1;
}
