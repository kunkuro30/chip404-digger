// Chip404 PoW solver v2: keccak256(salt(32) || wallet(20) || nonce(32 BE)) with >= bits leading zero bits.
// Fast Keccak-f1600 (in-place rho-pi chain). Args: salt wallet bits threads progress_secs offset gstride
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define ROTL(x,y) (((x) << (y)) | ((x) >> (64 - (y))))

static void keccakf(uint64_t A[25]){
  static const uint64_t RC[24] = {
    0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
    0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
    0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
    0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
    0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
    0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL};
  static const uint8_t piln[24] = {10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};
  static const uint8_t rol[24] = {1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
  for(int round=0; round<24; round++){
    // theta
    uint64_t C0=A[0]^A[5]^A[10]^A[15]^A[20];
    uint64_t C1=A[1]^A[6]^A[11]^A[16]^A[21];
    uint64_t C2=A[2]^A[7]^A[12]^A[17]^A[22];
    uint64_t C3=A[3]^A[8]^A[13]^A[18]^A[23];
    uint64_t C4=A[4]^A[9]^A[14]^A[19]^A[24];
    uint64_t D0=C4^ROTL(C1,1), D1=C0^ROTL(C2,1), D2=C1^ROTL(C3,1), D3=C2^ROTL(C4,1), D4=C3^ROTL(C0,1);
    A[0]^=D0; A[5]^=D0; A[10]^=D0; A[15]^=D0; A[20]^=D0;
    A[1]^=D1; A[6]^=D1; A[11]^=D1; A[16]^=D1; A[21]^=D1;
    A[2]^=D2; A[7]^=D2; A[12]^=D2; A[17]^=D2; A[22]^=D2;
    A[3]^=D3; A[8]^=D3; A[13]^=D3; A[18]^=D3; A[23]^=D3;
    A[4]^=D4; A[9]^=D4; A[14]^=D4; A[19]^=D4; A[24]^=D4;
    // rho + pi in-place chain
    uint64_t t=A[1];
    for(int i=0;i<24;i++){ uint64_t tmp=A[piln[i]]; A[piln[i]]=ROTL(t, rol[i]); t=tmp; }
    // chi per row
    for(int y=0;y<25;y+=5){
      uint64_t B0=A[y],B1=A[y+1],B2=A[y+2],B3=A[y+3],B4=A[y+4];
      A[y]  =B0^((~B1)&B2);
      A[y+1]=B1^((~B2)&B3);
      A[y+2]=B2^((~B3)&B4);
      A[y+3]=B3^((~B4)&B0);
      A[y+4]=B4^((~B0)&B1);
    }
    A[0]^=RC[round];
  }
}

static void keccak256_84(const uint8_t msg[84], uint8_t out[32]){
  uint64_t A[25]={0};
  uint8_t block[136]={0};
  memcpy(block, msg, 84);
  block[84]=0x01; block[135]|=0x80;
  for(int i=0;i<17;i++){ uint64_t lane; memcpy(&lane, block+8*i, 8); A[i]=lane; }
  keccakf(A);
  for(int i=0;i<4;i++){ uint64_t lane=A[i]; memcpy(out+8*i, &lane, 8); }
}

static int hexval(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
static int parsehex(const char*s, uint8_t*out, int n){
  if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')) s+=2;
  for(int i=0;i<n;i++){ int hi=hexval(s[2*i]), lo=hexval(s[2*i+1]); if(hi<0||lo<0) return -1; out[i]=(hi<<4)|lo; }
  return 0;
}

static uint8_t g_msg[84];
static int g_bits;
static atomic_int g_found;
static atomic_ullong g_count;
static uint64_t g_offset, g_gstride;
static int g_nthreads;

static int passes(const uint8_t h[32]){
  int full=g_bits/8, rem=g_bits%8;
  for(int i=0;i<full;i++) if(h[i]) return 0;
  if(rem){ uint8_t mask=(uint8_t)(0xFF<<(8-rem)); if(h[full]&mask) return 0; }
  return 1;
}

static void* worker(void* arg){
  long tid=(long)arg;
  uint8_t msg[84]; memcpy(msg, g_msg, 84);
  uint8_t h[32];
  uint64_t nonce = g_offset + (uint64_t)tid;
  uint64_t local=0;
  while(!atomic_load(&g_found)){
    for(int i=0;i<8;i++) msg[76+i]=(uint8_t)(nonce>>(56-8*i));
    keccak256_84(msg, h);
    local++;
    if(passes(h)){
      if(!atomic_exchange(&g_found,1)){
        printf("FOUND nonce=%llu hash=", (unsigned long long)nonce);
        for(int i=0;i<32;i++) printf("%02x", h[i]);
        printf("\n"); fflush(stdout);
      }
      break;
    }
    if((local & 0xFFFFF)==0) atomic_fetch_add(&g_count, 0x100000ULL);
    nonce += g_gstride;
  }
  atomic_fetch_add(&g_count, local & 0xFFFFFULL);
  return NULL;
}

int main(int argc, char** argv){
  if(argc>=2 && strcmp(argv[1],"--test")==0){
    if(argc!=4){ fprintf(stderr,"usage: --test <salt_hex> <wallet_hex>\n"); return 1; }
    uint8_t salt[32], wallet[20], msg[84], out[32];
    if(parsehex(argv[2], salt, 32)||parsehex(argv[3], wallet, 20)){ fprintf(stderr,"bad hex\n"); return 1; }
    memcpy(msg, salt, 32); memcpy(msg+32, wallet, 20); memset(msg+52, 0, 32);
    keccak256_84(msg, out);
    printf("payload nonce=0 hash="); for(int i=0;i<32;i++) printf("%02x", out[i]); printf("\n");
    msg[83]=1; keccak256_84(msg, out);
    printf("payload nonce=1 hash="); for(int i=0;i<32;i++) printf("%02x", out[i]); printf("\n");
    return 0;
  }
  if(argc!=8){ fprintf(stderr,"usage: %s <salt_hex> <wallet_hex> <bits> <threads> <progress_secs> <offset> <gstride>\n", argv[0]); return 1; }
  uint8_t salt[32], wallet[20];
  if(parsehex(argv[1], salt, 32)||parsehex(argv[2], wallet, 20)){ fprintf(stderr,"bad hex\n"); return 1; }
  memcpy(g_msg, salt, 32); memcpy(g_msg+32, wallet, 20); memset(g_msg+52, 0, 32);
  g_bits=atoi(argv[3]);
  g_nthreads=atoi(argv[4]);
  int progress_secs=atoi(argv[5]);
  g_offset=strtoull(argv[6], NULL, 10);
  g_gstride=strtoull(argv[7], NULL, 10);
  atomic_store(&g_found,0); atomic_store(&g_count,0);
  pthread_t th[64];
  for(long i=0;i<g_nthreads;i++) pthread_create(&th[i], NULL, worker, (void*)i);
  uint64_t last=0;
  while(!atomic_load(&g_found)){
    int slept=0;
    while(slept<progress_secs && !atomic_load(&g_found)){
      struct timespec ts={1,0}; nanosleep(&ts,NULL); slept++;
    }
    uint64_t c=atomic_load(&g_count);
    fprintf(stderr,"progress: %llu hashes, ~%.0f h/s\n",(unsigned long long)c, (double)(c-last)/(progress_secs>0?progress_secs:1));
    last=c;
  }
  for(int i=0;i<g_nthreads;i++) pthread_join(th[i], NULL);
  return 0;
}
