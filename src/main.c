#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../include/parser.h"
#include "../include/generator.h"

#define MAX_PATH 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* output_dir = argv[1];
    
    // 确保输出目录存在
    #ifdef _WIN32
        _mkdir(output_dir);
    #else
        mkdir(output_dir, 0755);
    #endif

    // 生成博客
    generate_blog(output_dir);
    
    printf("Blog generation completed successfully!\n");
    return EXIT_SUCCESS;
}
