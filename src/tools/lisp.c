#include "syscalls.h"

// Funções utilitárias
static int kstrcmp(const char* s1, const char* s2) {
    while(*s1 && *s1 == *s2) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void kstrcpy(char* dest, const char* src) {
    while(*src) { *dest++ = *src++; }
    *dest = '\0';
}

static void print_int(int n) {
    if (n < 0) { vga_putchar('-'); n = -n; }
    if (n == 0) { vga_putchar('0'); return; }
    char buf[16];
    int i = 0;
    while(n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while(i > 0) vga_putchar(buf[--i]);
}

// AST Nodes
typedef enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_LIST, LVAL_NIL, LVAL_FUNC } LType;

typedef struct LispNode {
    LType type;
    int num;
    char sym[32];
    struct LispNode* first;
    struct LispNode* rest;
} LispNode;

#define MEMORY_POOL_SIZE (1024 * 1024)
static char memory_pool[MEMORY_POOL_SIZE];
static int memory_pool_index = 0;

static void* lisp_malloc(int size) {
    if (memory_pool_index + size > MEMORY_POOL_SIZE) return NULL;
    void* ptr = &memory_pool[memory_pool_index];
    memory_pool_index += size;
    return ptr;
}

static LispNode* alloc_node(LType type) {
    LispNode* n = (LispNode*)lisp_malloc(sizeof(LispNode));
    if (!n) {
        vga_print("Erro: AST estourou o Memory Pool do LISP (1 MB).\n");
        sys_exit();
    }
    n->type = type;
    return n;
}

static LispNode* new_lval_num(int num) {
    LispNode* n = alloc_node(LVAL_NUM);
    n->num = num;
    return n;
}

static LispNode* new_lval_err(const char* m) {
    LispNode* n = alloc_node(LVAL_ERR);
    kstrcpy(n->sym, m);
    return n;
}

static LispNode* new_lval_sym(const char* s) {
    LispNode* n = alloc_node(LVAL_SYM);
    kstrcpy(n->sym, s);
    return n;
}

static LispNode* new_lval_list(LispNode* first, LispNode* rest) {
    LispNode* n = alloc_node(LVAL_LIST);
    n->first = first;
    n->rest = rest;
    return n;
}

static LispNode* new_lval_nil(void) {
    return alloc_node(LVAL_NIL);
}

static LispNode* new_lval_func(LispNode* params, LispNode* body) {
    LispNode* n = alloc_node(LVAL_FUNC);
    n->first = params;
    n->rest = body;
    return n;
}

// Parser
static const char* lisp_src;

static char peek(void) {
    while(*lisp_src == ' ' || *lisp_src == '\n' || *lisp_src == '\t' || *lisp_src == '\r') lisp_src++;
    return *lisp_src;
}

static LispNode* read_list(void);

static LispNode* read_atom(void) {
    char buf[32];
    int i = 0;
    while (*lisp_src && *lisp_src != ' ' && *lisp_src != '\n' && *lisp_src != '\t' && *lisp_src != '\r' && *lisp_src != '(' && *lisp_src != ')') {
        if (i < 31) buf[i++] = *lisp_src;
        lisp_src++;
    }
    buf[i] = '\0';
    
    if (i == 0) return NULL;

    int is_num = 1;
    for(int j=0; j<i; j++) {
        if (j==0 && buf[j]=='-' && i>1) continue;
        if (buf[j] < '0' || buf[j] > '9') is_num = 0;
    }
    if (is_num) {
        int n = 0;
        int sign = 1;
        char* p = buf;
        if (*p == '-') { sign = -1; p++; }
        while(*p) { n = n*10 + (*p - '0'); p++; }
        return new_lval_num(n * sign);
    } else {
        return new_lval_sym(buf);
    }
}

static LispNode* read_expr(void) {
    char c = peek();
    if (c == '\0') return NULL;
    if (c == '(') {
        lisp_src++;
        return read_list();
    }
    if (c == ')') return new_lval_err("Unexpected ')'");
    return read_atom();
}

static LispNode* read_list(void) {
    char c = peek();
    if (c == ')') {
        lisp_src++;
        return new_lval_nil();
    }
    LispNode* first = read_expr();
    if (!first || first->type == LVAL_ERR) return first ? first : new_lval_err("EOF");
    LispNode* rest = read_list();
    return new_lval_list(first, rest);
}

// Environment
typedef struct {
    char sym[32];
    LispNode* val;
} EnvVar;

static EnvVar global_env[128];
static int global_env_count = 0;

static void env_set(const char* sym, LispNode* val) {
    for(int i=0; i<global_env_count; i++) {
        if (kstrcmp(global_env[i].sym, sym) == 0) {
            global_env[i].val = val;
            return;
        }
    }
    if (global_env_count < 128) {
        kstrcpy(global_env[global_env_count].sym, sym);
        global_env[global_env_count].val = val;
        global_env_count++;
    }
}

static LispNode* env_get(const char* sym, EnvVar* local_env, int local_count) {
    for(int i=0; i<local_count; i++) {
        if (kstrcmp(local_env[i].sym, sym) == 0) return local_env[i].val;
    }
    for(int i=0; i<global_env_count; i++) {
        if (kstrcmp(global_env[i].sym, sym) == 0) return global_env[i].val;
    }
    
    // Builtin functions evaluation as symbols directly
    if (kstrcmp(sym, "+") == 0 || kstrcmp(sym, "-") == 0 || kstrcmp(sym, "*") == 0 || kstrcmp(sym, "/") == 0 || kstrcmp(sym, "print") == 0) {
        return new_lval_sym(sym);
    }
    
    return new_lval_err("Variavel/Funcao nao definida");
}

// Evaluator
static LispNode* eval(LispNode* ast, EnvVar* local_env, int local_count) {
    if (!ast) return new_lval_nil();
    if (ast->type == LVAL_NUM) return ast;
    if (ast->type == LVAL_SYM) return env_get(ast->sym, local_env, local_count);
    if (ast->type == LVAL_NIL) return ast;
    
    if (ast->type == LVAL_LIST) {
        LispNode* first = ast->first;
        
        // Special Forms
        if (first->type == LVAL_SYM) {
            if (kstrcmp(first->sym, "defun") == 0) {
                LispNode* rest1 = ast->rest;
                LispNode* name = rest1->first;
                LispNode* args = rest1->rest->first;
                LispNode* body = rest1->rest->rest->first; // we assume 1 expr body for simplicity
                LispNode* func = new_lval_func(args, body);
                env_set(name->sym, func);
                return new_lval_sym("ok");
            }
            if (kstrcmp(first->sym, "setq") == 0) {
                LispNode* name = ast->rest->first;
                LispNode* val = eval(ast->rest->rest->first, local_env, local_count);
                env_set(name->sym, val);
                return val;
            }
        }
        
        // Function call
        LispNode* f = eval(first, local_env, local_count);
        if (f->type == LVAL_ERR) return f;
        
        // Eval args
        LispNode* args_eval[16];
        int ac = 0;
        LispNode* curr = ast->rest;
        while(curr && curr->type == LVAL_LIST) {
            args_eval[ac++] = eval(curr->first, local_env, local_count);
            curr = curr->rest;
        }
        
        // Builtins
        if (f->type == LVAL_SYM) {
            if (kstrcmp(f->sym, "+") == 0) {
                int res = 0;
                for(int i=0; i<ac; i++) res += args_eval[i]->num;
                return new_lval_num(res);
            }
            if (kstrcmp(f->sym, "-") == 0) {
                if (ac == 1) return new_lval_num(-args_eval[0]->num);
                int res = args_eval[0]->num;
                for(int i=1; i<ac; i++) res -= args_eval[i]->num;
                return new_lval_num(res);
            }
            if (kstrcmp(f->sym, "*") == 0) {
                int res = 1;
                for(int i=0; i<ac; i++) res *= args_eval[i]->num;
                return new_lval_num(res);
            }
            if (kstrcmp(f->sym, "/") == 0) {
                if (ac > 1 && args_eval[1]->num != 0) {
                    return new_lval_num(args_eval[0]->num / args_eval[1]->num);
                }
                return new_lval_err("Divisao por zero");
            }
            if (kstrcmp(f->sym, "print") == 0) {
                for(int i=0; i<ac; i++) {
                    if (args_eval[i]->type == LVAL_NUM) {
                        print_int(args_eval[i]->num);
                    } else if (args_eval[i]->type == LVAL_SYM) {
                        vga_print(args_eval[i]->sym);
                    }
                    vga_putchar(' ');
                }
                vga_print("\n");
                return new_lval_nil();
            }
        } else if (f->type == LVAL_FUNC) {
            // Setup local env
            EnvVar new_env[16];
            int new_count = 0;
            LispNode* p = f->first; // params
            int ai = 0;
            while(p && p->type == LVAL_LIST && ai < ac) {
                kstrcpy(new_env[new_count].sym, p->first->sym);
                new_env[new_count].val = args_eval[ai++];
                new_count++;
                p = p->rest;
            }
            return eval(f->rest, new_env, new_count); // evaluate body
        }
        
        return new_lval_err("Nao e uma funcao valida");
    }
    
    return new_lval_err("Erro de avaliacao");
}

void _start(const char* args) {
    const char* filename = "lisp.txt";
    if (args && args[0] != '\0') {
        filename = args;
    }
    
    int fsize = fat16_get_file_size(filename);
    if (fsize == 0) {
        vga_print("Erro: Arquivo nao encontrado ou vazio no HD.\n");
        sys_exit();
    }
    
    char* buffer = (char*)lisp_malloc(fsize + 1);
    if (!buffer) {
        vga_print("Erro: Script muito grande para o Memory Pool (1 MB).\n");
        sys_exit();
    }
    
    int size = fat16_read_file(filename, (uint8_t*)buffer);
    if (size == 0) {
        vga_print("Erro ao ler arquivo do HD.\n");
        sys_exit();
    }
    
    buffer[size] = '\0';
    
    // Reset pool
    global_env_count = 0;
    
    vga_print("Executando Script LISP...\n");
    
    lisp_src = (const char*)buffer;
    
    LispNode* last_res = NULL;
    while(peek() != '\0') {
        LispNode* ast = read_expr();
        if (!ast) break;
        if (ast->type == LVAL_ERR) {
            vga_print("LISP PARSE ERROR: ");
            vga_print(ast->sym);
            vga_print("\n");
            sys_exit();
        }
        
        last_res = eval(ast, NULL, 0);
        
        if (last_res->type == LVAL_ERR) {
            vga_print("LISP EVAL ERROR: ");
            vga_print(last_res->sym);
            vga_print("\n");
            sys_exit();
        }
    }
    
    if (last_res) {
        vga_print("Resultado: ");
        if (last_res->type == LVAL_NUM) {
            print_int(last_res->num);
        } else if (last_res->type == LVAL_SYM) {
            vga_print(last_res->sym);
        } else if (last_res->type == LVAL_NIL) {
            vga_print("nil");
        } else if (last_res->type == LVAL_FUNC) {
            vga_print("#<function>");
        }
        vga_print("\n");
    }
    sys_exit();
}
