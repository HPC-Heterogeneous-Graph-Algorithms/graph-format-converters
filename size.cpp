#include<bits/stdc++.h>
using namespace std;


int main(){
    printf("Size of int: %d bytes\n", sizeof(int));
    printf("Size of long: %d bytes\n", sizeof(long));
    printf("Size of long long: %d bytes\n", sizeof(long long));
    vector<long> A;
    vector<int> B;
    A.resize(1000000);
    B.resize(10000000000);
    size_t size = A.size();
    printf("Size of A.size(): %d bytes\n", sizeof(size));
    size_t size2 = B.size();
    printf("Size of B.size(): %d bytes\n", sizeof(size2));
    return 0;
}