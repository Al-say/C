#include "../include/generator.h"
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#define MKDIR(path) _mkdir(path)
#define F_OK 0
#define access _access
#define MAX_PATH 260
#else
#include <unistd.h>
#include <dirent.h>
#define PATH_SEPARATOR '/'
#define MKDIR(path) mkdir(path, 0755)
#endif

// 辅助函数：检查文件是否存在
static int file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

// 辅助函数：检查是否是Markdown文件
static int is_markdown_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    return ext && strcmp(ext, ".md") == 0;
}

// 辅助函数：连接路径
static char* join_path(const char* dir, const char* file) {
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    char* path = malloc(dir_len + file_len + 2);
    
    if (path) {
        strcpy(path, dir);
        if (dir[dir_len - 1] != PATH_SEPARATOR) {
            path[dir_len] = PATH_SEPARATOR;
            strcpy(path + dir_len + 1, file);
        } else {
            strcpy(path + dir_len, file);
        }
    }
    return path;
}

// 生成器上下文操作
GeneratorContext* create_generator_context(const BlogConfig* config, const char* output_dir) {
    GeneratorContext* ctx = (GeneratorContext*)malloc(sizeof(GeneratorContext));
    if (!ctx) return NULL;
    
    ctx->pool = create_memory_pool(1024 * 1024);  // 1MB
    if (!ctx->pool) {
        free(ctx);
        return NULL;
    }
    
    ctx->config = (BlogConfig*)pool_alloc(ctx->pool, sizeof(BlogConfig));
    if (!ctx->config) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
        return NULL;
    }
    
    memcpy(ctx->config, config, sizeof(BlogConfig));
    
    size_t dir_len = strlen(output_dir);
    ctx->output_dir = (char*)pool_alloc(ctx->pool, dir_len + 1);
    if (!ctx->output_dir) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
        return NULL;
    }
    strcpy(ctx->output_dir, output_dir);
    
    ctx->template_dir = NULL;  // 将在后续设置
    
    return ctx;
}

void destroy_generator_context(GeneratorContext* ctx) {
    if (ctx) {
        destroy_memory_pool(ctx->pool);
        free(ctx);
    }
}

// 创建目录结构
void create_directory_structure(const char* base_dir) {
    char* dirs[] = {
        "posts",
        "assets",
        "assets/css",
        "assets/js",
        "assets/images",
        "templates"
    };
    
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++) {
        char* path = join_path(base_dir, dirs[i]);
        if (path) {
            MKDIR(path);
            free(path);
        }
    }
}

// 提取文章元数据
PostMetadata* extract_post_metadata(const char* markdown_content) {
    if (!markdown_content) return NULL;
    
    PostMetadata* metadata = malloc(sizeof(PostMetadata));
    if (!metadata) return NULL;
    
    // 初始化默认值
    metadata->title = NULL;
    metadata->date = NULL;
    metadata->author = NULL;
    metadata->description = NULL;
    metadata->tags = NULL;
    metadata->tag_count = 0;
    metadata->permalink = NULL;
    
    char line[1024];
    const char* ptr = markdown_content;
    int in_metadata = 0;
    
    // 查找元数据块 (YAML front matter)
    if (strncmp(ptr, "---\n", 4) == 0) {
        in_metadata = 1;
        ptr += 4;
        
        while (*ptr && in_metadata) {
            size_t i = 0;
            while (*ptr && *ptr != '\n' && i < sizeof(line)-1) {
                line[i++] = *ptr++;
            }
            line[i] = '\0';
            if (*ptr) ptr++;
            
            if (strcmp(line, "---") == 0) {
                in_metadata = 0;
                break;
            }
            
            char* key = line;
            char* value = strchr(line, ':');
            if (value) {
                *value = '\0';
                value++;
                while (isspace(*value)) value++;
                
                if (strcmp(key, "title") == 0) {
                    metadata->title = strdup(value);
                } else if (strcmp(key, "date") == 0) {
                    metadata->date = strdup(value);
                } else if (strcmp(key, "author") == 0) {
                    metadata->author = strdup(value);
                } else if (strcmp(key, "description") == 0) {
                    metadata->description = strdup(value);
                }
            }
        }
    }
    
    return metadata;
}

// 生成永久链接
char* generate_permalink(const char* title, const char* date) {
    if (!title || !date) return NULL;
    
    char* permalink = malloc(strlen(title) + strlen(date) + 2);
    if (!permalink) return NULL;
    
    // 简化的永久链接生成
    sprintf(permalink, "%s-%s", date, title);
    
    // 将空格转换为连字符
    for (char* p = permalink; *p; p++) {
        if (isspace(*p)) *p = '-';
    }
    
    return permalink;
}

// 处理文章
void process_posts(GeneratorContext* ctx, const char* posts_dir) {
#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.md", posts_dir);
    
    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) return;
    
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char* post_path = join_path(posts_dir, find_data.cFileName);
            if (post_path && is_markdown_file(post_path)) {
                FILE* fp = fopen(post_path, "r");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long size = ftell(fp);
                    rewind(fp);
                    
                    char* content = malloc(size + 1);
                    if (content) {
                        if (fread(content, 1, size, fp) == (size_t)size) {
                            content[size] = '\0';
                            PostMetadata* metadata = extract_post_metadata(content);
                            if (metadata) {
                                generate_post_page(ctx, content, metadata);
                                free(metadata);
                            }
                        }
                        free(content);
                    }
                    fclose(fp);
                }
                free(post_path);
            }
        }
    } while (FindNextFile(find_handle, &find_data));
    
    FindClose(find_handle);
#else
    DIR* dir = opendir(posts_dir);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        char* post_path = join_path(posts_dir, entry->d_name);
        if (post_path && is_markdown_file(post_path)) {
            FILE* fp = fopen(post_path, "r");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                rewind(fp);
                
                char* content = malloc(size + 1);
                if (content) {
                    if (fread(content, 1, size, fp) == (size_t)size) {
                        content[size] = '\0';
                        PostMetadata* metadata = extract_post_metadata(content);
                        if (metadata) {
                            generate_post_page(ctx, content, metadata);
                            free(metadata);
                        }
                    }
                    free(content);
                }
                fclose(fp);
            }
            free(post_path);
        }
    }
    closedir(dir);
#endif
}

// 生成文章页面
void generate_post_page(GeneratorContext* ctx, const char* markdown_content, PostMetadata* metadata) {
    ParserContext* parser_ctx = create_parser_context(NULL);
    if (!parser_ctx) return;
    
    if (parse_markdown_with_context(parser_ctx, markdown_content)) {
        char* html_content = get_html_output(parser_ctx);
        
        if (html_content) {
            char* output_path = join_path(ctx->output_dir, 
                metadata->permalink ? metadata->permalink : "post.html");
            
            if (output_path) {
                FILE* fp = fopen(output_path, "w");
                if (fp) {
                    char* page = apply_template("templates/post.html", html_content, metadata);
                    if (page) {
                        fputs(page, fp);
                        free(page);
                    }
                    fclose(fp);
                }
                free(output_path);
            }
            free(html_content);
        }
    }
    
    destroy_parser_context(parser_ctx);
}

// 生成网站
void generate_blog(const char* output_dir) {
    BlogConfig config = {
        .site_title = "My C Blog",
        .site_description = "A static blog generated by C",
        .author = "Anonymous",
        .theme = "default",
        .base_url = "http://localhost"
    };
    
    GeneratorContext* ctx = create_generator_context(&config, output_dir);
    if (!ctx) return;
    
    create_directory_structure(output_dir);
    process_posts(ctx, "posts");
    generate_index_page(ctx);
    generate_tag_pages(ctx);
    generate_archive_page(ctx);
    generate_rss_feed(ctx);
    generate_sitemap(ctx);
    
    destroy_generator_context(ctx);
}

// RSS和站点地图生成
void generate_rss_feed(GeneratorContext* ctx) {
    char* rss_path = join_path(ctx->output_dir, "rss.xml");
    if (!rss_path) return;
    
    FILE* fp = fopen(rss_path, "w");
    if (fp) {
        fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(fp, "<rss version=\"2.0\">\n");
        fprintf(fp, "  <channel>\n");
        fprintf(fp, "    <title>%s</title>\n", ctx->config->site_title);
        fprintf(fp, "    <description>%s</description>\n", ctx->config->site_description);
        fprintf(fp, "    <link>%s</link>\n", ctx->config->base_url);
        fprintf(fp, "  </channel>\n");
        fprintf(fp, "</rss>\n");
        fclose(fp);
    }
    
    free(rss_path);
}

void generate_sitemap(GeneratorContext* ctx) {
    char* sitemap_path = join_path(ctx->output_dir, "sitemap.xml");
    if (!sitemap_path) return;
    
    FILE* fp = fopen(sitemap_path, "w");
    if (fp) {
        fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(fp, "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
        fprintf(fp, "</urlset>\n");
        fclose(fp);
    }
    
    free(sitemap_path);
}

// 索引页和归档页生成
void generate_index_page(GeneratorContext* ctx) {
    char* index_path = join_path(ctx->output_dir, "index.html");
    if (!index_path) return;
    
    FILE* fp = fopen(index_path, "w");
    if (fp) {
        fprintf(fp, "<html><body><h1>Welcome to %s</h1></body></html>\n",
                ctx->config->site_title);
        fclose(fp);
    }
    
    free(index_path);
}

void generate_tag_pages(GeneratorContext* ctx) {
    char* tags_dir = join_path(ctx->output_dir, "tags");
    if (!tags_dir) return;
    
    MKDIR(tags_dir);
    free(tags_dir);
}

void generate_archive_page(GeneratorContext* ctx) {
    char* archive_path = join_path(ctx->output_dir, "archive.html");
    if (!archive_path) return;
    
    FILE* fp = fopen(archive_path, "w");
    if (fp) {
        fprintf(fp, "<html><body><h1>Archives</h1></body></html>\n");
        fclose(fp);
    }
    
    free(archive_path);
}
