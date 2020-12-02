// ConsoleApplication1.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include "pch.h"
#include <iostream>

#include <stdio.h>
#include <stdlib.h>

typedef struct _Node
{
	int key;                // Значение.
	struct _Node *next;     // Указатель на следующий элемент.
	struct _Node *prev;     // Указатель на последний элемент.
} Node;



typedef struct _List
{
	size_t size;            // Размер списка
	Node *head;             // Указатель на голову списка (начало).
	Node *tail;             // Указатель на хвост списка (конец).
} List;


// Создание списка.
List* create_list()
{
	List *tmp = (List*)malloc(sizeof(List));       // Выделяем память под список.

	// Задаем начальные значения списка (по умолчанию).
	tmp->size = 0;                                  // Изначально размер списка равен нулю.
	tmp->head = NULL;                               // Указатель NULL - нулевой указатель,
	tmp->tail = NULL;                               // голова и хвост списка ни на что не указывают в памяти.

	// Возвращаем адрес списка
	return tmp;
}


// Освобождение памяти, выделенной под список.
void delete_list(List **list)
{
	Node *tmp = (*list)->head;  // Будем начинать просмотр списка с головы.
	Node *next;

	// Просматриваем весь список пока tmp
	while (tmp)
	{
		next = tmp->next;       // Получаем адрес следующего элемента.
		free(tmp);              // Освобождаем память текущего элемента.
		tmp = next;             // Следующий элемент становится текущим.
	}

	free(*list);        // Освобождаем память выделенную под список
	(*list) = NULL;
}


// Добавляем узел в начало списка.
void push_front(List *list, int data)
{
	Node *tmp = (Node*)malloc(sizeof(Node));   // Выделение памяти под новый узел.
	if (tmp == NULL)                            // Если нет возможности выделить новую память,
		throw 0;                                // то завершение работы программы.

	// Инициализируем новый список начальными значениями.
	tmp->key = data;                            // Кладем в узел значение.
	tmp->next = list->head;                     // следующий за ним - head,
	tmp->prev = NULL;                           // а предыдущего нет.

	// Если список не пустой был
	if (list->head)
		list->head->prev = tmp;     // предыдущим элементом прошлой головы становится новый узел.

	list->head = tmp;   // Добавляемый новый узел становится головой списка.

	// Если список был пустой, то добавляемый узел первый.
	if (list->tail == NULL)
		list->tail = tmp;

	list->size++;   // увеличиваем кол-во элементов списка на единицу.
}


// Удаляем узел из начала списка и возвращаем значение удаляемого элемента.
int pop_front(List *list)
{
	Node *prev;
	int tmp;

	// Если список был пустым - ошибка.
	if (list->head == NULL)
		throw 2;

	prev = list->head;                  // prev - удаляемый элемент. сохраним его.
	list->head = list->head->next;      // указатель на голову списка

	// Если список существовал, предыдущий элемент NULL
	if (list->head)
		list->head->prev = NULL;

	// Если список был всего из одного элемента
	if (prev == list->tail)
		list->tail = NULL;

	// Сохраняем значение которое должны были вернуть.
	tmp = prev->key;
	free(prev);     // Освобождаем память занимаемое эти листом

	list->size--;   // Уменьшаем размер списка.
	return tmp;     // Возвращаем значение удаленного элемента.
}


// Добавить элемент в конец списка 
void push_back(List *list, int key)
{
	Node *tmp = (Node*)malloc(sizeof(Node));  // Выделяем память для нового элемента
	if (tmp == NULL)                           //  Если нет возможность выделить память то ошибка
		throw 2;

	// Задаем значения добавляемому элементу.
	tmp->key = key;
	tmp->next = NULL;
	tmp->prev = list->tail;

	// Если элемент не первый
	if (list->tail)
		list->tail->next = tmp;

	list->tail = tmp;

	if (list->head == NULL)
		list->head = tmp;

	list->size++;   // Увеличиваем размер списка.
}


// Удаление элемента с конца списка.
int pop_back(List *list)
{
	Node *next;
	int tmp;

	// Если список пуст то выход.
	if (list->tail == NULL)
		throw 2;

	// Сохраняем в next удаляемый элемент.
	next = list->tail;
	list->tail = list->tail->prev;    //  Хвост списка теперь предыдущий элемент.
	if (list->tail)
		list->tail->next = NULL;

	if (next == list->head)
		list->head = NULL;

	tmp = next->key;    // То что вернем сохраним.
	free(next);         // Освободим выделенную память.

	list->size--;       // Уменьшим размер списка.

	return tmp;
}


// Получить элемент списка по индексу.
Node* get_node(List *list, size_t index)
{
	Node *tmp = list->head;
	size_t i = 0;

	// Список просматриваем пока не обойдем весь список или не найдем нужный элемент.
	while (tmp && i < index)
	{
		tmp = tmp->next;
		i++;
	}

	return tmp;
}


// Печатаем список на экран.
void print_list(List *list)
{
	printf("\n");

	Node* tmp = list->head;             // Просмотр списка с головы.
	while (tmp)
	{
		printf("%d ", tmp->key);        // Печатаем элемент списка.
		tmp = tmp->next;                // Переходим на след. элемент списка.
	}

	printf("\n");
}


// Добавление элемента перед заданным
void append_prev(List *list, Node *cur, int value) // list - список, node - узел, перед которым добавляем, value - что добавляем
{
	Node *prev = cur->prev;

	// Создаем новый узел, которй будем добавлять.
	Node* New = (Node*)malloc(sizeof(Node));   // Выделяем память для нового элемента.
	if (!New) throw 1;               // Если память не была выделена.

	// Инициализируем узел начальными значениями, связывая его с соседними элементами.
	New->key = value;
	New->prev = prev;
	New->next = cur;

	// Соединяем граничные элементы списка с новым.
	if (prev) prev->next = New; // если добавляем элемент перед первым эл-м в списке.
	else list->head = New; // если элемент первый он становится головой.
	cur->prev = New;

	list->size = list->size + 1; // Не забываем увеличить размер списка на единичку.
}


// Удаление элемента перед заданным
void delete_prev(List *list, Node *cur) // list - список, node - узел, перед которым удаляем.
{
	Node *prev = cur->prev;

	// Если перед этим узлом нет элементов, то ошибка.
	if (!cur->prev) throw 0;

	// Соединяем граничные элементы.
	if (prev->prev) prev->prev->next = prev->next; // Проверка на случае если удаляем голову списка
	else list->head = cur; // Если удалили голову.
	prev->next->prev = prev->prev;

	free(prev); // Освобождаем память

	list->size = list->size - 1; // Уменьшаем размер списка на единичку.
}


// Добавление элемента после заданного
void append_next(List *list, Node *cur, int value) // list - список, node - узел, после которого добавляем, value - что добавляем
{
	Node *next = cur->next;

	// Создаем новый узел, которй будем добавлять.
	Node *New = (Node*)malloc(sizeof(Node));   // Выделяем память для нового элемента.
	if (!New) throw 1;               // Если память не была выделена.

	// Инициализируем узел начальными значениями, связывая его с соседними элементами.
	New->key = value;
	New->prev = cur;
	New->next = next;

	// Соединяем граничные элементы списка с новым.
	if (next) next->prev = New; // если мы добавляем элемент после последнего  в  списке.
	else list->tail = New; // если элемент первый он становится головой.
	cur->next = New;

	list->size = list->size + 1; // увеличиваем размер списка на единичку.
}


// Удаление элемента после заданного
void delete_next(List *list, Node *cur) // list - список, node - узел, после которого удаляем.
{
	Node *next = cur->next;

	// Если после этого узла нет элементов, то ошибка.
	if (!cur->next) throw 0;

	// Соединяем граничные элементы.
	if (next->next) next->next->prev = next->prev; // Проверка на случае если удаляем голову списка
	else list->tail = cur; // Если удалили голову.
	next->prev->next = next->next;

	free(next); // Освобождаем память

	list->size = list->size - 1; // Уменьшаем размер списка на ед.
}


// Добавление элемента перед заданным
void insert(List *list, size_t index, int value)
{
	if (index > list->size) throw 2; // не корректный индекс

	// Создаем новый узел, которй будем добавлять.
	Node *New = (Node*)malloc(sizeof(Node));   // Выделяем память для нового элемента.
	if (!New) throw 1;               // Если память не была выделена.

	// Если добавляем в голову
	if (index == 0)
	{
		push_front(list, value);
		return;
	}
	// Если добавляем в конец
	else if (index == list->size)
	{
		push_back(list, value);
		return;
	}

	Node *tmp = list->head;
	size_t i = 1;

	// Ищем нужный элемент.
	while (i < index)
	{
		tmp = tmp->next;
		i++;
	}

	// Инициализируем узел начальными значениями, связывая его с соседними элементами.
	New->key = value;
	New->prev = tmp;
	New->next = tmp->next;

	// Соседние элементов с новым.
	tmp->next = New;
	tmp->next->prev = New;

	list->size = list->size + 1;
}


// Удаление элемента по индексу
void Delete(List *list, size_t index)
{
	if (index > list->size) throw 2; // Некорректный индекс

	// Если удаляем из головы
	if (index == 0)
	{
		pop_front(list);
		return;
	}
	// Если удаление с конца
	else if (index == list->size - 1)
	{
		pop_back(list);
		return;
	}

	// Ищем нужный элемент.
	Node *tmp = list->head;
	size_t i = 1;
	while (i < index)
	{
		tmp = tmp->next;
		i++;
	}

	tmp->next->prev = tmp->prev;
	tmp->prev->next = tmp->next;
	free(tmp);

	list->size = list->size - 1;
}






int main()
{
	List *list = create_list();
	try
	{

		push_front(list, 3);
		push_front(list, 2);
		push_front(list, 1);
		push_back(list, 4);

		Delete(list, 2);
		print_list(list);
		

		push_back(list, 5);
		push_back(list, 6);
		print_list(list);
		

		// Тестируем с середины списка.
		Node* node = get_node(list, 3);
		printf("%d ", node->key);

		append_prev(list, node, 13);
		append_next(list, node, 14);
		print_list(list);
		

		// Тестируем с начала списка.
		node = get_node(list, 0);
		printf("%d ", node->key);
		append_prev(list, node, 666);
		print_list(list);
		


		// Тестируем с конца списка.
		node = get_node(list, list->size - 1);
		printf("%d ", node->key);
		append_next(list, node, 777);
		print_list(list);
		


		// Протестируем удаление элемента перед заданным.
		node = get_node(list, 4);
		delete_prev(list, node);
		print_list(list);
		

		node = get_node(list, 1);
		delete_prev(list, node);
		print_list(list);
		


		// Протестируем удаление элемента после заданного.
		node = get_node(list, 4);
		delete_next(list, node);
		print_list(list);
		

		node = get_node(list, list->size - 2);
		delete_next(list, node);
		print_list(list);
		


		// Тестирование вставки на место.
		insert(list, 0, 1999);
		print_list(list);
		

		insert(list, 3, 1999);
		print_list(list);
		

		insert(list, list->size, 1999);
		print_list(list);



		// Тестирование удаления
		Delete(list, 0);
		print_list(list);


		Delete(list, 3);
		print_list(list);
		

		Delete(list, list->size - 1);
		print_list(list);
		

		delete_list(&list);


	}
	catch (int i)
	{
		switch (i)
		{
		case(1):
			printf("no memory allocated");
		case(2):
			printf("empty list");
		case(0):
			printf("missing an element");
		}


	}
	system("pause");
	return 0;
}


