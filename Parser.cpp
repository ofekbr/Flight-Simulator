//
// Created by shani on 17/12/2019.
//

#include "Command.h"
#include "Parser.h"
#include "Lexer.h"

void Parser::parse(vector<string> tokens){

    for (int i = 0; i< tokens.size(); i++) {

        if (i == 25) {
            int c = 5;
        }


        auto token = tokens[i];
        // DEBUG cout << token << " " << tokens[i+1] << " " << tokens[i+2] << " " << tokens[i+3] << " " << tokens[i+4] << endl;
        auto it = m_commands.find(tokens[i]);
        if (it == m_commands.end()){
            it = m_commands.find("default");
        }
        Command *c = it->second;

        if (c != nullptr) {
            i += (c->execute(tokens, i));
        }

    }

}
