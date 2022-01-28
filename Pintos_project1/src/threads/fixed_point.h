//@1-3 This will be included in the interrupt.c
/* Implement our float point calcuation here! */
#ifndef THREAD_FIXED_POINT_H
#define THREAD_FIXED_POINT_H

/* In our Implementation, using 15.16 format: */ 
#define P 15
#define Q 16
/* Thus, F is 1<<Q */
#define F 1 << Q

/*Make it more tidy to reperensent the fixed_point*/
typedef int fx_p;

/* Convert n to fixed point: n*f 
  In the 15.16 format, n should has a different reperesentation */
#define CONVERT2FP(n) (int)(n<<Q)

/* Convert x to integer (rounding toward zero) x/f: */
#define CONVERT2INT_ZERO(x) (int)(x>>Q)

/* Convert x to integer (rounding to nearest):
   (x + f / 2) / f if x >= 0, 
   (x - f / 2) / f if x <= 0. */
#define CONVERT2INT_ROUND(x) (x >= 0 ? (int)((x+(1 << (Q - 1)))>>Q) : (int)((x-(1 << (Q - 1)))>>Q))

/* Add 2 fixed points */
#define FP_ADD(x,y) (x+y)

/* Subtract y from x: */
#define FP_SUB(x,y) (x-y)

/* FP add integer n, so we need to change n to FP */
#define FP_ADD_INT(x,n) (x+(int)(n<<Q))

/* Subtract n from x, so we need to change n to FP */
#define FP_SUB_INT(x,n) (x-(int)(n<<Q))

/* Multiply x by y:((int64_t) x) * y / f */
#define FP_MUL(x,y) (int)(((int64_t) x)*y>>Q)

/* Multiply x by n: x * n */
#define FP_MUL_INT(x,n) (x*n)

/* Divide x by y:((int64_t) x) * f / y */
#define FP_DIV(x,y) (int)((((int64_t) x)<<Q)/y)

/* Divide x by n: x/n */
#define FP_DIV_INT(x,n) (int)(x/n)

#endif