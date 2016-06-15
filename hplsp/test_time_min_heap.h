#ifndef _TIME_MIN_HEAP_
#define _TIME_MIN_HEAP_

#include <iostream>
#include <netinet/in.h>
#include <time.h>
using std::exception;

#define BUFFER_SIZE 64

class heap_timer;

/* 客户数据类型 */
typedef struct tag {
    struct sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer *timer;
} client_data;

/* 定时器类 */
class heap_timer {
  public:
    heap_timer(int delay) {
        expire = time(NULL) + delay;
    }

  public:
    time_t expire;                  // 定时器生效的绝对时间
    void (*cb_func)(client_data*);  // 定时器回调函数
    client_data *user_data;         // 用户数据
};

/* 时间堆类 */
class time_heap {
  public:
    time_heap(int cap) throw (std::exception) : capacity(cap), cur_size(0) {
        array = new heap_timer*[capacity];
        if(array == NULL)
            throw std::exception();
        for(int i = 0; i < capacity; ++i)
            array[i] = NULL;
    }

    time_heap(heap_timer **init_array, int size, int capacity) throw
            (std::exception) : capacity(size), cur_size(size) {
        if(capacity < size)
            throw std::exception();
        array = new heap_timer*[capacity];
        if(array == NULL)
            throw std::exception();
        for(int i = 0; i < capacity; ++i)
            array[i] = NULL;
        if(size != 0) {
            for(int i = 0; i < size; ++i)
                array[i] = init_array[i];
            for(int i = (cur_size-1)/2; i >= 0; --i)
                percolate_down(i);
        }
    }

    ~time_heap() {
        for(int i = 0; i < cur_size; ++i)
            delete array[i];
        delete [] array;
    }

    /* 添加目标定时器 */
    void add_timer(heap_timer *timer) throw (std::exception) {
        if(!timer) return ;
        if(cur_size >= capacity)  // 当前堆数组容量不够，则扩大1倍
            resize();
        int hole = cur_size++;
        int parent;
        for( ; hole > 0; hole = parent) {
            parent = (hole-1)/2;
            if(array[parent]->expire <= timer->expire)
                break;
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }

    /* 删除目标定时器 */
    void del_timer(heap_timer *timer) {
        if(!timer) return;
        timer->cb_func = NULL;
    }

    /* 获得堆顶的定时器 */
    heap_timer *top() const {
        if(empty())
            return NULL;
        return array[0];
    }

    /* 删除堆顶的定时器 */
    void pop_timer() {
        if(empty())
            return;
        if(array[0]) {
            delete array[0];
            array[0] = array[--cur_size];
            percolate_down(0);
        }
    }

    /* 心搏函数 */
    void tick() {
        heap_timer *tmp = array[0];
        time_t cur = time(NULL);
        while(!empty()) {
            if(!tmp)
                break;
            if(tmp->expire > cur)
                break;
            if(array[0]->cb_func)
                array[0]->cb_func(array[0]->user_data);
            pop_timer();
            tmp = array[0];
        }
    }

    bool empty() const {
        return cur_size == 0;
    }

  private:
    void percolate_down(int hole) {
        heap_timer *tmp = array[hole];
        int child = 0;
        for(; hole*2 + 1 <= cur_size-1; hole = child) {
            child = hole*2 + 1;
            if(child < cur_size-1 && array[child+1]->expire < array[child]->expire)
                ++child;
            if(array[child]->expire < tmp->expire)
                array[hole] = array[child];
            else
                break;
        }
        array[hole] = tmp;
    }

    void resize() throw (std::exception) {
        heap_timer **tmp = new heap_timer*[2*capacity];
        for(int i = 0; i < 2*capacity; ++i)
            tmp[i] = NULL;
        if(!tmp)
            throw std::exception();
        capacity = 2 * capacity;
        for(int i = 0; i < cur_size; ++i)
            tmp[i] = array[i];
        delete [] array;
        array = tmp;
    }

  private:
    heap_timer **array;  // 堆数组
    int capacity;        // 堆数组容量
    int cur_size;        // 当前堆数组包含的元素个数
};

#endif  /* _TIME_MIN_HEAP_ */
