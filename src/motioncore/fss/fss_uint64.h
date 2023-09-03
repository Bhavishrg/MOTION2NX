#ifndef __FSS_H__64
#define __FSS_H__64

#include <stdint.h>     // uint8_t, uint32_t, uint64_t
#include <stdbool.h>    // bool, true, false
#include <stdio.h>      // printf()
#include <time.h>       // time()
#include <stdlib.h>     // rand(), srand()

//----------------------------------------------------------------------------//
// DEPENDENCIES

#define SEED_LEN        32

#if defined(_OPENMP)
    #include <omp.h>        // OpenMP header
#endif

#include "aes.h" // AES-128-NI and AES-128-standalone

//----------------------------------------------------------------------------//
//------------------------  CONFIGURABLE PARAMETERS --------------------------//
//----------------------------------------------------------------------------//
#define SEC_PARAM       128                 // Security parameter in bits
#ifndef R_t_64
#define R_t_64             int8_t             // Ring data type for all the constructions
#endif
#define BETA            1                   // Value of the output of the FSS gate

//----------------------------------------------------------------------------//
//-------------------------------- PRIVATE -----------------------------------//
//----------------------------------------------------------------------------//
// UTILS
#define CEIL(x,y)       (((x) - 1) / (y) + 1)               // x/y rounded up to the nearest integer
#define assertm(exp, msg) assert(((void)msg, exp))          // Assert with message
#define U(x)            ((unsigned)(x))                     // Unsigned cast

// FIXED DEFINITIONS
#define N_BITS_64          sizeof(R_t_64)*8                       // Number of bits in R_t_64

#define G_IN_LEN_64        CEIL(SEC_PARAM,8)                   // [SEC_PARAM/8] input bytes
#define OUT_LEN_64         CEIL(2*SEC_PARAM+2*N_BITS_64+2,8)      // output bytes
#define G_OUT_LEN_64       (CEIL(OUT_LEN_64,G_IN_LEN_64)*G_IN_LEN_64)   // output bytes of G

#define TO_R_t_64(ptr)     (*((R_t_64*)(ptr)))                    // Cast pointer to R_t_64
#define TO_BOOL_64(ptr)    ((bool)((*(uint8_t*)(ptr))&0x01))   // Cast pointer to bool

// Sizes of the various elements in the FSS key
#define S_LEN_64           G_IN_LEN_64                            // Size of the states s
#define V_LEN_64           sizeof(R_t_64)                         // Size of the masking values v
#define CW_LEN_64          (S_LEN_64 + sizeof(R_t_64) + 2)           // Size of the correction words
#define CW_CHAIN_LEN_64    ((CW_LEN_64*N_BITS_64)+V_LEN_64)             // Size of the correction word chain
#define KEY_LEN_64         (S_LEN_64 + CW_CHAIN_LEN_64 + V_LEN_64)      // Size of the FSS key

// Positions of the elements in the correction word chain, for each correction word j
#define S_CW_PTR_64(j)     (j*CW_LEN_64)                          // Position of state s_cw
#define V_CW_PTR_64(j)     (S_CW_PTR_64(j) + S_LEN_64)               // Position of value v_cw
#define T_CW_L_PTR_64(j)   (V_CW_PTR_64(j) + V_LEN_64)               // Position of bit t_cw_l
#define T_CW_R_PTR_64(j)   (T_CW_L_PTR_64(j) + 1)                 // Position of bit t_cw_r
#define LAST_CW_PTR_64     (CW_LEN_64*N_BITS_64)                     // Position of last correction word, v_cw_n+1

// Positions of left and right elements in the output of G 
#define S_L_PTR_64         0                                   // Position of state s_l in G output
#define S_R_PTR_64         (S_L_PTR_64 + S_LEN_64)                   // Position of state s_r in G output
#define V_L_PTR_64         (S_R_PTR_64 + S_LEN_64)                   // Position of value v_l in G output
#define V_R_PTR_64         (V_L_PTR_64 + V_LEN_64)                   // Position of value v_r in G output
#define T_L_PTR_64         (V_R_PTR_64 + V_LEN_64)                   // Position of bit t_l in G output
#define T_R_PTR_64         (T_L_PTR_64 + 1)                       // Position of bit t_r in G output

// Positions of the elements in the FSS key
#define S_PTR_64           0                                   // Position of state s
#define CW_CHAIN_PTR_64    (S_PTR_64 + S_LEN_64)                     // Position of correction word chain
#define Z_PTR_64           (CW_CHAIN_PTR_64 + CW_CHAIN_LEN_64)       // Position of value z

//----------------------------------------------------------------------------//
//--------------------------------  PRIVATE  ---------------------------------//
//----------------------------------------------------------------------------//
void xor_c_64(const uint8_t *a, const uint8_t *b, uint8_t *res, size_t s_len);
void bit_decomposition_64(R_t_64 value, bool *bits_array);
void xor_cond_64(const uint8_t *a, const uint8_t *b, uint8_t *res, size_t len, bool cond);
//----------------------------------------------------------------------------//
//--------------------------------- PUBLIC -----------------------------------//
//----------------------------------------------------------------------------//

//.............................. RANDOMNESS GEN ..............................//
// Manages randomness. Uses libsodium (cryptographically secure) if USE_LIBSODIUM
//  is defined, otherwise uses rand() (not cryptographically secure but portable).
R_t_64 random_dtype_64();                                    // Non-deterministic seed
R_t_64 random_dtype_seeded_64(const uint8_t seed[SEED_LEN]);
void random_buffer_64(uint8_t buffer[], size_t buffer_len);   // Non-deterministic seed 
void random_buffer_seeded_64(uint8_t buffer[], size_t buffer_len, const uint8_t seed[SEED_LEN]);

//................................ DPF GATE ..................................//
// FSS gate for the Distributed Point Function (DPF) gate.
//  Yields o0 + o1 = BETA*((unsigned)x=(unsigned)alpha)

/// @brief Generate a FSS key pair for the DPF gate
/// @param alpha input mask (should be uniformly random in R_t_64)
/// @param k0   pointer to the key of party 0
/// @param k1   pointer to the key of party 1
/// @param s0   Initial seed/state of party 0 (if NULL/unspecified, will be generated)
/// @param s1   Initial seed/state of party 1 (if NULL/unspecified, will be generated)
void DPF_gen_64(R_t_64 alpha, uint8_t k0[KEY_LEN_64], uint8_t k1[KEY_LEN_64]);
void DPF_gen_seeded_64(R_t_64 alpha, uint8_t k0[KEY_LEN_64], uint8_t k1[KEY_LEN_64], uint8_t s0[S_LEN_64], uint8_t s1[S_LEN_64]);

/// @brief Evaluate the DCF gate for a given input x in a 2PC setting
/// @param b        party number (0 or 1)
/// @param kb       pointer to the key of the party
/// @param x_hat    public input to the FSS gate
/// @return         result of the FSS gate o, such that o0 + o1 = BETA*((unsigned)x>(unsigned)alpha)
R_t_64 DPF_eval_64(bool b, const uint8_t kb[KEY_LEN_64], R_t_64 x_hat);

//................................ DCF GATE ..................................//
// FSS gate for the Distributed Conditional Function (DCF) gate.
//  Yields o0 + o1 = BETA*((unsigned)x>(unsigned)alpha)

/// @brief Generate a FSS key pair for the DCF gate
/// @param alpha input mask (should be uniformly random in R_t_64)
/// @param k0   pointer to the key of party 0
/// @param k1   pointer to the key of party 1
/// @param s0   Initial seed/state of party 0 (if NULL/unspecified, will be generated)
/// @param s1   Initial seed/state of party 1 (if NULL/unspecified, will be generated)
void DCF_gen_64(R_t_64 alpha, uint8_t k0[KEY_LEN_64], uint8_t k1[KEY_LEN_64]);
void DCF_gen_seeded_64(R_t_64 alpha, uint8_t k0[KEY_LEN_64], uint8_t k1[KEY_LEN_64], uint8_t s0[S_LEN_64], uint8_t s1[S_LEN_64]);

/// @brief Evaluate the DCF gate for a given input x in a 2PC setting
/// @param b        party number (0 or 1)
/// @param kb       pointer to the key of the party
/// @param x_hat    public input to the FSS gate
/// @return         result of the FSS gate o, such that o0 + o1 = BETA*((unsigned)x>(unsigned)alpha)
R_t_64 DCF_eval_64(bool b, const uint8_t kb[KEY_LEN_64], R_t_64 x_hat);


//................................ IC GATE ...................................//

/// @brief Generate a FSS key pair for the Interval Containment (IC) gate.
/// @param r_in     input mask (should be uniformly random in R_t_64)
/// @param r_out    output mask (should be uniformly random in R_t_64)
/// @param p        lower bound of the interval
/// @param q        upper bound of the interval
/// @param k0_ic    pointer to the key of party 0
/// @param k1_ic    pointer to the key of party 1
void IC_gen_64(R_t_64 r_in, R_t_64 r_out, R_t_64 p, R_t_64 q, uint8_t k0_ic[KEY_LEN_64], uint8_t k1_ic[KEY_LEN_64]);

/// @brief Evaluate the IC gate for a given input x in a 2PC setting
/// @param b        party number (0 or 1)
/// @param p        lower bound of the interval
/// @param q        upper bound of the interval
/// @param kb_ic    pointer to the function key of the party
/// @param x_hat    public input to the FSS gate
/// @return         result of the FSS gate oj, such that o0 + o1 = BETA*(p<=x<=q)
R_t_64 IC_eval_64(bool b, R_t_64 p, R_t_64 q, const uint8_t kb_ic[KEY_LEN_64], R_t_64 x_hat);


#endif // __FSS_H__