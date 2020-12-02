struct Node
{
	Node *next;
};

struct List
{
	Node *head;
};

int main(int argc, char *argv[])
{
	List *list = new List;
	list->head = new Node;
	list->head->next = new Node;
	list->head->next->next = new Node;
	
	delete list->head;
	
	return 0;
}

