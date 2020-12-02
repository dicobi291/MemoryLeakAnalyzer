//#include <stdio.h>
#include "header.hpp"
class CallMethodClass
{
public:
	CallMethodClass() : i(i), f(f) {}
	void Method() {
	
	}
	
private:
	int i;
	float f;
};

struct Struct{
	int data;
};

struct Node
{
	int data;
	Node *next;
	Struct str;
	Node(int d = 0, Node *n = nullptr, Struct s = Struct());
	CallMethodClass callMethodClass;
};

Node * getNode()
{
	
}

Node::Node(int d, Node *n, Struct s)  : data(false), next(getNode()), str(s) {}

struct List
{
	Node *head;
};

void foo(const List *l = nullptr, int *i = nullptr);

int foo_a(int a, int b);

void foo_ptr(int **a, int arr[]);

Node alo()
{
	
}

void function(char ch);

int main(int argc, char *argv[])
{
	int a;
	a = 1;
	List list;
	Node node{1, nullptr, Struct()};
	list.head = &node;
	list.head->callMethodClass.Method();
	int *i_ptr;
	int *i_ptr_ = new int;
	for(int i = 0; i < a; i++){}
//	List list;
//	List *list_1;
//	List *list_2 = new List;
	
	foo_a(1, 2);
	
	return 0;
}

void foo(const List *l, int *i)
{
	float *f;
}

int foo_a(int a, int b)
{
	int *i_ptr;
	int *i_ptr_ = new int;
	List list;
	List *list_1;
	List *list_2 = new List;
	
	return 0;
}

void function(char ch)
{
	int d;
}