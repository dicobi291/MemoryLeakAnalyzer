//#include <stdio.h>
//#include <stdlib.h>

//#define TRUE 1
//#define FALSE 0

struct Node
{
    int val;
    Node * nx;
};

Node* DevideTowoPart(Node *head, int count, Node **reserve)
{
    Node *part = nullptr, *p;

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
        p->nx =  nullptr;
    }
    *reserve = part;
    return nullptr;
    //return part;
}

void CreatePointerArray(Node ***arr, int size)
{
        *arr = new Node *[size];
}

void CreateList(Node **list, int size, int withVal)
{
        *list = new Node;
        (*list)->nx = nullptr;
        Node *tmp = *list;
        int i;
        for(i = 1; i < size; i++) {
                tmp->nx = new Node;
                tmp = tmp->nx;
                tmp->val = withVal ? i : 0;
                tmp->nx = nullptr;
        }
}

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
    CreateList(&list, 10, 1);

    Node *reserve;
    Node *part = DevideTowoPart(list, 5, &reserve);

    Node *node = new Node;

    ClearList(list);
//    ClearList(part);

        return 0;
}
