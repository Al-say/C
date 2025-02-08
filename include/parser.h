#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Markdown 块类型定义
typedef enum {
    BLOCK_PARAGRAPH,
    BLOCK_HEADING,
    BLOCK_CODE,
    BLOCK_QUOTE,
    BLOCK_LIST,
    BLOCK_HR
} BlockType;

// Markdown 块结构体
typedef struct Block {
    BlockType type;
    char* content;
    int level;  // 用于标题级别和列表嵌套层级
    struct Block* next;
} Block;

// 内存池结构体定义
typedef struct {
    char* pool;
    size_t used;
    size_t capacity;
} MemPool;

// 解析器配置结构体
typedef struct {
    int enable_toc;           // 是否生成目录
    int enable_footnotes;     // 是否支持脚注
    int enable_syntax_highlight;  // 是否启用代码高亮
    char* syntax_theme;       // 代码高亮主题
} ParserConfig;

// 解析器上下文结构体
typedef struct {
    MemPool* pool;           // 内存池
    ParserConfig config;     // 解析器配置
    Block* first_block;      // 第一个块
    Block* current_block;    // 当前处理的块
} ParserContext;

// 内存池操作函数
MemPool* create_memory_pool(size_t initial_size);
void* pool_alloc(MemPool* pool, size_t size);
void destroy_memory_pool(MemPool* pool);

// 高级解析函数
ParserContext* create_parser_context(const ParserConfig* config);
void destroy_parser_context(ParserContext* ctx);
int parse_markdown_with_context(ParserContext* ctx, const char* input_path);
char* get_html_output(ParserContext* ctx);

// 工具函数
char* escape_html(const char* str);
int is_valid_path(const char* path);
void sanitize_html(char* html);

#endif /* PARSER_H */
