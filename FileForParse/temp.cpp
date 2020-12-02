#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

typedef struct Node
{
    int val;
    struct Node * nx;
} Node;

struct Node* DevideTowoPart(struct Node *head, int count, Node **reserve)
{
    struct Node *part = NULL, *p;

    part = head;
    p = head;
    while(p &&  count - 1)
    {
        p = p->nx;
        count --;
    }

    if(p->nx)
    {
        part  = p->nx;
        p->nx =  NULL;
    }
    *reserve = part;
    return NULL;
    //return part;
}

void CreatePointerArray(Node ***arr, int size)
{
        *arr = new Node[size];
}

void CreateList(Node **list, int size, int withVal)
{
        *list = new Node;
        (*list)->nx = NULL;
        Node *tmp = *list;
        int i;
        for(i = 1; i < size; i++) {
                tmp->nx = (Node *) malloc(sizeof(Node));
                tmp = tmp->nx;
                tmp->val = withVal ? i : 0;
                tmp->nx = NULL;
        }
}

/*void PrintArray(Node **arr, int n)
{
    int i;
    for(i = 0; i < n; i++){
        printf("pointer(%p), Node(%p): (%d), nx(%p)\n", &arr[i], arr[i], arr[i] != NULL ? arr[i]->val : -1, arr[i]->nx);
    }
}

void PrintList(Node *l)
{
    if (l == NULL) {
        printf("list is empty\n");
        return;
    }
    Node *tmp = l;
    for(; tmp != NULL; ) {
        printf("Node(%p): val(%d), nx(%p)\n", tmp, tmp->val, tmp->nx);
        tmp = tmp->nx;
    }
}*/

void ClearArray(Node **arr, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        delete arr[i];
    }
}

void ClearList(Node *l)
{
    Node *tmp = l;
    for(; tmp != nullptr; ) {
        Node *node_to_delete = tmp;
        tmp = tmp->nx;
        delete node_to_delete;
    }
}

int main()
{
    Node *list;
    CreateList(&list, 10, TRUE);
    printf("list\n");
    PrintList(list);

    Node *reserve;
    Node *part = DevideTowoPart(list, 5, &reserve);
    /*printf("after deviding\n");
    printf("list\n");
    PrintList(list);
    printf("part");
    PrintList(part);*/

    Node *node = new Node;

    ClearList(list);
//    ClearList(part);

        return 0;
}
