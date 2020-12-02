struct Node
{
	int data;
	Node *next;
};

struct List
{
	Node *head;
	Node *tail;
};

int main()
{
	Node node;
	List list;
	
	int a = 1;
	node.data = a;
	
	return 0;
}