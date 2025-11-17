#pragma once
#include<iostream>
#include<thread>
#include<mutex>
using namespace std;
mutex mux;
template<typename T>
class CircularQueue{
    size_t st;
    size_t end;
    size_t static_array_size;
    bool isfull;
    T* arr;
public:
    CircularQueue():st{0},end{0},static_array_size{50}, isfull{false}, arr{new T[static_array_size]}{}
    CircularQueue(size_t cap):st{0},end{0},static_array_size{cap},isfull{false}, arr{new T[cap]}{}
    void enqueue ( const T & value ){
        if(isfull) throw runtime_error("Cannot add. Queue is full");
        mux.lock();
        this->arr[end]=value;
        end++;
        end = end % static_array_size;
        if(end==st) isfull = true;
        mux.unlock();
    }
    bool try_enqueue ( const T & value ){
        if(isfull){ 
            return false;
        }
        enqueue(value);
        return true;
    }
    T dequeue (){
        if(end == st && !isfull) throw runtime_error("Cannot deque. Queue is empty");
        mux.lock();
        T temp =  this->arr[st++];
        st = st % static_array_size;
        if(isfull) isfull = false;
        mux.unlock();
        return temp;
    }
    bool try_dequeue ( T & out ){
        if(end > static_array_size || this->empty()) return false;
        out = dequeue();
        return true;
    }
    size_t size () const{
        if(isfull) return static_array_size;
        return ((static_array_size + end) - st) % static_array_size;
    }
    bool empty () const{
        if(end == st && !isfull) return true;
        return false;
    }
    size_t capacity () const{return static_array_size;}
    ~CircularQueue() { delete[] arr;}
};