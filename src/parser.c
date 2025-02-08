#include "../include/parser.h"
#include <ctype.h>

#define POOL_INITIAL_SIZE (1024 * 1024)  // 1MB
#define MAX_LINE_LENGTH 4096
#define MAX_HEADING_LEVEL 6

// �ڴ�غ���ʵ��
MemPool* create_memory_pool(size_t initial_size) {
    MemPool* pool = (MemPool*)malloc(sizeof(MemPool));
    if (!pool) return NULL;
    
    pool->pool = (char*)malloc(initial_size);
    if (!pool->pool) {
        free(pool);
        return NULL;
    }
    
    pool->capacity = initial_size;
    pool->used = 0;
    return pool;
}

void* pool_alloc(MemPool* pool, size_t size) {
    if (pool->used + size > pool->capacity) {
        size_t new_capacity = pool->capacity * 2;
        char* new_pool = (char*)realloc(pool->pool, new_capacity);
        if (!new_pool) return NULL;
        
        pool->pool = new_pool;
        pool->capacity = new_capacity;
    }
    
    void* ptr = pool->pool + pool->used;
    pool->used += size;
    return ptr;
}

void destroy_memory_pool(MemPool* pool) {
    if (pool) {
        if (pool->pool) free(pool->pool);
        free(pool);
    }
}

// �����������ĺ���ʵ��
ParserContext* create_parser_context(const ParserConfig* config) {
    ParserContext* ctx = (ParserContext*)malloc(sizeof(ParserContext));
    if (!ctx) return NULL;
    
    ctx->pool = create_memory_pool(POOL_INITIAL_SIZE);
    if (!ctx->pool) {
        free(ctx);
        return NULL;
    }
    
    if (config) {
        memcpy(&ctx->config, config, sizeof(ParserConfig));
    } else {
        // Ĭ������
        ctx->config.enable_toc = 1;
        ctx->config.enable_footnotes = 1;
        ctx->config.enable_syntax_highlight = 1;
        ctx->config.syntax_theme = "github";
    }
    
    ctx->first_block = NULL;
    ctx->current_block = NULL;
    return ctx;
}

void destroy_parser_context(ParserContext* ctx) {
    if (ctx) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
    }
}

// ���������������µĿ�
static Block* create_block(ParserContext* ctx, BlockType type) {
    Block* block = (Block*)pool_alloc(ctx->pool, sizeof(Block));
    if (!block) return NULL;
    
    block->type = type;
    block->content = NULL;
    block->level = 0;
    block->next = NULL;
    return block;
}

// ������������ӿ鵽����
static void add_block(ParserContext* ctx, Block* block) {
    if (!ctx->first_block) {
        ctx->first_block = block;
    } else {
        ctx->current_block->next = block;
    }
    ctx->current_block = block;
}

// ����������
static Block* parse_heading(ParserContext* ctx, const char* line) {
    int level = 0;
    while (line[level] == '#' && level < MAX_HEADING_LEVEL) {
        level++;
    }
    
    if (level == 0 || !isspace(line[level])) return NULL;
    
    Block* block = create_block(ctx, BLOCK_HEADING);
    if (!block) return NULL;
    
    block->level = level;
    const char* content = line + level;
    while (isspace(*content)) content++;
    
    size_t content_len = strlen(content);
    block->content = (char*)pool_alloc(ctx->pool, content_len + 1);
    if (!block->content) return NULL;
    
    strcpy(block->content, content);
    return block;
}

// ����������ʵ��
int parse_markdown_with_context(ParserContext* ctx, const char* input_path) {
    if (!is_valid_path(input_path)) return 0;
    
    FILE* fp = fopen(input_path, "r");
    if (!fp) return 0;
    
    char line[MAX_LINE_LENGTH];
    Block* block = NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        // �Ƴ���β�Ļ��з�
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        // ��������
        if (len == 0) continue;
        
        // ��������
        if (line[0] == '#') {
            block = parse_heading(ctx, line);
            if (block) {
                add_block(ctx, block);
                continue;
            }
        }
        
        // ���û��ʶ��Ϊ����飬����Ϊ���䴦��
        block = create_block(ctx, BLOCK_PARAGRAPH);
        if (!block) {
            fclose(fp);
            return 0;
        }
        
        block->content = (char*)pool_alloc(ctx->pool, len + 1);
        if (!block->content) {
            fclose(fp);
            return 0;
        }
        
        strcpy(block->content, line);
        add_block(ctx, block);
    }
    
    fclose(fp);
    return 1;
}

// ����HTML���
char* get_html_output(ParserContext* ctx) {
    if (!ctx || !ctx->first_block) return NULL;
    
    // Ԥ����Ҫ�Ļ�������С
    size_t buffer_size = 4096;
    char* output = (char*)malloc(buffer_size);
    if (!output) return NULL;
    
    size_t used = 0;
    Block* block = ctx->first_block;
    
    while (block) {
        char* block_html = NULL;
        size_t block_size = 0;
        
        switch (block->type) {
            case BLOCK_HEADING:
                block_size = strlen(block->content) + 50;
                block_html = (char*)malloc(block_size);
                if (block_html) {
                    snprintf(block_html, block_size, "<h%d>%s</h%d>\n", 
                            block->level, block->content, block->level);
                }
                break;
                
            case BLOCK_PARAGRAPH:
                block_size = strlen(block->content) + 50;
                block_html = (char*)malloc(block_size);
                if (block_html) {
                    snprintf(block_html, block_size, "<p>%s</p>\n", 
                            block->content);
                }
                break;
                
            default:
                break;
        }
        
        if (block_html) {
            size_t html_len = strlen(block_html);
            if (used + html_len >= buffer_size) {
                buffer_size *= 2;
                char* new_output = (char*)realloc(output, buffer_size);
                if (!new_output) {
                    free(block_html);
                    free(output);
                    return NULL;
                }
                output = new_output;
            }
            
            strcpy(output + used, block_html);
            used += html_len;
            free(block_html);
        }
        
        block = block->next;
    }
    
    return output;
}

// ���ߺ���ʵ��
char* escape_html(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    size_t escaped_len = len * 6 + 1;  // �����������ַ�����Ҫת��
    char* escaped = (char*)malloc(escaped_len);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '<':
                strcpy(escaped + j, "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(escaped + j, "&gt;");
                j += 4;
                break;
            case '&':
                strcpy(escaped + j, "&amp;");
                j += 5;
                break;
            case '"':
                strcpy(escaped + j, "&quot;");
                j += 6;
                break;
            default:
                escaped[j++] = str[i];
        }
    }
    escaped[j] = '\0';
    
    return escaped;
}

int is_valid_path(const char* path) {
    if (!path) return 0;
    
    // ���·�����Ƿ����Σ�յ�ģʽ
    const char* dangerous[] = {"..", "//", "~"};
    for (size_t i = 0; i < sizeof(dangerous)/sizeof(dangerous[0]); i++) {
        if (strstr(path, dangerous[i])) return 0;
    }
    
    return 1;
}

void sanitize_html(char* html) {
    if (!html) return;
    
    const char* patterns[] = {"<script", "javascript:", "onerror="};
    for (size_t i = 0; i < sizeof(patterns)/sizeof(patterns[0]); i++) {
        char* pos = NULL;
        while ((pos = strstr(html, patterns[i])) != NULL) {
            memset(pos, 'X', strlen(patterns[i]));
        }
    }
}
