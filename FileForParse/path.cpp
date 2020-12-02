/*int getSize()
{
	return 10;
}

int *createInt(int size)
{
	return new int[size];
}

char getChar()
{
	return 'a';
}

void function(int *num, char ch)
{
	if(*num == 0) {
		*num = 1;
	} else {
		*num = 0;
	}
}

struct Node
{
	int i;
};
*/
/*void fun(int *i)
{
	if(*i == 1){
		return;
	}
	*i = 2;
}*/

struct Node
{
	Node *next;
};

int main()
{
	Node *node = new Node;
	
	std::srand(std::time(nullptr));
	int num = std::rand();
	
	if(num % 2) {
		delete node;
	}
	
	return 0;
}