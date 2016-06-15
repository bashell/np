#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>

#define BUFFER_SIZE 64

class util_timer;

/* 用户数据结构 */
typedef struct tag {
    struct sockaddr_in address;  // 客户端socket地址
    int sockfd;                  // socket文件描述符
    char buf[BUFFER_SIZE];       // 读缓存
    util_timer *timer;           // 定时器
} client_data;

/* 定时器 */
class util_timer {
  public:
    util_timer() : prev(NULL), next(NULL) {}
  
  public:
    time_t expire;  // 任务超时时间（这里使用绝对时间）
    void (*cb_func)(client_data*);  // 任务回调函数
    client_data *user_data;  // 回调函数处理的客户数据（作为参数传入cb_func）
    util_timer *prev;
    util_timer *next;
};

/* 定时器链表 */
class sort_timer_lst {
  public:
    sort_timer_lst() : head(NULL), tail(NULL) {}
    ~sort_timer_lst() {  // 销毁整个链表
        util_timer *tmp = head;
        while(tmp) {
            head = head->next;
            delete tmp;
            tmp = head;
        }
    }
    // 向链表添加定时器timer
    void add_timer(util_timer *timer) {  
        if(!timer) return;
        if(!head) {
            head = tail = timer;
            return ;
        }
        if(timer->expire < head->expire) {  // 插到链表头
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);  // 插入到合适位置
    }

    /* 某个定时任务发生变化时， 调整该任务在链表中的位置（假设只考虑超时时间延长情况) */
    void adjust_timer(util_timer *timer) {
        if(!timer) return;
        util_timer *tmp = timer->next;
        // 待调整的定时器处于链表尾部或者未超过下一个定时器的超时时间，则不需要调整
        if(!tmp || (timer->expire < tmp->expire)) 
            return;
        if(timer == head) {  // 待调整的定时器为链表表头
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        } else {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    /* 将目标定时器从链表中删除 */
    void del_timer(util_timer *timer) {
        if(!timer) return;
        if(timer == head && timer == tail) {  // 链表中仅有一个结点
            delete timer;
            head = NULL, tail = NULL;
            return ;
        }
        if(timer == head) {  // 在表头删除
            head = head->next;
            head->prev = NULL;
            delete timer;
            return ;
        }
        if(timer == tail) {  // 在表尾删除
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return ;
        }
        // 在中间某处删除
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    /* 找到第一个尚未到期的定时器 */
    void tick() {
        if(!head) return;
        printf("timer tick\n");
        time_t cur = time(NULL);
        util_timer *tmp = head;
        while(tmp) {
            if(cur < tmp->expire)  // 判断定时器是否到期
                break;
            tmp->cb_func(tmp->user_data);  // 调用定时器的回调函数，执行定时任务
            head = tmp->next;
            if(head)
                head->prev = NULL;
            delete tmp;
            tmp = head;
        }
    }

  private:
    void add_timer(util_timer *timer, util_timer *lst_head) {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        while(tmp) {
            if(timer->expire < tmp->expire) {  // 找到了合适的插入点
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if(!tmp) {  // 插在链表尾部
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

  private:
    util_timer *head;
    util_timer *tail;
};

#endif  /* LST_TIMER */
