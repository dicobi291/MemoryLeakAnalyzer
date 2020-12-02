struct Node
{
	Node *next;
	Node *prev;
};

struct List
{
	List *list;
	Node *head;
};

/*float * createFloat()
{
	return new float;
}

int * createInt(int *i)
{
	*i = 2;
	return new int;
}

char * createChar()
{
	return new char;
}*/

Node * createNode()
{
	Node *node = new Node;
	node->next = new Node;
	return node->next;
}

void allocate(int *i, float *f, Node *node)
{
	i = new int;
}

int main(int argc, char *argv[])
{
	Node *node;
	node = createNode();
	
	return 0;
}