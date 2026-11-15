/* Compiler runtime glue for ARM7TDMI clang builds. Freestanding, no division ops. */
#include <stdint.h>
extern void *memcpy(void *, const void *, unsigned);
extern void *memmove(void *, const void *, unsigned);
extern void *memset(void *, int, unsigned);
void __aeabi_memcpy(void*d,const void*s,unsigned n){memcpy(d,s,n);}
void __aeabi_memcpy4(void*d,const void*s,unsigned n){memcpy(d,s,n);}
void __aeabi_memcpy8(void*d,const void*s,unsigned n){memcpy(d,s,n);}
void __aeabi_memmove(void*d,const void*s,unsigned n){memmove(d,s,n);}
void __aeabi_memmove4(void*d,const void*s,unsigned n){memmove(d,s,n);}
void __aeabi_memmove8(void*d,const void*s,unsigned n){memmove(d,s,n);}
void __aeabi_memclr(void*d,unsigned n){memset(d,0,n);}
void __aeabi_memclr4(void*d,unsigned n){memset(d,0,n);}
void __aeabi_memclr8(void*d,unsigned n){memset(d,0,n);}
void __aeabi_memset(void*d,unsigned n,int c){memset(d,c,n);}
void __aeabi_memset4(void*d,unsigned n,int c){memset(d,c,n);}
void __aeabi_memset8(void*d,unsigned n,int c){memset(d,c,n);}
int __clzsi2(unsigned n){int z=0;if(!n)return 32;while(!(n&0x80000000u)){z++;n<<=1;}return z;}
static void divu(unsigned n,unsigned d,unsigned*q,unsigned*r){
 if(!d){*q=0;*r=n;return;}
 unsigned factor=1,div=d,quot=0;
 while(div<=n && !(div&0x80000000u)){div<<=1;factor<<=1;}
 if(div>n){div>>=1;factor>>=1;}
 while(factor){if(n>=div){n-=div;quot|=factor;}div>>=1;factor>>=1;}
 *q=quot;*r=n;
}
void __divmod_u_helper(unsigned n,unsigned d,unsigned*q,unsigned*r){divu(n,d,q,r);}
void __divmod_s_helper(int n,int d,int*q,int*r){
 unsigned un=n<0?0u-(unsigned)n:(unsigned)n,ud=d<0?0u-(unsigned)d:(unsigned)d;
 unsigned uq,ur;divu(un,ud,&uq,&ur);
 *q=(n<0)^(d<0)?(int)(0u-uq):(int)uq;
 *r=n<0?(int)(0u-ur):(int)ur;
}
unsigned __aeabi_uidiv(unsigned n,unsigned d){unsigned q,r;divu(n,d,&q,&r);return q;}
int __aeabi_idiv(int n,int d){int q,r;__divmod_s_helper(n,d,&q,&r);return q;}
void *memmove(void*d,const void*s,unsigned n){unsigned char*dst=d;const unsigned char*src=s;if(dst==src)return d;if(dst<src){for(unsigned i=0;i<n;i++)dst[i]=src[i];}else{while(n){n--;dst[n]=src[n];}}return d;}
