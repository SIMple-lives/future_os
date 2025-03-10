#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "global.h"

//将0强制转换为struct_type* 类型的指针，相当于struct_type结构体的起始地址，访问member成员,&取member的地址，转换为int取偏移量
#define offset(struct_type, member) (int)(&((struct_type*)0)->member) // 获取成员在结构体中的偏移量

#define elem2entry(struct_type, struct_member_name, elem_ptr) \
(struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

/**********   定义链表结点成员结构   ***********
*结点中不需要数据成元,只要求前驱和后继结点指针*/
//单纯为了将已有的数据以一定的时序连接起来
struct list_elem {
    struct list_elem* prev;		//前驱结点
    struct list_elem* next;		//后继结点
};

//链表实现队列
struct list {
    //有虚拟头结点
    struct list_elem head;		//头结点
    struct list_elem tail;		//尾结点
};

//自定义函数类型function,用于在list_traversal中做回调函数
typedef bool (function)(struct list_elem*, int arg);

void list_init(struct list* );
void list_insert_before(struct list_elem* before, struct list_elem* elem);		// 在before结点前插入elem结点
void list_push(struct list* plist, struct list_elem* elem);
void list_iterate(struct list* plist);
void list_append(struct list*plist, struct list_elem* elem);
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(struct list* plist);
bool list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
bool elem_find(struct list* plist, struct list_elem* obj_elem);
#endif