typedef struct Stree Stree;
struct Stree {
    char term;
    Stree* left;
    Stree* right;
};

Stree* emptyNode(char c) {
    Stree* new = (Stree*) malloc(sizeof(Stree));
    if (!new) {
        fprintf(stderr, "emptyNode: failed to allocate\n");
        exit(1);
    }
    new->term = c;
    new->left = new->right = NULL;
    return new;
}

void freeTree(Stree* t) {
    if(t) {
        freeTree(t->left);
        freeTree(t->right);
        freeTree(t);
    }
    return;
}

void printTree(Stree* t) {

}

/*
    C
   / \
  B   D
 /     \
A 


*/
