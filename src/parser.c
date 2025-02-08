#include "../include/parser.h"
#include <ctype.h>

#define POOL_INITIAL_SIZE (1024 * 1024)  // 1MB
#define MAX_LINE_LENGTH 4096
#define MAX_HEADING_LEVEL 6

// 内存池实现
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

// 解析器上下文实现
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
        // 默认配置
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

// 创建块的实现
static Block* create_block(ParserContext* ctx, BlockType type) {
    Block* block = (Block*)pool_alloc(ctx->pool, sizeof(Block));
    if (!block) return NULL;
    
    block->type = type;
    block->content = NULL;
    block->level = 0;
    block->next = NULL;
    return block;
}

// 将块添加到解析器上下文中
static void add_block(ParserContext* ctx, Block* block) {
    if (!ctx->first_block) {
        ctx->first_block = block;
    } else {
        ctx->current_block->next = block;
    }
    ctx->current_block = block;
}

// 解析标题
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

// 解析列表项
static Block* parse_list_item(ParserContext* ctx, const char* line) {
    if (!line || !(*line == '-' || *line == '*' || isdigit(*line))) return NULL;
    
    // 跳过列表标记和空格
    const char* content = line;
    if (*content == '-' || *content == '*') {
        content++;
    } else {
        while (isdigit(*content)) content++;
        if (*content == '.') content++;
    }
    while (isspace(*content)) content++;
    
    Block* block = create_block(ctx, BLOCK_LIST);
    if (!block) return NULL;
    
    size_t content_len = strlen(content);
    block->content = (char*)pool_alloc(ctx->pool, content_len + 1);
    if (!block->content) return NULL;
    
    strcpy(block->content, content);
    return block;
}

// 解析代码块
static Block* parse_code_block(ParserContext* ctx, const char** ptr) {
    const char* start = *ptr;
    if (strncmp(start, "```", 3) != 0) return NULL;
    
    // Skip opening marker and get language identifier
    start += 3;
    const char* lang_start = start;
    while (*start && *start != '\n') start++;
    size_t lang_len = start - lang_start;
    
    // Skip newline after language identifier
    if (*start == '\n') start++;
    
    // Find closing marker
    const char* end = strstr(start, "\n```");
    if (!end) return NULL;
    
    Block* block = create_block(ctx, BLOCK_CODE);
    if (!block) return NULL;
    
    // Calculate content length and allocate memory
    size_t code_len = end - start;
    size_t total_len = lang_len + code_len + 2;  // +2 for null terminators
    block->content = (char*)pool_alloc(ctx->pool, total_len);
    if (!block->content) return NULL;
    
    // Store language identifier
    if (lang_len > 0) {
        strncpy(block->content, lang_start, lang_len);
        block->content[lang_len] = '\0';
        block->level = lang_len;  // Use level to store language identifier length
    } else {
        block->content[0] = '\0';
        block->level = 0;
    }
    
    // Store code content
    char* code_content = block->content + lang_len + 1;
    
    // Process code content
    const char* src = start;
    char* dst = code_content;
    int in_whitespace = 1;  // Track if we're in leading whitespace
    int line_start = 1;     // Track if we're at the start of a line
    
    while (src < end) {
        if (line_start) {
            // Skip common indentation at the start of lines
            while (isspace(*src) && *src != '\n') src++;
            line_start = 0;
            in_whitespace = 1;
        }
        
        if (*src == '\n') {
            *dst++ = *src++;
            line_start = 1;
            in_whitespace = 1;
        } else if (isspace(*src)) {
            // Collapse multiple spaces into one, except in code
            if (!in_whitespace) {
                *dst++ = ' ';
                in_whitespace = 1;
            }
            src++;
        } else {
            *dst++ = *src++;
            in_whitespace = 0;
        }
    }
    
    // Remove trailing whitespace
    while (dst > code_content && isspace(*(dst-1))) {
        dst--;
    }
    *dst = '\0';
    
    // Update pointer position to after the closing marker
    *ptr = end + 4;  // Skip "\n```"
    if (**ptr == '\n') (*ptr)++;  // Skip additional newline if present
    
    return block;
}

// 从字符串解析Markdown
int parse_markdown_with_context(ParserContext* ctx, const char* content) {
    if (!ctx || !content) return 0;
    
    const char* ptr = content;
    char line[MAX_LINE_LENGTH];
    Block* block = NULL;
    int in_list = 0;  // Track if we're in a list
    int list_type = 0;  // 0: no list, 'u': unordered list, 'o': ordered list
    
    // Skip YAML front matter
    if (strncmp(ptr, "---\n", 4) == 0) {
        ptr += 4;
        while (*ptr) {
            const char* eol = strchr(ptr, '\n');
            if (!eol) break;
            
            size_t line_len = eol - ptr;
            if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
            
            strncpy(line, ptr, line_len);
            line[line_len] = '\0';
            
            if (strcmp(line, "---") == 0) {
                ptr = eol + 1;
                // Skip any additional newlines after front matter
                while (*ptr == '\n') ptr++;
                break;
            }
            
            ptr = eol + 1;
        }
    }
    
    // Process the actual content
    while (*ptr) {
        // Check for code block
        if (strncmp(ptr, "```", 3) == 0) {
            if (in_list) {
                // End current list
                block = create_block(ctx, BLOCK_LIST);
                if (!block) return 0;
                block->content = NULL;  // Mark list end
                add_block(ctx, block);
                in_list = 0;
                list_type = 0;
            }
            block = parse_code_block(ctx, &ptr);
            if (block) {
                add_block(ctx, block);
                continue;
            }
        }
        
        // Read a line
        const char* eol = strchr(ptr, '\n');
        size_t line_len;
        if (eol) {
            line_len = eol - ptr;
            if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
            strncpy(line, ptr, line_len);
            line[line_len] = '\0';
            ptr = eol + 1;
        } else {
            line_len = strlen(ptr);
            if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
            strncpy(line, ptr, line_len);
            line[line_len] = '\0';
            ptr += line_len;
        }
        
        // Skip empty lines
        if (line_len == 0) {
            if (in_list) {
                // End current list
                block = create_block(ctx, BLOCK_LIST);
                if (!block) return 0;
                block->content = NULL;  // Mark list end
                add_block(ctx, block);
                in_list = 0;
                list_type = 0;
            }
            continue;
        }
        
        // Parse heading
        if (line[0] == '#') {
            if (in_list) {
                // End current list
                block = create_block(ctx, BLOCK_LIST);
                if (!block) return 0;
                block->content = NULL;  // Mark list end
                add_block(ctx, block);
                in_list = 0;
                list_type = 0;
            }
            block = parse_heading(ctx, line);
            if (block) {
                add_block(ctx, block);
                continue;
            }
        }
        
        // Parse list item
        if (line[0] == '-' || line[0] == '*' || isdigit(line[0])) {
            int new_list_type = isdigit(line[0]) ? 'o' : 'u';
            if (!in_list || list_type != new_list_type) {
                if (in_list) {
                    // End current list
                    block = create_block(ctx, BLOCK_LIST);
                    if (!block) return 0;
                    block->content = NULL;  // Mark list end
                    add_block(ctx, block);
                }
                // Start new list
                block = create_block(ctx, BLOCK_LIST);
                if (!block) return 0;
                block->content = (char*)pool_alloc(ctx->pool, 2);
                if (!block->content) return 0;
                block->content[0] = new_list_type;  // Mark list type
                block->content[1] = '\0';
                add_block(ctx, block);
                in_list = 1;
                list_type = new_list_type;
            }
            block = parse_list_item(ctx, line);
            if (block) {
                add_block(ctx, block);
                continue;
            }
        } else if (in_list) {
            // End current list
            block = create_block(ctx, BLOCK_LIST);
            if (!block) return 0;
            block->content = NULL;  // Mark list end
            add_block(ctx, block);
            in_list = 0;
            list_type = 0;
        }
        
        // If not recognized as other block, treat as paragraph
        block = create_block(ctx, BLOCK_PARAGRAPH);
        if (!block) return 0;
        
        block->content = (char*)pool_alloc(ctx->pool, line_len + 1);
        if (!block->content) return 0;
        
        strcpy(block->content, line);
        add_block(ctx, block);
    }
    
    // If still in a list, end it
    if (in_list) {
        block = create_block(ctx, BLOCK_LIST);
        if (!block) return 0;
        block->content = NULL;  // Mark list end
        add_block(ctx, block);
    }
    
    return 1;
}

// 生成HTML输出
char* get_html_output(ParserContext* ctx) {
    if (!ctx || !ctx->first_block) return NULL;
    
    // 预估需要的缓冲区大小
    size_t buffer_size = 4096;
    char* output = (char*)malloc(buffer_size);
    if (!output) return NULL;
    
    size_t used = 0;
    Block* block = ctx->first_block;
    int in_list = 0;  // 跟踪是否在列表中
    
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
                if (strlen(block->content) > 0) {  // 只输出非空段落
                    block_size = strlen(block->content) + 50;
                    block_html = (char*)malloc(block_size);
                    if (block_html) {
                        snprintf(block_html, block_size, "<p>%s</p>\n", 
                                block->content);
                    }
                }
                break;
                
            case BLOCK_LIST:
                if (!block->content) {
                    // 列表结束
                    block_html = (char*)malloc(10);
                    if (block_html) {
                        snprintf(block_html, 10, "</%cl>\n", in_list);
                        in_list = 0;
                    }
                } else if (block->content[0] == 'u' || block->content[0] == 'o') {
                    // 列表开始
                    in_list = block->content[0];
                    block_html = (char*)malloc(10);
                    if (block_html) {
                        snprintf(block_html, 10, "<%cl>\n", 
                                block->content[0] == 'u' ? 'u' : 'o');
                    }
                } else {
                    // 列表项
                    block_size = strlen(block->content) + 50;
                    block_html = (char*)malloc(block_size);
                    if (block_html) {
                        snprintf(block_html, block_size, "<li>%s</li>\n",
                                block->content);
                    }
                }
                break;
                
            case BLOCK_CODE:
                {
                    const char* lang = block->content;
                    const char* code = block->content + block->level + 1;
                    block_size = strlen(code) + block->level + 100;
                    block_html = (char*)malloc(block_size);
                    if (block_html) {
                        if (block->level > 0) {
                            // Trim any whitespace from language identifier
                            char lang_buf[32];
                            size_t i = 0;
                            while (i < block->level && i < sizeof(lang_buf)-1 && !isspace(lang[i])) {
                                lang_buf[i] = lang[i];
                                i++;
                            }
                            lang_buf[i] = '\0';
                            
                            snprintf(block_html, block_size, 
                                    "<pre><code class=\"language-%s\">%s</code></pre>\n",
                                    lang_buf, code);
                        } else {
                            snprintf(block_html, block_size, 
                                    "<pre><code>%s</code></pre>\n",
                                    code);
                        }
                    }
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
    
    // 确保所有列表都被关闭
    if (in_list) {
        char list_end[10];
        snprintf(list_end, sizeof(list_end), "</%cl>\n", in_list);
        size_t len = strlen(list_end);
        if (used + len >= buffer_size) {
            buffer_size *= 2;
            char* new_output = (char*)realloc(output, buffer_size);
            if (!new_output) {
                free(output);
                return NULL;
            }
            output = new_output;
        }
        strcpy(output + used, list_end);
        used += len;
    }
    
    return output;
}

// 转义HTML
char* escape_html(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    size_t escaped_len = len * 6 + 1;  // 最坏情况下，每个字符都需要转义
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
    
    // 检查路径是否包含危险字符串，如"..", "//", "~"
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
