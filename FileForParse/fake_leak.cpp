struct List {
    int val;
    List * nx;
};

void Clear(List** tab, int size)
{
    for(int i=0;i<size;i++) {
        tab[i] = nullptr;
    }
}

int main(int argc, char** argv)
{
    List ** mas;
    
    mas = new List*[5];
    
    for(int i=0;i<5; i++) {
        mas[i]  = new List;
    }
    
    List ** masTmp;
    
    masTmp = new List*[5];
    
    for(int i=0;i < 5; i++) {
        masTmp[i]  = mas[i];
    }
    
    Clear(mas, 5);
    List *head = nullptr;
    head = new List;
    head->nx = nullptr;
    List *p = head;
    for(int i=1; i<10;i++) {
        p->nx = new List;
        p=p->nx;
        p->nx = nullptr;       
    }
    
    List *tmp = head;
    tmp = tmp->nx->nx;
    //обрыв связи
    delete tmp;
	
    return 0;
}

