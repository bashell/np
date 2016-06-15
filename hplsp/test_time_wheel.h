#ifndef _TIME_WHEEL_H_
#define _TIME_WHEEL_H_

#include <time.h>
#include <stdio.h>
#include <netinet/in.h>

#define BUFFER_SIZE 64

class tw_timer;

/* 客户数据类型 */
typedef struct tag {
    struct sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer *timer;
} client_data;

/* 定时器类 */
class tw_timer {
  public:
    tw_timer(int rot, int ts) : rotation(rot), time_slot(ts), prev(NULL), next(NULL) {}

  public:
    int rotation;   // 定时器在时间轮转多少圈后生效
    int time_slot;  // 记录定时器属于哪个槽
    void (*cb_func)(client_data*);  // 定时器回调函数
    client_data *user_data;         // 客户数据
    tw_timer *prev;
    tw_timer *next;
};

/* 时间轮 */
class time_wheel {
  public:
    time_wheel() : cur_slot(0) {
        for(int i = 0; i < N; ++i)
            slots[i] = NULL;
    }

    ~time_wheel() {  // 遍历每个槽，并销毁槽中所有的定时器
        for(int i = 0; i < N; ++i) {
            tw_timer *tmp = slots[i];
            while(tmp) {
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    /* 根据timeout创建一个定时器，并插入到合适的槽中 */
    tw_timer *add_timer(int timeout) {
        if(timeout < 0) return NULL;
        int ticks = 0;
        // 根据待插入定时器的超时值计算它将在时间轮转动多少个滴答后被触发
        if(timeout < SI)
            ticks = 1;
        else
            ticks = timeout / SI;
        // 计算待插入的定时器在时间轮转动多少圈后被触发
        int rotation = ticks / N;
        // 计算待插入的定时器应该被插入到哪个槽中
        int ts = (cur_slot + (ticks % N)) % N;

        tw_timer *timer = new tw_timer(rotation, ts);
        if(!slots[ts]) {
            printf("add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot);
            slots[ts] = timer;
        } else {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }

    /* 删除目标定时器 */
    void del_timer(tw_timer *timer) {
        if(!timer) return;
        int ts = timer->time_slot;
        if(timer == slots[ts]) {
            slots[ts] = slots[ts]->next;
            if(slots[ts])
                slots[ts]->prev = NULL;
            delete timer;
        } else {
            timer->prev->next = timer->next;
            if(timer->next)
                timer->next->prev = timer->prev;
            delete timer;
        }
    }

    /* SI时间到后，用该函数使得时间轮向前滚动一个槽的间隔 */
    void tick() {
        tw_timer *tmp = slots[cur_slot];
        printf("curent slot is %d\n", cur_slot);
        while(tmp) {
            printf("tick the timer once\n");
            if(tmp->rotation > 0) {
                --tmp->rotation;
                tmp = tmp->next;
            } else {
                tmp->cb_func(tmp->user_data);
                if(tmp == slots[cur_slot]) {
                    printf("delete header in cur_slot\n");
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if(slots[cur_slot])
                        slots[cur_slot]->prev = NULL;
                    tmp = slots[cur_slot];
                } else {
                    tmp->prev->next = tmp->next;
                    if(tmp->next)
                        tmp->next->prev = tmp->prev;
                    tw_timer *tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        cur_slot = ++cur_slot % N;  // 更新时间轮的当前槽
    }

  private:
    static const int N = 60;  // 槽数目
    static const int SI = 1;  // 每1s时间轮转动一次
    tw_timer *slots[N];       // 时间轮的槽，每个元素指向一个定时器链表
    int cur_slot;             // 时间轮的当前槽
};


#endif  /* _TIME_WHEEL_H_ */
