//
//  main.cpp
//  injectable
//
//  Created by BlueCocoa on 2016/10/11.
//  Copyright © 2016 BlueCocoa. All rights reserved.
//

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <mach-o/fat.h>

struct _cpu_type_names {
    cpu_type_t cputype;
    const char * cpu_name;
};

#pragma push_macro("CPU_TYPE_ARM64")
#define CPU_TYPE_ARM64 (CPU_TYPE_ARM | CPU_ARCH_ABI64)

static struct _cpu_type_names cpu_type_names[] = {
    { CPU_TYPE_I386, "i386" },
    { CPU_TYPE_X86_64, "x86_64" },
    { CPU_TYPE_ARM, "arm" },
    { CPU_TYPE_ARM64, "arm64" }
};

#pragma pop_macro("CPU_TYPE_ARM64")

static const char *cpu_type_name(cpu_type_t cpu_type);

void dump_segments(FILE *obj_file);
void injectable(FILE *obj_file, FILE * injected_file);
uint32_t read_magic(FILE *obj_file, int offset);
int is_magic_64(uint32_t magic);
int should_swap_bytes(uint32_t magic);
int is_fat(uint32_t magic);
void *load_bytes(FILE *obj_file, int offset, int size);
void dump_segment_commands(FILE *obj_file, FILE *injected_file, int offset, int is_swap, uint32_t ncmds);
void modify_mach_header(FILE *obj_file, FILE *injected_file, int offset, int is_64, int is_swap);
void modify_fat_header(FILE *obj_file, FILE *injected_file, int is_swap);
int cp(FILE *f1, FILE *f2);

int main(int argc, char *argv[]) {
    if (argc == 2) {
        const char * filename = argv[1];
        char * injected;
        asprintf(&injected, "%s.injectable", filename);
        
        FILE *obj_file = fopen(filename, "rb");
        if (obj_file) {
            FILE *injected_file = fopen(injected, "wb");
            cp(obj_file, injected_file);
            
            fseek(obj_file, 0, SEEK_SET);
            fseek(injected_file, 0, SEEK_SET);
            injectable(obj_file, injected_file);
            
            fclose(obj_file);
            fclose(injected_file);
            
            printf("Done!\nInjectable file at %s\n", injected);
            free(injected);
        } else {
            printf("Cannot open file at %s\n", argv[1]);
        }
    } else {
        printf("Usage: injectable [MachO file]\n");
    }
    
    return 0;
}

void injectable(FILE * obj_file, FILE * injected_file) {
    uint32_t magic = read_magic(obj_file, 0);
    int is_64 = is_magic_64(magic);
    int is_swap = should_swap_bytes(magic);
    int fat = is_fat(magic);
    if (fat) {
        modify_fat_header(obj_file, injected_file, is_swap);
    } else {
        modify_mach_header(obj_file, injected_file, 0, is_64, is_swap);
    }
}

uint32_t read_magic(FILE *obj_file, int offset) {
    uint32_t magic;
    fseek(obj_file, offset, SEEK_SET);
    fread(&magic, sizeof(uint32_t), 1, obj_file);
    return magic;
}

int is_magic_64(uint32_t magic) {
    return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

int should_swap_bytes(uint32_t magic) {
    return magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM;
}

int is_fat(uint32_t magic) {
    return magic == FAT_MAGIC || magic == FAT_CIGAM;
}

void *load_bytes(FILE *obj_file, int offset, int size) {
    void *buf = calloc(1, size);
    fseek(obj_file, offset, SEEK_SET);
    fread(buf, size, 1, obj_file);
    return buf;
}

void dump_section(FILE *obj_file, FILE *injected_file, int offset, int is_swap, int is_64, uint32_t nsects) {
    int section_offset = offset;
    bool __RESTRICT = false;
    for (int i = 0; i < nsects && !__RESTRICT; i++) {
        if (is_64) {
            int section_size = sizeof(struct section_64);
            struct section_64 * sec = (struct section_64 *)load_bytes(obj_file, section_offset, section_size);
            if (is_swap) {
                swap_section_64(sec, nsects, NX_LittleEndian);
            }
            
            if (!__RESTRICT && strcmp("__restrict", sec->sectname) == 0) {
                printf("    __restrict section found\n");
                printf("    Modifying __restrict section...\n");
                __RESTRICT = true;
                sec->segname[0] = '!';
                sec->sectname[0] = '!';
            }
            fseek(injected_file, section_offset, SEEK_SET);
            fwrite(sec, sizeof(struct section_64), 1, injected_file);
            
            free(sec);
            section_offset += section_size;
        } else {
            int section_size = sizeof(struct section);
            struct section * sec = (struct section *)load_bytes(obj_file, section_offset, section_size);
            if (is_swap) {
                swap_section(sec, nsects, NX_LittleEndian);
            }
            
            if (!__RESTRICT && strcmp("__restrict", sec->sectname) == 0) {
                printf("    __restrict section found\n");
                printf("    Modifying __restrict section...\n");
                __RESTRICT = true;
                sec->segname[0] = '!';
                sec->sectname[0] = '!';
            }
            fseek(injected_file, section_offset, SEEK_SET);
            fwrite(sec, sizeof(struct section), 1, injected_file);
            
            free(sec);
            section_offset += section_size;
        }
    }
}

void dump_segment_commands(FILE *obj_file, FILE *injected_file, int offset, int is_swap, uint32_t ncmds) {
    int actual_offset = offset;
    bool __RESTRICT = false;
    for (int  i = 0; i < ncmds && !__RESTRICT; i++) {
        struct load_command *cmd = (struct load_command *)load_bytes(obj_file, actual_offset, sizeof(struct load_command));
        if (is_swap) {
            swap_load_command(cmd, NX_LittleEndian);
        }
        
        if (cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *segment = (struct segment_command_64 *)load_bytes(obj_file, actual_offset, sizeof(struct segment_command_64));
            if (is_swap) {
                swap_segment_command_64(segment, NX_LittleEndian);
            }
            
            if (!__RESTRICT && strcmp("__RESTRICT", segment->segname) == 0) {
                printf("  __RESTRICT segment found\n");
                printf("  Modifying __RESTRICT segment...\n");
                __RESTRICT = true;
                segment->segname[0] = '!';
            }
            fseek(injected_file, actual_offset, SEEK_SET);
            fwrite(segment, sizeof(struct segment_command_64), 1, injected_file);
            
            dump_section(obj_file, injected_file, actual_offset + (int)(sizeof(struct segment_command_64)), is_swap, 1, segment->nsects);
            
            free(segment);
        } else if (cmd->cmd == LC_SEGMENT) {
            struct segment_command *segment = (struct segment_command *)load_bytes(obj_file, actual_offset, sizeof(struct segment_command));
            if (is_swap) {
                swap_segment_command(segment, NX_LittleEndian);
            }
            
            if (!__RESTRICT && strcmp("__RESTRICT", segment->segname) == 0) {
                printf("  __RESTRICT segment found\n");
                printf("  Modifying __RESTRICT segment...\n");
                __RESTRICT = true;
                segment->segname[0] = '!';
            }
            fseek(injected_file, actual_offset, SEEK_SET);
            fwrite(segment, sizeof(struct segment_command), 1, injected_file);
            
            dump_section(obj_file, injected_file, actual_offset + (int)(sizeof(struct segment_command)), is_swap, 0, segment->nsects);
            
            free(segment);
        }
        
        actual_offset += cmd->cmdsize;
        
        free(cmd);
    }
    
    if (!__RESTRICT) {
        printf("  No __RESTRICT section in current arch\n");
    }
}

void modify_mach_header(FILE *obj_file, FILE *injected_file, int offset, int is_64, int is_swap) {
    uint32_t ncmds;
    int load_commands_offset = offset;
    
    if (is_64) {
        int header_size = sizeof(struct mach_header_64);
        struct mach_header_64 *header = (struct mach_header_64 *)load_bytes(obj_file, offset, header_size);
        if (is_swap) {
            swap_mach_header_64(header, NX_LittleEndian);
        }
        ncmds = header->ncmds;
        load_commands_offset += header_size;
        
        printf("Found arch %s\n", cpu_type_name(header->cputype));
        
        if (header->flags & MH_PIE) {
            printf("  ASLR/PIE is enabled\n");
            printf("  Disabling ASLR/PIE ...\n");
            header->flags &= ~MH_PIE;
        } else {
            printf("  ASLR/PIE is disabled\n");
        }
        fseek(injected_file, offset, SEEK_SET);
        fwrite(header, header_size, 1, injected_file);
        
        free(header);
    } else {
        int header_size = sizeof(struct mach_header);
        struct mach_header *header = (struct mach_header *)load_bytes(obj_file, offset, header_size);
        if (is_swap) {
            swap_mach_header(header, NX_LittleEndian);
        }
        
        ncmds = header->ncmds;
        load_commands_offset += header_size;
        
        printf("Found arch %s\n", cpu_type_name(header->cputype));
        
        if (header->flags & MH_PIE) {
            printf("  ASLR/PIE is enabled\n");
            printf("  Disabling ASLR/PIE ...\n");
            header->flags &= ~MH_PIE;
        } else {
            printf("  ASLR/PIE is disabled\n");
        }
        fseek(injected_file, offset, SEEK_SET);
        fwrite(header, header_size, 1, injected_file);
        
        free(header);
    }
    
    dump_segment_commands(obj_file, injected_file, load_commands_offset, is_swap, ncmds);
}

void modify_fat_header(FILE *obj_file, FILE *injected_file, int is_swap) {
    int header_size = sizeof(struct fat_header);
    int arch_size = sizeof(struct fat_arch);
    
    struct fat_header *header = (struct fat_header *)load_bytes(obj_file, 0, header_size);
    if (is_swap) {
        swap_fat_header(header, NX_LittleEndian);
    }
    
    int arch_offset = header_size;
    for (int i = 0; i < header->nfat_arch; i++) {
        struct fat_arch *arch = (struct fat_arch *)load_bytes(obj_file, arch_offset, arch_size);
        
        
        if (is_swap) {
            swap_fat_arch(arch, 1, NX_LittleEndian);
        }
        
        int mach_header_offset = arch->offset;
        free(arch);
        arch_offset += arch_size;
        
        uint32_t magic = read_magic(obj_file, mach_header_offset);
        int is_64 = is_magic_64(magic);
        int is_swap_mach = should_swap_bytes(magic);
        
        modify_mach_header(obj_file, injected_file, mach_header_offset, is_64, is_swap_mach);
    }
    free(header); 
}

static const char *cpu_type_name(cpu_type_t cpu_type) {
    static int cpu_type_names_size = sizeof(cpu_type_names) / sizeof(struct _cpu_type_names);
    for (int i = 0; i < cpu_type_names_size; i++ ) {
        if (cpu_type == cpu_type_names[i].cputype) {
            return cpu_type_names[i].cpu_name;
        }
    }
    
    return "unknown";
}

int cp(FILE *f1, FILE *f2) {
    char buffer[BUFSIZ];
    size_t n;
    
    while ( (n = fread(buffer, sizeof(char), sizeof(buffer), f1)) > 0 ) {
        if (fwrite(buffer, sizeof(char), n, f2) != n) {
            return -1;
        }
    }
    return 0;
}
