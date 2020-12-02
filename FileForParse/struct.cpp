/*struct Struct
{
	int alo;
};*/

struct Data
{
	int *number;
	//Struct str;
	Data (int *n = nullptr) : number(n) {}
	
	Data &operator=(const Data &d){
		if(this == &d){
			return *this;
		}
		return *this;
	}
};

/*int * foo(int i)
{
	i = 1;
}*/

struct Node
{
	int data;
	int *i_ptr;
	Data *next;
	Data next_1;
	Node() {}
	Node(Data arg/*, Data *d = nullptr, int *d = nullptr, int *n_2 = nullptr*/){
		Data alo;
		alo = arg;
	}
};

int *createInt()
{
	return new int;
}

Node * foo(int *alo)
{
	alo = new int;
	
	delete [] alo;
	
	return new Node;
}

int main(int argc, char *argv[])
{
	Node *node;
	node = foo(createInt());
	// Node *alo = new Node;
	
	/*Node node;
	node.next_1.str.alo = 1;*/
	/*int data = 3;
	if(data == 2) {
		int i = 1;
	} else if(data == 1){
		int i = 2;
	}*/
	/*int data_1;
	int data_2;
	data_1 = data_2 = 1;*/
	
	return 0;
}