#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

#include "../include/generator.h"
#include "../include/parser.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <output_dir>\n", argv[0]);
        return 1;
    }
    
    const char* output_dir = argv[1];
    
    // 创建输出目录
    if (mkdir(output_dir, 0755) != 0) {
        if (errno != EEXIST) {
            printf("Error: Could not create output directory\n");
            return 1;
        }
    }
    
    // 创建博客配置
    BlogConfig config = {
        .blog_title = "My C Blog",
        .blog_description = "A static blog generator written in C",
        .author = "Alsay",
        .base_url = "https://example.com",
        .theme = "default"
    };
    
    // 创建生成器上下文
    GeneratorContext* ctx = create_generator_context(&config, output_dir);
    if (!ctx) {
        printf("Error: Could not create generator context\n");
        return 1;
    }
    
    // 创建目录结构
    create_directory_structure(output_dir);
    
    // 处理文章
    process_posts(ctx, "posts");
    
    // 生成其他页面
    generate_index_page(ctx);
    generate_tag_pages(ctx);
    generate_archive_page(ctx);
    generate_rss_feed(ctx);
    generate_sitemap(ctx);
    
    // 清理资源
    destroy_generator_context(ctx);
    
    printf("Blog generation completed successfully!\n");
    return 0;
}
