#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Markdown �ļ���������
void parse_markdown(const char* input_path, char** html_output);

// �ڴ�ؽṹ�嶨��
typedef struct {
    char* pool;
    size_t used;
    size_t capacity;
} MemPool;

// �ڴ�ز�������
MemPool* create_memory_pool(size_t initial_size);
void* pool_alloc(MemPool* pool, size_t size);
void destroy_memory_pool(MemPool* pool);

// Markdown ����֧�ֵĿ�����
typedef enum {
    BLOCK_PARAGRAPH,
    BLOCK_HEADING,
    BLOCK_CODE,
    BLOCK_QUOTE,
    BLOCK_LIST,
    BLOCK_HR
} BlockType;

// Markdown ��ṹ��
typedef struct {
    BlockType type;
    char* content;
    int level;  // ���ڱ��⼶����б�Ƕ�׼���
    struct Block* next;
} Block;

// ���������ýṹ��
typedef struct {
    int enable_toc;           // �Ƿ�����Ŀ¼
    int enable_footnotes;     // �Ƿ�֧�ֽ�ע
    int enable_syntax_highlight;  // �Ƿ����ô������
    char* syntax_theme;       // �����������
} ParserConfig;

// �����������Ľṹ��
typedef struct {
    MemPool* pool;           // �ڴ��
    ParserConfig config;     // ����������
    Block* first_block;      // ��һ����
    Block* current_block;    // ��ǰ����Ŀ�
} ParserContext;

// �߼���������
ParserContext* create_parser_context(const ParserConfig* config);
void destroy_parser_context(ParserContext* ctx);
int parse_markdown_with_context(ParserContext* ctx, const char* input_path);
char* get_html_output(ParserContext* ctx);

// ���ߺ���
char* escape_html(const char* str);
int is_valid_path(const char* path);
void sanitize_html(char* html);

#endif /* PARSER_H */
