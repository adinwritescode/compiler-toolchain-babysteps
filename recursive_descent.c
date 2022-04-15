/* Recursive Descent Parsers
Inspired by Exercises for 2.4 in the dragon book
Adin Gitig
4/14/22
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "Stree.c"

/*
Grammar 1:
S -> + S S | - S S | a

Some examples:
+ a a
- + a a a
+ - a a - a + a a
- - - - a a - a a + a + a + a a - a a

And some non-examples:
- a + a
+ - a a
a + a
+ + + + + a a a a - a a - a
*/

void match (char t, char* lookahead_ptr) {
    if (*lookahead_ptr == t) {
        *lookahead_ptr = getchar();
        return;
    }
    fprintf(stderr, "Syntax Error\n");
}

Stree* S(char* la) {
    switch (*la) {
        case '+':
            match('+', la);
            printf("+");
            Stree* plustree = emptyNode('+');
            plustree->left = S(la);
            plustree->right = S(la);
            return plustree;
        case '-':
            match('-', la);
            printf("-");
            Stree* minustree = emptyNode('-');
            minustree->left = S(la);
            minustree->right = S(la);
            return minustree;
        case 'a':
            match('a', la);
            printf("a");
            Stree* atree = emptyNode('a');
            return atree;
        case ' ':
            *la = getchar();
            return S(la);
        default:
            fprintf(stderr, "Syntax Error\n");
            return NULL;
    }
}


int main(int argc, char** argv) {
    while(1) {
        char lookahead = getchar();

       if (lookahead == 'q') {
            break;
        }

        Stree* ast = NULL;

        ast = S(&lookahead);

        if (lookahead != '\n') {
            fprintf(stderr, "Syntax Error\n");
        }

        printf("%c", lookahead);
    }
    return 0;
}