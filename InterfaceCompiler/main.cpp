#include <stdio.h> 
#include <string>
#include <vector>
#include <map>
#include <stack>

/*std::map<std::string,std::string> types = {
    {"byte", "uint8_t"},
    {"uint16", "uint16_t"},
    {"uint32", "uint32_t"},
    {"uint64", "uint64_t"},
    {"int16", "int16_t"},
    {"int32", "int32_t"},
    {"int64", "int64_t"},
};*/

enum TokenType{
    TokenEndline,
    TokenIdentifier,
    TokenKeyword,
    TokenTypename,
    TokenLeftBrace,
    TokenRightBrace,
    TokenLeftParens,
    TokenRightParens,
    TokenComma,
    KeywordInterface,
    KeywordSync,
    KeywordAsync,
    KeywordResponse,
};

enum StatementType {
    StatementDeclareInterface,
    StatementEnterScope,
    StatementExitScope,
    StatementAsyncCallDeclaration,
    StatementSyncCallDeclaration,
    StatementParameterList,
};

enum {
    ParserStateNone,
    ParserStateDeclarationAsync,
    ParserStateDeclarationSync,
    ParserStateDeclarationInterface,
    ParserStateParameterList,
};

#define IsDeclaration(x) ((x == ParserStateDeclarationSync) || (x == ParserStateDeclarationAsync) || (x == ParserStateDeclarationInterface))

struct Token{
    TokenType type;
    std::string value;
    int lineNum;

    Token(){}

    Token(int ln, TokenType type){
        lineNum = ln;
        this->type = type;
    }

    Token(int ln, TokenType type, std::string& text) : value(text) {
        lineNum = ln;
        this->type = type;
    }
};

struct ParserState{
    int state = ParserStateNone;
    bool identified = false;

    Token identifier;

    ParserState(int s){
        state = s;
    }
};

std::map<std::string, TokenType> keywords = {
    {"interface", KeywordInterface},
    {"sync", KeywordSync},
    {"async", KeywordAsync},
    {"response", KeywordResponse},
};

std::vector<Token> tokens;

void BuildTokens(std::string& input){
    int lineNum = 0;

    std::string buf;
    for(char c : input){
        auto appendIdentifier = [lineNum](std::string& buf) { 
            if(buf.length()){
                tokens.push_back(Token(lineNum, TokenIdentifier, buf));
                buf.clear();
            }
        };

        switch (c)
        {   
        case ',':
            tokens.push_back(Token(lineNum, TokenComma));
            appendIdentifier(buf);
            break;
        case ')':
            tokens.push_back(Token(lineNum, TokenRightParens));
            appendIdentifier(buf);
            break;
        case '(':
            tokens.push_back(Token(lineNum, TokenLeftParens));
            appendIdentifier(buf);
            break;
        case '}':
            tokens.push_back(Token(lineNum, TokenRightBrace));
            appendIdentifier(buf);
            break;
        case '{':
            tokens.push_back(Token(lineNum, TokenLeftBrace));
            appendIdentifier(buf);
            break;
        case '\n':
            lineNum++;
        case ' ':
            appendIdentifier(buf);
            break;
        default:
            buf += c;
            break;
        }
    }

    for(Token& tok : tokens){
        if(tok.type == TokenIdentifier){
            for(auto keyword : keywords){
                if(!tok.value.compare(keyword.first)){
                    tok.type = keyword.second;
                    break;
                }
            }
        }
    }
}

int main(int argc, char** argv){
    if(argc < 2){
        printf("Usage: %s <file>\n", argv[0]);
        exit(2);
    }

    FILE* inputFile;
    if(!(inputFile = fopen(argv[1], "r"))){
        perror("Error opening file for reading: ");
        exit(2);
    }

    std::string input;

    fseek(inputFile, 0, SEEK_END);
    size_t inputSz = ftell(inputFile);

    input.resize(inputSz);
    fseek(inputFile, 0, SEEK_SET);

    fread(&input.front(), 1, inputSz, inputFile);

    BuildTokens(input);

    std::stack<ParserState> parserState;
    parserState.push(ParserState(ParserStateNone));

    for(Token tok : tokens){
        switch(tok.type){
            case KeywordSync:
            case KeywordAsync:
                if(parserState.top().state == ParserStateNone){
                    parserState.push((tok.type == KeywordSync) ? ParserStateDeclarationSync : ParserStateDeclarationAsync);
                } else {
                    printf("error: [line %d] Unexpected declaration '%s'.\n", tok.lineNum, tok.value.c_str());
                    exit(1);
                }
                break;
            case KeywordInterface:
                if(parserState.top().state == ParserStateNone){
                    parserState.push(ParserState(ParserStateDeclarationInterface));
                } else {
                    printf("error: [line %d] Unexpected declaration '%s'.\n", tok.lineNum, tok.value.c_str());
                    exit(1);
                }
                break;
            case TokenIdentifier:
                if(IsDeclaration(parserState.top().state)){
                    parserState.top().identifier = tok;
                    parserState.top().identified = true;
                }
                break;
            case TokenLeftParens:
                if(IsDeclaration(parserState.top().state) && parserState.top().state != ParserStateDeclarationInterface){
                    if(!parserState.top().identified){
                        printf("error: [line %d] Expected identifier before '('.\n", tok.lineNum);
                        exit(1);
                    }
                } else {
                    printf("error: [line %d] Unexpected '('\n", tok.lineNum);
                    exit(1);
                }
                break;
        }
    }

    exit(0);
}