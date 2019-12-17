//
// Created by shani on 15/12/2019.
//

#ifndef FLIGHT_SIMULATOR_SINGELTON_H
#define FLIGHT_SIMULATOR_SINGELTON_H

#include "Ex1.h"
#include <unordered_map>

class Singleton {
    static Singleton *instance;
    unordered_map<string,Variable*> progVars;
    unordered_map<string,Variable*> simVars;

    // Private constructor so that no objects can be created.
    Singleton() {}

public:
    static Singleton *getInstance() {
        if (!instance)
            instance = new Singleton;
        return instance;
    }

    unordered_map<string ,Variable*> getProg() {
        return this -> progVars;
    }

    unordered_map<string ,Variable*> getSim() {
        return this -> simVars;
    }

    /*void setProg(unordered_map<string ,Variable> data) {
        this -> progVars = data;
    }

    void setSim(unordered_map<string,Variable> data) {
        this -> simVars = data;
    }*/
};

#endif //FLIGHT_SIMULATOR_SINGELTON_H