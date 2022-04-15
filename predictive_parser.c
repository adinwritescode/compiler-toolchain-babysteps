/* 
Predictive Parser for grammar consisting of single digit addition/subtraction
as described in the dragon book for compilers.
Adin Gitig, 4/14/22
*/

/*
Grammar:
expr -> expr + digit | expr - digit | digit
digit -> 0 | ... | 9
*/

enum lookahead = {
    plus,
    minus
};

void expr(enum lookahead) {
    switch lookahead {
        case plus:
            dig(i);
            match(plus);
            dig(i+1);
            break;
        case minus:
            dig(i);
            match(plus);
            dig(i+1);
            break;
        default:
            fprintf(stderr, "syntax error");

    }
}

void dig(int d) {
    printf("%d", d);
}

int main() {
    while(getline()) {
        if (line.token == PLUS || MINUS) {
            expr(token)
        }
    }
}