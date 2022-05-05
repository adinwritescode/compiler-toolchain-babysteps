/* Tree support for parsers
Adin Gitig
Tree visualization algorithm from:
 https://stackoverflow.com/questions/801740/c-how-to-draw-a-binary-tree-to-the-console
4/17/22
*/

#include <math.h>

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


int treeLen(Stree* t) {
    if (!t) {
        return 0;
    }
    return 1 + treeLen(t->left) + treeLen(t->right);
}

Stree** createQueue(int* front, int* rear, int len)
{
	Stree** queue = (Stree**)malloc(sizeof(Stree*) * len);

	*front = *rear = 0;
	return queue;
}

void enQueue(Stree** queue, int* rear, Stree* new_node)
{
	queue[*rear] = new_node;
	(*rear)++;
}

Stree* deQueue(Stree** queue, int* front)
{
	(*front)++;
	return queue[*front - 1];
}

Stree** tree_to_arr(Stree* t, int len)
{
	int rear, front, i;
    i = 0;

	Stree** queue = createQueue(&front, &rear, len);
    Stree** arr = (Stree**)malloc(sizeof(Stree*) * len);
	Stree* temp = t;

	while (temp) {
		arr[i] = temp;

		/*Enqueue left child */
		if (temp->left)
			enQueue(queue, &rear, temp->left);

		/*Enqueue right child */
		if (temp->right)
			enQueue(queue, &rear, temp->right);

		/*Dequeue node and make it temp_node*/
		temp = deQueue(queue, &front);

        i++;
	}

    free(queue);
    return arr;
}


void printTree(Stree* t) {
    int len = treeLen(t);
    Stree** arr = tree_to_arr(t, len);

    int print_pos[len];
    int i, j, k, pos, x=1, level=0;

    print_pos[0] = 0;
    for(i=0, j=1; i<len; i++,j++) {
        pos = print_pos[(i-1)/2] + (i%2?-1:1)*(70/(pow(2,level+1))+1);

        for (k=0; k<pos-x; k++) printf("%c",i==0||i%2?' ':'~');
        printf("%c",arr[i]->term);

        print_pos[i] = x = pos+1;
        if (j==pow(2,level)) {
            printf("\n");
            level++;
            x = 1;
            j = 0;
        }
    }

    free(arr);
}

/*
    C
   / \
  B   D
 /     \
A 


*/