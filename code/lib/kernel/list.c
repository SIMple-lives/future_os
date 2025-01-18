#include "list.h"
#include "interrupt.h"

//初始化双向链表
void list_init(struct list* list) {
    list->head.prev = NULL;
    list->head.next = &list->tail;        //初始化链表头, 头结点后继元素 为尾结点
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

//把链表元素elem插入到元素before之前
void list_insert_before(struct list_elem* before, struct list_elem* elem) {
    enum intr_status old_status = intr_disable(); //关中断

    //将berfore的前驱的next指向elem
    before->prev->next = elem;
    //将elem的前驱指向before的前驱
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;
    intr_set_status(old_status); //开中断
}

//添加元素到列表队首
void list_push(struct list* plist, struct list_elem* elem) {
    list_insert_before(plist->head.next,elem);
}

//要注意的是list里面存的是两个结构体元素，元素内的成员是指针。
void list_append(struct list* plist, struct list_elem* elem) {
    list_insert_before(&plist->tail,elem);
}

void list_remove(struct list_elem* pelem) {
    enum intr_status old_status = intr_disable(); //关中断

    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;

    intr_set_status(old_status); //开中断
}

struct list_elem* list_pop(struct list* plist) {
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;
}

bool elem_find(struct list* plist, struct list_elem* obj_elem) {
    struct list_elem* elem = plist->head.next;
    while(elem != &plist->tail) {
        if(elem == obj_elem) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}


/* 把列表plist中的每个元素elem和arg传给回调函数func,
 * arg给func用来判断elem是否符合条件.
 * 本函数的功能是遍历列表内所有元素,逐个判断是否有符合条件的元素。
 * 找到符合条件的元素返回元素指针,否则返回NULL. */
struct list_elem* list_traversal(struct list* plist, function func, int arg)
{
    struct list_elem* elem = plist->head.next;
    // 
    if( list_empty(plist)) {
        return NULL;
    }    

    while(elem != &plist->tail) {
        if(func(elem, arg)){
            return elem; // 找到返回
        }
        elem = elem->next;
    }
    return NULL;
}


uint32_t list_len(struct list* plist) {
    struct list_elem* elem = plist->head.next;
    uint32_t len = 0;
    while (elem != &plist->tail) 
    {
        len++;
        elem = elem->next;
    }
    return len;
}

bool list_empty(struct list* plist) {
    return (plist->head.next == &plist->tail ? true : false);    
}