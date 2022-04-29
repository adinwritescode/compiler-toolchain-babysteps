/* 
Predictive Parser for grammar consisting of single digit addition/subtraction
as described in the dragon book for compilers.
Adin Gitig, 4/14/22
*/

#include <stdio.h>
#include <stdlib.h>

/*
Grammar:
expr -> expr + term {print('+')}
      | expr - term {print('-')}
      | term
term -> 0 | ... | 9 {print('0')}

Some examples:

1 + 2
9 - 5 + 2
3 - 4 + 6 - 8 + 2
*/

void match (char t, char* lookahead_ptr) {
    if (*lookahead_ptr == t) {
        do
        {
            *lookahead_ptr = getchar();
        } while (*lookahead_ptr == ' ');
        
        return;
    }
    fprintf(stderr, "Syntax Error\n");
}

void dig(int d, char* lookahead) {
    printf("%d", d);
    *lookahead = getchar();
}

void expr(char* la) {
    dig(atoi(la), la);
    while(1) {
        switch (*la) {
            case '+':
                match('+', la);
                dig(atoi(la), la);
                printf("+");
                break;
            case '-':
                match('-', la);
                dig(atoi(la), la);
                printf("-");
                break;
            case ' ':
                *la = getchar();
                continue;
            default:
                return;
        }
    }
}

int main() {
    char lookahead;
    
    while(1) {
        lookahead = getchar();

        if (lookahead == 'q') {
            break;
        }

        expr(&lookahead);
        printf("\n");
    }

    return 0;
}