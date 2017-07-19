#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>


/*This skeleton code is auto generated*/

typedef struct STU_S{
    int rollno;
    char name[32];
    struct STU_S *next;
}STU_T;

int count(void);
typedef enum position_s{FIRST, INCR, LAST}position_t;

STU_T *stu_data;

void insert( int rollno, position_t pos,  char *name){
    STU_T *dummy, *iter;

    dummy = malloc(sizeof(STU_T));
    dummy->rollno = rollno;
    strcpy(dummy->name, name);

    if(stu_data == NULL){
        stu_data = dummy;
    }else{
        if(pos == FIRST){
            dummy->next = stu_data;
            stu_data = dummy;
        }else if (pos == LAST){
        for(iter = stu_data; iter->next; iter=iter->next);
        iter->next = dummy;
        }else if (pos == INCR){
            if(stu_data->rollno > rollno){
                dummy->next = stu_data;
                stu_data = dummy;
            }else{
                for(iter = stu_data; iter->next && (rollno > iter->next->rollno); iter=iter->next);
                dummy->next = iter->next;
                iter->next = dummy;
            }
        }
    }
}

void print(){
    STU_T *dummy;
    
    for (dummy = stu_data; dummy; dummy=dummy->next){
        printf("%d %s\n", dummy->rollno, dummy->name);
    }
    
    printf("Count :%d \n", count());
}

void delete(int rollno){
    STU_T *iter, *dummy, *target;

    if(stu_data->rollno == rollno){
        dummy = stu_data;
        stu_data = stu_data->next;
        free(dummy);
    }else{
        for(iter = stu_data; iter->next && iter->next->rollno != rollno; iter = iter->next);

        if(iter->next){
        dummy = iter->next;

        iter->next = iter->next->next;

        free(dummy);
        }
    }


}

int count(){
    int count = 0;
    STU_T *dummy;
    dummy = stu_data;

    for( ; dummy; dummy = dummy->next, count++);

    return count;
}

int delete_first(){

    STU_T *dummy;
    dummy = stu_data;

    stu_data = stu_data->next;
    free(dummy);

    return 0;
}

void swap(int data1, int data2){
    STU_T *node1, *prev1, *node2, *prev2, *next1;

    for(node1 = stu_data ; node1->next != NULL && node1->rollno != data1; prev1 =node1, node1 = node1->next);
    for(node2 = stu_data ; node2->next != NULL && node2->rollno != data2; prev2 =node2, node2 = node2->next);

    printf("prev1: %d node1: %d prev2: %d node2: %d\n", prev1->rollno, node1->rollno, prev2->rollno, node2->rollno);
    /*next2 = node2->next;
    node2->next = node1->next;
    prev1->next = node2;

    node1->next = next2;
    prev2->next = node1;
*/

    prev1->next = node2;
    prev2->next = node1;

    next1 = node1->next;
    node1->next = node2->next;
    node2->next = next1;
}

void reverse(){

    STU_T *node, *prev, *after;

    prev = NULL;
    node = stu_data;
    while(node){
        after = node->next;
        node->next = prev;

        prev = node;
        node = after;
    }

    stu_data = prev;
}
int main(){

    insert(22, LAST, "ramesh");
    delete(22);
    insert(24, LAST, "Pavan");
    insert(16, FIRST, "Ganesh");
    insert(19, INCR, "Suresh");
    insert(18, INCR, "harish");

    swap(18, 24);
    print();
    delete(19);
    delete(12);

    print();


    delete_first();
    print();

    reverse();
    print();
    return 0;
}


