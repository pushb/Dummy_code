#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* This is generated by a template program */


typedef struct stu_s{
    int roll_no;
    int  status;
    int ratings;
}stu_t;


void get_data(int *count, void *data){
    stu_t *stu_data;
    stu_data = (stu_t*) malloc(sizeof(stu_t) * 10);

    stu_data[0].roll_no = 12;
    stu_data[0].status = 0;
    stu_data[0].ratings = 90;

    stu_data[1].roll_no = 121;
    stu_data[1].status = 0;
    stu_data[1].ratings = 92;

    *((stu_t **)data) = stu_data;
    *count = 2;
}

int main(){

    int count = 0 , i = 0;
    stu_t *stu_p;

    get_data(&count, &stu_p);

    for(i = 0; i < count; i ++){
        printf("%d %d %d\n", stu_p[i].roll_no,  stu_p[i].status, stu_p[i].ratings);
    }

    return 0;
}
