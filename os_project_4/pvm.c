#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_PATH_LENGTH 256
#define PAGE_SIZE 4096

void pvm_frameinfo(int pid, unsigned long pfn) {
    char *path = "/proc/kpageflags";

    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        off_t offset = pfn * sizeof(uint64_t);
        if (lseek(fd, offset, SEEK_SET) != -1) {
            uint64_t flags;
            if (read(fd, &flags, sizeof(uint64_t)) == sizeof(uint64_t)) {
                printf("PFN: %lu\nFlags: 0x%lx\n", pfn, flags);
            }
        }
        close(fd);
    }
}

void pvm_mapall(int pid) {
    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp) != NULL) {
            printf("%s", line);
        }
        fclose(fp);
    }
}

void pvm_mapallin(int pid) {
    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp) != NULL) {
            unsigned long start, end;
            sscanf(line, "%lx-%lx", &start, &end);
            
            unsigned long num_pages = (end - start) / PAGE_SIZE;
            
            for (unsigned long i = 0; i < num_pages; i++) {
                unsigned long virtual_page_number = (start / PAGE_SIZE) + i;
                
                char pagemap_path[MAX_PATH_LENGTH];
                snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%d/pagemap", pid);
                
                int pagemap_fd = open(pagemap_path, O_RDONLY);
                if (pagemap_fd != -1) {
                    off_t offset = virtual_page_number * sizeof(uint64_t);
                    if (lseek(pagemap_fd, offset, SEEK_SET) != -1) {
                        uint64_t page_entry;
                        if (read(pagemap_fd, &page_entry, sizeof(uint64_t)) == sizeof(uint64_t)) {
                            if (page_entry & (1ULL << 63)) {
                                unsigned long pfn = page_entry & ((1ULL << 55) - 1);
                                printf("Page %lu is in memory. PFN: %lu\n", virtual_page_number, pfn);
                                
                                // Print frame information
                                pvm_frameinfo(pid, pfn);
                                
                            } else {
                                printf("Page %lu is not in memory.\n", virtual_page_number);
                            }
                        }
                    }
                    close(pagemap_fd);
                }
            }
        }
        fclose(fp);
    }
}

void pvm_alltablesize(int pid) {
    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[512];
        unsigned long total_table_size = 0;
        
        while (fgets(line, sizeof(line), fp) != NULL) {
            unsigned long start, end;
            sscanf(line, "%lx-%lx", &start, &end);
            
            // Calculate the number of pages for the given virtual address range
            unsigned long num_pages = (end - start) / PAGE_SIZE;
            
            // Calculate the size of the page table for the range
            unsigned long table_size = num_pages * sizeof(uint64_t);
            
            total_table_size += table_size;
        }
        
        printf("Total memory required for page tables: %lu bytes\n", total_table_size);
        
        fclose(fp);
    }
}


//3. option
void pvm_memory_usage(int pid) {
    char maps_path[MAX_PATH_LENGTH];
    char pagemap_path[MAX_PATH_LENGTH];
    char kpagecount_path[MAX_PATH_LENGTH];
    FILE *maps_fp, *pagemap_fp, *kpagecount_fp;
    char line[256];
    unsigned long long total_virtual_memory = 0;
    unsigned long long total_physical_memory = 0;
    unsigned long long exclusive_physical_memory = 0;

    sprintf(maps_path, "/proc/%d/maps", pid);
    sprintf(pagemap_path, "/proc/%d/pagemap", pid);
    sprintf(kpagecount_path, "/proc/kpagecount");

    maps_fp = fopen(maps_path, "r");
    pagemap_fp = fopen(pagemap_path, "rb");
    kpagecount_fp = fopen(kpagecount_path, "rb");

    if( maps_fp == NULL || pagemap_fp == NULL || kpagecount_fp == NULL){
        perror("Failed to open maps, pagemap, or kpagecount");
        exit(EXIT_FAILURE);
    }

    unsigned long long pfn, mapcount;
    int kpagecount_fd = fileno(kpagecount_fp);

    while (fgets(line, sizeof(line), maps_fp) != NULL) {
        unsigned long start, end;
        sscanf(line, "%lx-%lx", &start, &end);

        unsigned long num_pages = (end - start) / PAGE_SIZE;
        
        total_virtual_memory += num_pages * PAGE_SIZE;

        for (unsigned long i = 0; i < num_pages; i++) {
            unsigned long virtual_page_number = (start / PAGE_SIZE) + i;

            off_t offset = virtual_page_number * sizeof(uint64_t);
            if (lseek(fileno(pagemap_fp), offset, SEEK_SET) != -1) {
                uint64_t page_entry;
                if (read(fileno(pagemap_fp), &page_entry, sizeof(uint64_t)) == sizeof(uint64_t)) {
                    if (page_entry & (1ULL << 63)) {
                        pfn = page_entry & ((1ULL << 55) - 1);

                        total_physical_memory += PAGE_SIZE;

                        if (lseek(kpagecount_fd, pfn * sizeof(unsigned long long), SEEK_SET) != -1) {
                            if (read(kpagecount_fd, &mapcount, sizeof(unsigned long long)) == sizeof(unsigned long long)) {
                                if (mapcount == 1) {
                                    exclusive_physical_memory += PAGE_SIZE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    total_virtual_memory /= 1024;
    total_physical_memory /= 1024;
    exclusive_physical_memory /= 1024;

    printf("Virtual memory usage in KB: %llu\n", total_virtual_memory);
    printf("Physical memory usage in KB: %llu\n", total_physical_memory);
    printf("Exclusive physical memory usage in KB: %llu\n", exclusive_physical_memory);

    fclose(maps_fp);
    fclose(pagemap_fp);
    fclose(kpagecount_fp);
}


//4. option
//find and print the physical address of a virtual address VA of a process with PID
//print the physical address in hexadecimal
void pvm_virtual_to_physical(int pid, unsigned long va){
    char pagemap_path[MAX_PATH_LENGTH];
    FILE *pagemap_fp;
    unsigned long long pfn;
    unsigned long long offset;
    unsigned long long physical_address;

    sprintf(pagemap_path, "/proc/%d/pagemap", pid);
    pagemap_fp = fopen(pagemap_path, "rb");

    if(pagemap_fp == NULL){
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    unsigned long long page_number = va / PAGE_SIZE;
    offset = va % PAGE_SIZE;

    off_t offset_into_pagemap = page_number * sizeof(uint64_t);

    if (lseek(fileno(pagemap_fp), offset_into_pagemap, SEEK_SET) != -1) {
        uint64_t page_entry;
        if (read(fileno(pagemap_fp), &page_entry, sizeof(uint64_t)) == sizeof(uint64_t)) {
            if (page_entry & (1ULL << 63)) {
                pfn = page_entry & ((1ULL << 55) - 1);

                physical_address = (pfn * PAGE_SIZE) + offset;

                printf("Physical address of virtual address %lx: %llx\n", va, physical_address);
                
                // Print frame information
                pvm_frameinfo(pid, pfn);
                
            } else {
                printf("Virtual address %lx is not in memory.\n", va);
            }
        }
    }
    fclose(pagemap_fp);
}

//5. option
//find and print the info of the page of a virtual address VA of a process with PID
//use /proc/PID/pagemap to get the info
//print numbers in hexadecimal
void pvm_pageinfo(int pid, unsigned long va){
    char pagemap_path[MAX_PATH_LENGTH];
    FILE *pagemap_fp;
    unsigned long long pfn;
    unsigned long long offset;
    unsigned long long page_entry;
    unsigned long long page_flags;

    sprintf(pagemap_path, "/proc/%d/pagemap", pid);
    pagemap_fp = fopen(pagemap_path, "rb");

    if(pagemap_fp == NULL){
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    // Calculate the page number and offset into the page for the given virtual address
    unsigned long long page_number = va / PAGE_SIZE;
    offset = va % PAGE_SIZE;

    // Calculate the offset into the pagemap file
    off_t offset_into_pagemap = page_number * sizeof(uint64_t);

    // Read the pagemap entry for the given virtual address
    if (lseek(fileno(pagemap_fp), offset_into_pagemap, SEEK_SET) != -1) {
        if (read(fileno(pagemap_fp), &page_entry, sizeof(uint64_t)) == sizeof(uint64_t)) {
            if (page_entry & (1ULL << 63)) {
                // Page is present in memory
                pfn = page_entry & ((1ULL << 55) - 1);

                printf("PFN: %llx\n", pfn);
                printf("Offset: %llx\n", offset);
                printf("Page Entry: %llx\n", page_entry);
            } else {
                printf("Virtual address %lx is not present in memory\n", va);
            }
        }
    }

    fclose(pagemap_fp);
}

//6. option
//find and print the page number, frame number of the virtual address range [va1, va2) of a process with PID
//for each page in the range, print a line containing the page number and frame number (if any)
//if a page in the range is not used, print unused instead of the frame number
//if a page in the range is used but not in the memory, print not-in-memory instead of the frame number
void pvm_maprange(int pid, unsigned long va1, unsigned long va2){
    char maps_path[MAX_PATH_LENGTH];
    FILE *maps_fp;
    char line[512];

    sprintf(maps_path, "/proc/%d/maps", pid);

    maps_fp = fopen(maps_path, "r");

    if( maps_fp ) {
        while (fgets(line, sizeof(line), maps_fp) != NULL) {
            unsigned long start, end;
            sscanf(line, "%lx-%lx", &start, &end);

            // Calculate the number of pages for the given virtual address range
            unsigned long num_pages = (end - start) / PAGE_SIZE;

            // Check if each page is present in memory
            for (unsigned long i = 0; i < num_pages; i++) {
                unsigned long virtual_page_number = (start / PAGE_SIZE) + i;

                char pagemap_path[MAX_PATH_LENGTH];
                sprintf(pagemap_path, "/proc/%d/pagemap", pid);

                int pagemap_fd = open(pagemap_path, O_RDONLY);
                if( pagemap_fd != -1 ) {
                    if ( virtual_page_number >= (va1 / PAGE_SIZE) && virtual_page_number < (va2 / PAGE_SIZE) ) {
                        off_t offset = virtual_page_number * sizeof(uint64_t);
                        if (lseek(pagemap_fd, offset, SEEK_SET) != -1) {
                            uint64_t page_entry;
                            if (read(pagemap_fd, &page_entry, sizeof(uint64_t)) == sizeof(uint64_t)) {
                                if (page_entry & (1ULL << 63)) {
                                    // Page is present in memory
                                    unsigned long pfn = page_entry & ((1ULL << 55) - 1);
                                    printf("Page %lu is in memory. PFN: %lu\n", virtual_page_number, pfn);
                                } else {
                                    // Page is not in memory
                                    printf("Page %lu is not in memory.\n", virtual_page_number);
                                }
                            }
                        }
                    }
                    else{
                        printf("Page %lu is not in the range.\n", virtual_page_number);
                    }
                    close(pagemap_fd);
                }
                
            }
        }
        fclose(maps_fp);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <command> <arguments>\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[2]);

    if (strcmp(argv[1], "-frameinfo") == 0) { //2. option
        if (argc < 4) {
            printf("Usage: %s -frameinfo <PFN>\n", argv[0]);
            return 1;
        }
        unsigned long pfn = strtoul(argv[3], NULL, 0);
        pvm_frameinfo(pid, pfn);
    } else if (strcmp(argv[1], "-mapall") == 0) { //7. option
        pvm_mapallin(pid);
    } else if (strcmp(argv[1], "-mapallin") == 0) { //8. option
        pvm_mapall(pid);
    } else if (strcmp(argv[1], "-alltablesize") == 0) { //9. option
        pvm_alltablesize(pid);
    } else if (strcmp(argv[1], "-memused") == 0) { //3. option
        pvm_memory_usage(pid);
    } else if (strcmp(argv[1], "-mapva") == 0){ //4. option
        pvm_virtual_to_physical(pid, strtoul(argv[3], NULL, 0));
    } else if (strcmp(argv[1], "-pte") == 0){ //5. option
        pvm_pageinfo(pid, strtoul(argv[3], NULL, 0));
    } else if (strcmp(argv[1], "-maprange") == 0){
        pvm_maprange(pid, strtoul(argv[3], NULL, 0), strtoul(argv[4], NULL, 0));
    } else {
        printf("Invalid command\n");
        return 1;
    }

    return 0;
}
