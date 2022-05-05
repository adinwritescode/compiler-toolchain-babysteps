/* Lexer for simple Qadin language
4/29/2022
Adin Gitig
Sources: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl01.html
*/

using namespace std;

// Token Dict
enum Token {
    tok_eof = -1,

    // Commands
    tok_gate = -2,
    tok_extern = -3,
    /* More reserved keywords would be addressed here */

    // Term
    tok_id = -4,
    tok_num = -5,
};

static string IdStr; // Global, metadata for tok_id
static double NumVal; // Global, metadata for tok_num

// Lexer, or gettok() function
static int lexer() {
    static int LastChar = ' ';

    while (isspace(LastChar)) { // Loop to skip whitespace btwn tok's
        LastChar = getchar();
    }

    if (isalpha(LastChar)) { // Loop to get identifiers (must begin with a letter)
        
        /* Build out IdStr */
        IdStr = LastChar;
        while (isalnum((LastChar = getchar()))) {
            IdStr += LastChar;
        }

        /* Check against reserved keywords */
        if (IdStr == "gate") {
            return tok_gate;
        } else if (IdStr == "extern") {
            return tok_extern;
        } else {
            return tok_id; // Is some variable identifier
        }

    } else if (isdigit(LastChar) || LastChar == '.') { // Numbers of form x.y
        string NumStr;
        bool one_dec = (LastChar == '.'); // True => decimal num

        do {
            NumStr += LastChar;
            one_dec = one_dec || (LastChar == '.');
            LastChar = getchar();
        } while (isdigit(LastChar) || (LastChar == '.' && !one_dec));

        NumVal = strtod(NumStr.c_str(), 0);
        return tok_num;

    } else if (LastChar == '#') { // Comments

        do {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF) {
            return lexer(); // On next line
        } else {
            return tok_eof;
        }

    } else if (LastChar == EOF) { // EOF
        return tok_eof;
    } else { // Some ASCII character like ';', '+' etc., which we treat as its own tok
        int ThisChar = LastChar;
        LastChar = getchar();
        return ThisChar; // Some tok identifier 0 <= tok <= 255, its ASCII value
    }
}