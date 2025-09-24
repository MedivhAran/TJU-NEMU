static inline int F2int(FLOAT a) {
        return (a >> 16);
}
 
static inline FLOAT int2F(int a) {
        return (a << 16);
}
 
static inline FLOAT F_mul_int(FLOAT a, int b) {
        return a * b;
}
 
static inline FLOAT F_div_int(FLOAT a, int b) {
        return a / b;
}
