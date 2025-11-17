#ifndef ENCRYPT_H
#define ENCRYPT_H
#include<iostream>
#include<vector>
#include<algorithm>
#include<string>
#include<string.h>
using namespace std;
class Encrypt{
    vector<char> hidden;
    int size;
    public:
    Encrypt(){
        string r = "qwertyuiopasdfghjkl;1234567890zxcvbnm,./][\\;'{}|:\"<>+_!@#$%^&*()ASDFGHJKLZXCVBNMOPQWERTYUI";
        string h = "asdfghjkl;qwertyuiopzxcvbnm,./1234567890{}|:\"][\\;'+_<>)(*&^%$#@!ZXCVBNMOPASDFGHJKLTYUIQWER";
        size = h.size();
        hidden.resize(size);
        for(int i = 0;i<size;i++){
            int idx = r[i] - 33;
            hidden[idx] = h[i];
        }
    }
    void encrypt(char* buffer, size_t s){
        for(int i =0 ;i<s;i++){
            int idx = buffer[i] - 33;
            buffer[i] = hidden[idx];
        }
    }
    void decrypt(char* buffer, size_t s){
        for(int i =0 ;i<s;i++){
            int idx = buffer[i] - 33;
            buffer[i] = hidden[idx];
        }
    }
};
#endif