/* Recursive Descent Parsers
Exercises for 2.4 in the dragon book
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
        do
        {
            *lookahead_ptr = getchar();
        } while (*lookahead_ptr == ' ');
        
        return;
    }
    fprintf(stderr, "Syntax Error\n");
}

Stree* S(char* la) {
    switch (*la) {
        case '+':
            match('+', la);
            printf("[+");
            Stree* plustree = emptyNode('+');
            plustree->left = S(la);
            plustree->right = S(la);
            printf("]");
            return plustree;
        case '-':
            match('-', la);
            printf("[-");
            Stree* minustree = emptyNode('-');
            minustree->left = S(la);
            minustree->right = S(la);
            printf("]");
            return minustree;
        case 'a':
            match('a', la);
            printf("a");
            Stree* atree = emptyNode('a');
            return atree;
        default:
            fprintf(stderr, "Syntax Error\n");
            return NULL;
    }
}


/* Grammar 2:
E -> E (E) E | ''

Some examples:
()()()
((((()))))
()(())()
()()(((())))(())()(())

Some non:
((((())))
()((())
(((.)))
*/

void E(char* la) {
    if (*la == '(') {
        match('(', la);
        printf("(");
        E(la);
        match(')', la);
        printf(")");
        E(la);
    }
}


int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Please choose a grammar\n");
        exit(1);
    }

    int grammar = atoi(argv[1]);

    char lookahead;

    while(1) {
        lookahead = getchar();

       if (lookahead == 'q') {
            break;
        }

        if (grammar == 1) {
            S(&lookahead);
        } else if (grammar == 2) {
            E(&lookahead);
        } else {
            fprintf(stderr, "Only grammars 1 and 2 are available\n");
            exit(1);
        }

        if (lookahead != '\n') {
            fprintf(stderr, "Syntax Error\n");
            while(lookahead != '\n') {
                lookahead = getchar(); //clear buffer
            }
        }


        printf("\n\n");
    }
    return 0;
}