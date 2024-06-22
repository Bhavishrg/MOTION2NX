#include "fss_uint16.h"

// ---------------------------- HELPER FUNCTIONS ---------------------------- //
void xor_c_16(const uint8_t *a, const uint8_t *b, uint8_t *res, size_t s_len){
    size_t i;
    for (i = 0; i < s_len; i++)
    {
        res[i] = a[i] ^ b[i];
    }
}

void bit_decomposition_16(R_t_16 value, bool *bits_array){
    size_t i;
    for (i = 0; i < N_BITS_16; i++)
    {
        bits_array[i] = value & (1ULL<<(N_BITS_16-i-1));
    }
}

void xor_cond_16(const uint8_t *a, const uint8_t *b, uint8_t *res, size_t len, bool cond){
    if (cond)
    {
        xor_c_16(a, b, res, len);
    }   
    else
    {
        memcpy(res, a, len);
    }
}

// -------------------------------------------------------------------------- //
// --------------------------- RANDOMNESS SAMPLING -------------------------- //
// -------------------------------------------------------------------------- //

void random_buffer_seeded_16(uint8_t buffer[], size_t buffer_len, const uint8_t seed[SEED_LEN]){
    if (buffer==NULL)
    {
        printf("<Funshade Error>: buffer must have allocated memory to initialize\n");
        exit(EXIT_FAILURE);
    }              // Use insecure random number generation
        if (seed == NULL)
        {
            srand(100);        // Initialize random seed // made it deterministic
        }
        else
        {
            srand(100);        // Initialize seeded //made it deterministic
        }
        size_t i;
        for (i = 0; i < buffer_len; i++){
            buffer[i] = rand() % 256;
        }
}

void random_buffer_16(uint8_t buffer[], size_t buffer_len){
    random_buffer_seeded_16(buffer, buffer_len, NULL);
}

R_t_16 random_dtype_seeded_16(const uint8_t seed[SEED_LEN]){
    R_t_16 value = 0;
        if (seed != NULL)   // If seed is not provided, use secure randomness
        {
            srand(*((unsigned int*)seed));        // Initialize seeded
        }
        else                // Use provided seed
        {
            srand((unsigned int)time(NULL));        // Initialize random seed
        }
        size_t i;
        for (i = 0; i < sizeof(R_t_16); i++){
            value = (R_t_16)rand();
        }
    return value;
}

R_t_16 random_dtype_16(){
    return random_dtype_seeded_16(NULL);
}

// -------------------------------------------------------------------------- //
// ----------------- DISTRIBUTED POINT FUNCTION (DPF) ------------------ //
// -------------------------------------------------------------------------- //
void DPF_gen_seeded_16(R_t_16 alpha, uint8_t k0[KEY_LEN_16], uint8_t k1[KEY_LEN_16], uint8_t s0[S_LEN_16], uint8_t s1[S_LEN_16]){
    // Inputs and outputs to G
    uint8_t s0_i[S_LEN_16],  g_out_0[G_OUT_LEN_16],
            s1_i[S_LEN_16],  g_out_1[G_OUT_LEN_16];
    // Pointers to the various parts of the output of G
    uint8_t *s0_keep, *s0_lose, *v0_keep, *v0_lose, *t0_keep,
            *s1_keep, *s1_lose, *v1_keep, *v1_lose, *t1_keep;
    // Temporary variables
    uint8_t s_cw[S_LEN_16] = {0};
    R_t_16 V_cw, V_alpha=0;    bool t0=0, t1=1;                                    // L3
    bool t_cw_L, t_cw_R, t0_L, t0_R, t1_L, t1_R;
    size_t i;

    // Decompose alpha into an array of bits                                    // L1
    bool alpha_bits[N_BITS_16] = {0};
    bit_decomposition_16(alpha, alpha_bits);

    // Initialize s0 and s1 randomly if they are NULL                           // L2
    if (s0==NULL || s1==NULL)
    {
        random_buffer_16(s0_i, S_LEN_16);
        random_buffer_16(s1_i, S_LEN_16);
    }
    else
    {
        memcpy(s0_i, s0, S_LEN_16);
        memcpy(s1_i, s1, S_LEN_16);
    }
    memcpy(&k0[S_PTR_16], s0_i, S_LEN_16);
    memcpy(&k1[S_PTR_16], s1_i, S_LEN_16);
    
    // Main loop
    for (i = 0; i < N_BITS_16; i++)                                         // L4
    {
        #ifdef __AES__
            G_ni(s0_i, g_out_0, G_IN_LEN_16, G_OUT_LEN_16);                           // L5
            G_ni(s1_i, g_out_1, G_IN_LEN_16, G_OUT_LEN_16);                           // L6
        #else
            G_tiny(s0_i, g_out_0, G_IN_LEN_16, G_OUT_LEN_16);                         // L5
            G_tiny(s1_i, g_out_1, G_IN_LEN_16, G_OUT_LEN_16);                         // L6
        #endif
        t0_L = TO_BOOL_16(g_out_0 + T_L_PTR_16);   t0_R = TO_BOOL_16(g_out_0 + T_R_PTR_16);
        t1_L = TO_BOOL_16(g_out_1 + T_L_PTR_16);   t1_R = TO_BOOL_16(g_out_1 + T_R_PTR_16);
        if (alpha_bits[i])  // keep = R; lose = L;                              // L8
        {
            s0_keep = g_out_0 + S_R_PTR_16;    s0_lose = g_out_0 + S_L_PTR_16;
            v0_keep = g_out_0 + V_R_PTR_16;    v0_lose = g_out_0 + V_L_PTR_16;
            t0_keep = g_out_0 + T_R_PTR_16;  //t0_lose = g_out_0 + T_L_PTR_16;
            s1_keep = g_out_1 + S_R_PTR_16;    s1_lose = g_out_1 + S_L_PTR_16;
            v1_keep = g_out_1 + V_R_PTR_16;    v1_lose = g_out_1 + V_L_PTR_16;
            t1_keep = g_out_1 + T_R_PTR_16;  //t1_lose = g_out_1 + T_L_PTR_16;
        }
        else                // keep = L; lose = R;                              // L7
        {
            s0_keep = g_out_0 + S_L_PTR_16;    s0_lose = g_out_0 + S_R_PTR_16;
            v0_keep = g_out_0 + V_L_PTR_16;    v0_lose = g_out_0 + V_R_PTR_16;
            t0_keep = g_out_0 + T_L_PTR_16;  //t0_lose = g_out_0 + T_R_PTR_16;
            s1_keep = g_out_1 + S_L_PTR_16;    s1_lose = g_out_1 + S_R_PTR_16;
            v1_keep = g_out_1 + V_L_PTR_16;    v1_lose = g_out_1 + V_R_PTR_16;
            t1_keep = g_out_1 + T_L_PTR_16;  //t1_lose = g_out_1 + T_R_PTR_16;
        }
        xor_c_16(s0_lose, s1_lose, s_cw, S_LEN_16);                                     // L10
        V_cw = (t1?-1:1) * (TO_R_t_16(v1_lose) - TO_R_t_16(v0_lose) - V_alpha);       // L11
        V_cw += alpha_bits[i] * (t1?-1:1) * BETA; // Lose=L --> alpha_bits[i]=1 // L12

        V_alpha += TO_R_t_16(v0_keep) - TO_R_t_16(v1_keep) + (t1?-1:1)*V_cw;          // L14
        t_cw_L = t0_L ^ t1_L ^ alpha_bits[i] ^ 1;                               // L15
        t_cw_R = t0_R ^ t1_R ^ alpha_bits[i];

        memcpy(&k0[CW_CHAIN_PTR_16 + S_CW_PTR_16(i)], s_cw, S_LEN_16);                   // L16
        memcpy(&k0[CW_CHAIN_PTR_16 + V_CW_PTR_16(i)], &V_cw, V_LEN_16);
        memcpy(&k0[CW_CHAIN_PTR_16 + T_CW_L_PTR_16(i)], &t_cw_L, sizeof(bool));
        memcpy(&k0[CW_CHAIN_PTR_16 + T_CW_R_PTR_16(i)], &t_cw_R, sizeof(bool));
        
        xor_cond_16(s0_keep, s_cw, s0_i, S_LEN_16, t0);                               // L18
        t0 = TO_BOOL_16(t0_keep) ^ (t0 & (alpha_bits[i]?t_cw_R:t_cw_L));           // L19
        xor_cond_16(s1_keep, s_cw, s1_i, S_LEN_16, t1);                                    
        t1 = TO_BOOL_16(t1_keep) ^ (t1 & (alpha_bits[i]?t_cw_R:t_cw_L));              
    }
    V_alpha = (t1?-1:1) * (TO_R_t_16(s1_i) - TO_R_t_16(s0_i) - V_alpha);              // L20
    memcpy(&k0[CW_CHAIN_PTR_16]+LAST_CW_PTR_16, &V_alpha,  sizeof(R_t_16));
    // Copy the resulting CW_chain                                              // L21
    memcpy(&k1[CW_CHAIN_PTR_16], &k0[CW_CHAIN_PTR_16], CW_CHAIN_LEN_16);
}

void DPF_gen_16(R_t_16 alpha, uint8_t k0[KEY_LEN_16], uint8_t k1[KEY_LEN_16]){
    DCF_gen_seeded_16(alpha, k0, k1, NULL, NULL);
}

R_t_16 DPF_eval_16(bool b, const uint8_t kb[KEY_LEN_16], R_t_16 x_hat){
    R_t_16 V = 0;     bool t = b, x_bits[N_BITS_16];                                  // L1
    uint8_t s[S_LEN_16], g_out[G_OUT_LEN_16];     
    size_t i;
    // Copy the initial state to avoid modifying the original key
    memcpy(s, &kb[S_PTR_16], S_LEN_16);
    // Decompose x into an array of bits
    bit_decomposition_16(x_hat, x_bits);

    // Main loop
    for (i = 0; i < N_BITS_16; i++)                                         // L2
    {
        #ifdef __AES__
            G_ni(s, g_out, G_IN_LEN_16, G_OUT_LEN_16);                                // L4
        #else
            G_tiny(s, g_out, G_IN_LEN_16, G_OUT_LEN_16);
        #endif
        if (x_bits[i]==0)  // Pick the Left branch
        {
           V += (b?-1:1) * (  TO_R_t_16(&g_out[V_L_PTR_16]) +                         // L7
                            t*TO_R_t_16(&kb[CW_CHAIN_PTR_16+V_CW_PTR_16(i)]));
           xor_cond_16(g_out+S_L_PTR_16, &kb[CW_CHAIN_PTR_16+S_CW_PTR_16(i)], s, S_LEN_16, t); // L8
           t = TO_BOOL_16(g_out+T_L_PTR_16) ^ (t&TO_BOOL_16(&kb[CW_CHAIN_PTR_16+T_CW_L_PTR_16(i)]));
        }
        else               // Pick the Right branch
        {
           V += (b?-1:1) * (  TO_R_t_16(&g_out[V_R_PTR_16]) +                         // L9
                            t*TO_R_t_16(&kb[CW_CHAIN_PTR_16+V_CW_PTR_16(i)]));
           xor_cond_16(g_out + S_R_PTR_16, &kb[CW_CHAIN_PTR_16+S_CW_PTR_16(i)], s, S_LEN_16, t);// L10  
           t = TO_BOOL_16(g_out+T_R_PTR_16) ^ (t&TO_BOOL_16(&kb[CW_CHAIN_PTR_16+T_CW_R_PTR_16(i)]));                    
        }
    }
    V += (b?-1:1) * (TO_R_t_16(s) + t*TO_R_t_16(&kb[CW_CHAIN_PTR_16+LAST_CW_PTR_16]));      // L13
    return V;
}


// -------------------------------------------------------------------------- //
// ----------------- DISTRIBUTED COMPARISON FUNCTION (DCF) ------------------ //
// -------------------------------------------------------------------------- //
void DCF_gen_seeded_16(R_t_16 alpha, uint8_t k0[KEY_LEN_16], uint8_t k1[KEY_LEN_16], uint8_t s0[S_LEN_16], uint8_t s1[S_LEN_16]){
    // Inputs and outputs to G
    uint8_t s0_i[S_LEN_16],  g_out_0[G_OUT_LEN_16],
            s1_i[S_LEN_16],  g_out_1[G_OUT_LEN_16];
    // Pointers to the various parts of the output of G
    uint8_t *s0_keep, *s0_lose, *v0_keep, *v0_lose, *t0_keep,
            *s1_keep, *s1_lose, *v1_keep, *v1_lose, *t1_keep;
    // Temporary variables
    uint8_t s_cw[S_LEN_16] = {0};
    R_t_16 V_cw, V_alpha=0;    bool t0=0, t1=1;                                    // L3
    bool t_cw_L, t_cw_R, t0_L, t0_R, t1_L, t1_R;
    size_t i;

    // Decompose alpha into an array of bits                                    // L1
    bool alpha_bits[N_BITS_16] = {0};
    bit_decomposition_16(alpha, alpha_bits);

    // Initialize s0 and s1 randomly if they are NULL                           // L2
    if (s0==NULL || s1==NULL)
    {
        random_buffer_16(s0_i, S_LEN_16);
        random_buffer_16(s1_i, S_LEN_16);
    }
    else
    {
        memcpy(s0_i, s0, S_LEN_16);
        memcpy(s1_i, s1, S_LEN_16);
    }
    memcpy(&k0[S_PTR_16], s0_i, S_LEN_16);
    memcpy(&k1[S_PTR_16], s1_i, S_LEN_16);
    
    // Main loop
    for (i = 0; i < N_BITS_16; i++)                                         // L4
    {
        #ifdef __AES__
            G_ni(s0_i, g_out_0, G_IN_LEN_16, G_OUT_LEN_16);                           // L5
            G_ni(s1_i, g_out_1, G_IN_LEN_16, G_OUT_LEN_16);                           // L6
        #else
            G_tiny(s0_i, g_out_0, G_IN_LEN_16, G_OUT_LEN_16);                         // L5
            G_tiny(s1_i, g_out_1, G_IN_LEN_16, G_OUT_LEN_16);                         // L6
        #endif
        t0_L = TO_BOOL_16(g_out_0 + T_L_PTR_16);   t0_R = TO_BOOL_16(g_out_0 + T_R_PTR_16);
        t1_L = TO_BOOL_16(g_out_1 + T_L_PTR_16);   t1_R = TO_BOOL_16(g_out_1 + T_R_PTR_16);
        if (alpha_bits[i])  // keep = R; lose = L;                              // L8
        {
            s0_keep = g_out_0 + S_R_PTR_16;    s0_lose = g_out_0 + S_L_PTR_16;
            v0_keep = g_out_0 + V_R_PTR_16;    v0_lose = g_out_0 + V_L_PTR_16;
            t0_keep = g_out_0 + T_R_PTR_16;  //t0_lose = g_out_0 + T_L_PTR_16;
            s1_keep = g_out_1 + S_R_PTR_16;    s1_lose = g_out_1 + S_L_PTR_16;
            v1_keep = g_out_1 + V_R_PTR_16;    v1_lose = g_out_1 + V_L_PTR_16;
            t1_keep = g_out_1 + T_R_PTR_16;  //t1_lose = g_out_1 + T_L_PTR_16;
        }
        else                // keep = L; lose = R;                              // L7
        {
            s0_keep = g_out_0 + S_L_PTR_16;    s0_lose = g_out_0 + S_R_PTR_16;
            v0_keep = g_out_0 + V_L_PTR_16;    v0_lose = g_out_0 + V_R_PTR_16;
            t0_keep = g_out_0 + T_L_PTR_16;  //t0_lose = g_out_0 + T_R_PTR_16;
            s1_keep = g_out_1 + S_L_PTR_16;    s1_lose = g_out_1 + S_R_PTR_16;
            v1_keep = g_out_1 + V_L_PTR_16;    v1_lose = g_out_1 + V_R_PTR_16;
            t1_keep = g_out_1 + T_L_PTR_16;  //t1_lose = g_out_1 + T_R_PTR_16;
        }
        xor_c_16(s0_lose, s1_lose, s_cw, S_LEN_16);                                     // L10
        V_cw = (t1?-1:1) * (TO_R_t_16(v1_lose) - TO_R_t_16(v0_lose) - V_alpha);       // L11
        V_cw += alpha_bits[i] * (t1?-1:1) * BETA; // Lose=L --> alpha_bits[i]=1 // L12

        V_alpha += TO_R_t_16(v0_keep) - TO_R_t_16(v1_keep) + (t1?-1:1)*V_cw;          // L14
        t_cw_L = t0_L ^ t1_L ^ alpha_bits[i] ^ 1;                               // L15
        t_cw_R = t0_R ^ t1_R ^ alpha_bits[i];

        memcpy(&k0[CW_CHAIN_PTR_16 + S_CW_PTR_16(i)], s_cw, S_LEN_16);                   // L16
        memcpy(&k0[CW_CHAIN_PTR_16 + V_CW_PTR_16(i)], &V_cw, V_LEN_16);
        memcpy(&k0[CW_CHAIN_PTR_16 + T_CW_L_PTR_16(i)], &t_cw_L, sizeof(bool));
        memcpy(&k0[CW_CHAIN_PTR_16 + T_CW_R_PTR_16(i)], &t_cw_R, sizeof(bool));
        
        xor_cond_16(s0_keep, s_cw, s0_i, S_LEN_16, t0);                               // L18
        t0 = TO_BOOL_16(t0_keep) ^ (t0 & (alpha_bits[i]?t_cw_R:t_cw_L));           // L19
        xor_cond_16(s1_keep, s_cw, s1_i, S_LEN_16, t1);                                    
        t1 = TO_BOOL_16(t1_keep) ^ (t1 & (alpha_bits[i]?t_cw_R:t_cw_L));              
    }
    V_alpha = (t1?-1:1) * (TO_R_t_16(s1_i) - TO_R_t_16(s0_i) - V_alpha);              // L20
    memcpy(&k0[CW_CHAIN_PTR_16]+LAST_CW_PTR_16, &V_alpha,  sizeof(R_t_16));
    // Copy the resulting CW_chain                                              // L21
    memcpy(&k1[CW_CHAIN_PTR_16], &k0[CW_CHAIN_PTR_16], CW_CHAIN_LEN_16);
}

void DCF_gen_16(R_t_16 alpha, uint8_t k0[KEY_LEN_16], uint8_t k1[KEY_LEN_16]){
    DCF_gen_seeded_16(alpha, k0, k1, 0, 0);
}

R_t_16 DCF_eval_16(bool b, const uint8_t kb[KEY_LEN_16], R_t_16 x_hat){
    R_t_16 V = 0;     bool t = b, x_bits[N_BITS_16];                                  // L1
    uint8_t s[S_LEN_16], g_out[G_OUT_LEN_16];     
    size_t i;
    // Copy the initial state to avoid modifying the original key
    memcpy(s, &kb[S_PTR_16], S_LEN_16);
    // Decompose x into an array of bits
    bit_decomposition_16(x_hat, x_bits);

    // Main loop
    for (i = 0; i < N_BITS_16; i++)                                         // L2
    {
        #ifdef __AES__
            G_ni(s, g_out, G_IN_LEN_16, G_OUT_LEN_16);                                // L4
        #else
            G_tiny(s, g_out, G_IN_LEN_16, G_OUT_LEN_16);
        #endif
        if (x_bits[i]==0)  // Pick the Left branch
        {
           V += (b?-1:1) * (  TO_R_t_16(&g_out[V_L_PTR_16]) +                         // L7
                            t*TO_R_t_16(&kb[CW_CHAIN_PTR_16+V_CW_PTR_16(i)]));
           xor_cond_16(g_out+S_L_PTR_16, &kb[CW_CHAIN_PTR_16+S_CW_PTR_16(i)], s, S_LEN_16, t); // L8
           t = TO_BOOL_16(g_out+T_L_PTR_16) ^ (t&TO_BOOL_16(&kb[CW_CHAIN_PTR_16+T_CW_L_PTR_16(i)]));
        }
        else               // Pick the Right branch
        {
           V += (b?-1:1) * (  TO_R_t_16(&g_out[V_R_PTR_16]) +                         // L9
                            t*TO_R_t_16(&kb[CW_CHAIN_PTR_16+V_CW_PTR_16(i)]));
           xor_cond_16(g_out + S_R_PTR_16, &kb[CW_CHAIN_PTR_16+S_CW_PTR_16(i)], s, S_LEN_16, t);// L10  
           t = TO_BOOL_16(g_out+T_R_PTR_16) ^ (t&TO_BOOL_16(&kb[CW_CHAIN_PTR_16+T_CW_R_PTR_16(i)]));                    
        }
    }
    V += (b?-1:1) * (TO_R_t_16(s) + t*TO_R_t_16(&kb[CW_CHAIN_PTR_16+LAST_CW_PTR_16]));      // L13
    return V;
}

// -------------------------------------------------------------------------- //
// ------------------------- INTERVAL CONTAINMENT --------------------------- //
// -------------------------------------------------------------------------- //
void IC_gen_16(R_t_16 r_in, R_t_16 r_out, R_t_16 p, R_t_16 q, uint8_t k0_ic[KEY_LEN_16], uint8_t k1_ic[KEY_LEN_16]){
    DCF_gen_16((r_in-1), k0_ic, k1_ic);
    TO_R_t_16(&k0_ic[Z_PTR_16]) = random_dtype_16();
    TO_R_t_16(&k1_ic[Z_PTR_16]) = - TO_R_t_16(&k0_ic[Z_PTR_16]) + r_out 
                                + (U(p+r_in)  > U(q+r_in))  // alpha_p > alpha_q
                                - (U(p+r_in)  > U(p))       // alpha_p > p
                                + (U(q+r_in+1)> U(q+1))     // alpha_q_prime > q_prime
                                + (U(q+r_in+1)==U(0));      // alpha_q_prime = -1
}

R_t_16 IC_eval_16(bool b, R_t_16 p, R_t_16 q, const uint8_t kb_ic[KEY_LEN_16], R_t_16 x_hat){
    R_t_16 output_1 = DCF_eval_16(b, kb_ic, (x_hat-p-1));
    R_t_16 output_2 = DCF_eval_16(b, kb_ic, (x_hat-q-2));
    R_t_16 output = b*((U(x_hat)>U(p))-(U(x_hat)>U(q+1))) - output_1 + output_2 + TO_R_t_16(&kb_ic[Z_PTR_16]);
    return output;
}

