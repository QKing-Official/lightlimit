#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <limits.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <ncurses.h>
#include <time.h>
#include <signal.h>

#define CGROUP_BASE "/sys/fs/cgroup/cpu/lightlimit"
#define MAX_PROCESSES 500
#define REFRESH_DELAY 500000 // 500ms

typedef struct {
    pid_t pid;
    char command[256];
    double cpu_percent;
    double mem_percent;
    unsigned long vsize;
} process_info_t;

void print_help(const char *program_name);
int get_cpu_count(void);
void create_cgroup(void);
void remove_cgroup(void);
void set_cpu_limit_total(int cpu_percentage);
void set_cpu_preference(const char *core_list);
void reset_cpu_limit(void);
void print_cpu_info(void);
void monitor(void);

// Utility function to check if running as root
int is_root(void) {
    return (geteuid() == 0);
}

// Verify permissions and show warning if not root
void check_permissions(void) {
    if (!is_root()) {
        fprintf(stderr, "Warning: Not running as root. Some features may not work.\n");
    }
}

int get_cpu_count(void) {
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores == -1) {
        perror("Failed to get CPU core count");
        return 1;
    }
    return (int)cores;
}

void create_cgroup(void) {
    struct stat st = {0};
    
    // Check if directory exists
    if (stat(CGROUP_BASE, &st) == -1) {
        char command[256];
        snprintf(command, sizeof(command), "mkdir -p %s", CGROUP_BASE);
        if (system(command) != 0) {
            perror("Failed to create cgroup directory");
            exit(EXIT_FAILURE);
        }
    }
}

void remove_cgroup(void) {
    struct stat st = {0};
    
    // Check if directory exists before removing
    if (stat(CGROUP_BASE, &st) != -1) {
        char command[256];
        snprintf(command, sizeof(command), "rmdir %s", CGROUP_BASE);
        if (system(command) != 0) {
            perror("Failed to remove cgroup directory");
        }
    }
}

void set_cpu_limit_total(int cpu_percentage) {
    int period_fd, quota_fd;
    long long quota;
    char period_str[32], quota_str[32];

    check_permissions();
    create_cgroup();

    period_fd = open(CGROUP_BASE "/cpu.cfs_period_us", O_WRONLY);
    quota_fd = open(CGROUP_BASE "/cpu.cfs_quota_us", O_WRONLY);

    if (period_fd < 0 || quota_fd < 0) {
        perror("Failed to open cgroup files");
        exit(EXIT_FAILURE);
    }

    int period = 100000;
    int cores = get_cpu_count();
    quota = (long long)period * cpu_percentage * cores / 100;

    snprintf(period_str, sizeof(period_str), "%d", period);
    snprintf(quota_str, sizeof(quota_str), "%lld", quota);

    if (write(period_fd, period_str, strlen(period_str)) < 0 ||
        write(quota_fd, quota_str, strlen(quota_str)) < 0) {
        perror("Failed to write to cgroup files");
        exit(EXIT_FAILURE);
    }

    close(period_fd);
    close(quota_fd);

    printf("Total CPU limit set to %d%% across %d cores/threads.\n", cpu_percentage, cores);
}

void set_cpu_preference(const char *core_list) {
    cpu_set_t mask;
    CPU_ZERO(&mask);

    char *token, *str, *tofree;
    tofree = str = strdup(core_list);
    
    if (!str) {
        perror("Memory allocation failed");
        return;
    }

    while ((token = strsep(&str, ",")) != NULL) {
        int core = atoi(token);
        if (core >= 0 && core < CPU_SETSIZE) {
            CPU_SET(core, &mask);
        }
    }

    free(tofree);

    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        perror("Failed to set CPU affinity");
    } else {
        printf("CPU affinity set to cores: %s\n", core_list);
    }
}

void reset_cpu_limit(void) {
    check_permissions();
    remove_cgroup();
    create_cgroup();
    printf("CPU limit reset.\n");
}

void print_cpu_info(void) {
    int cores = get_cpu_count();
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        perror("Failed to get system information");
        return;
    }

    printf("CPU Cores: %d\n", cores);
    printf("Total RAM: %ld MB\n", info.totalram / (1024 * 1024));
    printf("Free RAM: %ld MB\n", info.freeram / (1024 * 1024));
    printf("Uptime: %ld days, %ld hours, %ld minutes\n", 
           info.uptime / 86400, (info.uptime % 86400) / 3600, 
           (info.uptime % 3600) / 60);
}

// Compare function for qsort - sort by CPU usage descending
int compare_cpu_usage(const void *a, const void *b) {
    process_info_t *p1 = (process_info_t *)a;
    process_info_t *p2 = (process_info_t *)b;
    
    if (p1->cpu_percent > p2->cpu_percent) return -1;
    if (p1->cpu_percent < p2->cpu_percent) return 1;
    return 0;
}

void get_system_stats(char *cpu_info, char *mem_info, int max_len) {
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        snprintf(cpu_info, max_len, "CPU: N/A");
        snprintf(mem_info, max_len, "Mem: N/A");
        return;
    }
    
    // Get CPU load
    double load = info.loads[0] / (double)(1 << SI_LOAD_SHIFT);
    int cores = get_cpu_count();
    double cpu_load_percent = (load / cores) * 100;
    
    // Memory usage
    unsigned long total_mem = info.totalram / (1024 * 1024);
    unsigned long used_mem = (info.totalram - info.freeram) / (1024 * 1024);
    double mem_percent = ((double)used_mem / total_mem) * 100;
    
    snprintf(cpu_info, max_len, "CPU: %.1f%%", cpu_load_percent);
    snprintf(mem_info, max_len, "Mem: %lu/%lu MB (%.1f%%)", 
             used_mem, total_mem, mem_percent);
}

void get_process_list(process_info_t *processes, int *count, int max_count) {
    DIR *proc_dir;
    struct dirent *entry;
    struct sysinfo sysinfo_data;
    unsigned long long total_time, seconds_since_boot;
    
    if (sysinfo(&sysinfo_data) != 0) {
        return;
    }
    
    seconds_since_boot = sysinfo_data.uptime;
    *count = 0;
    
    proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        return;
    }
    
    while ((entry = readdir(proc_dir)) != NULL && *count < max_count) {
        if (isdigit(entry->d_name[0])) {
            pid_t pid = atoi(entry->d_name);
            char stat_path[256], cmd_path[256], cmd_buf[256];
            
            // Get command name
            snprintf(cmd_path, sizeof(cmd_path), "/proc/%d/comm", pid);
            FILE *cmd_file = fopen(cmd_path, "r");
            if (cmd_file) {
                if (fgets(cmd_buf, sizeof(cmd_buf), cmd_file)) {
                    // Remove newline if present
                    size_t len = strlen(cmd_buf);
                    if (len > 0 && cmd_buf[len-1] == '\n') {
                        cmd_buf[len-1] = '\0';
                    }
                }
                fclose(cmd_file);
            } else {
                strcpy(cmd_buf, "unknown");
            }
            
            // Get process stats
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
            FILE *stat_file = fopen(stat_path, "r");
            
            if (stat_file) {
                // Format is complex and might vary, so we'll parse carefully
                char state;
                unsigned long utime, stime, vsize;
                unsigned long long starttime;
                
                // We need to read the command carefully as it can contain spaces and parentheses
                char stat_line[1024], *cmd_start, *cmd_end;
                if (fgets(stat_line, sizeof(stat_line), stat_file)) {
                    cmd_start = strchr(stat_line, '(');
                    cmd_end = strrchr(stat_line, ')');
                    
                    if (cmd_start && cmd_end && cmd_end > cmd_start) {
                        // Now we can safely use sscanf on the parts before and after the command
                        int items_before = sscanf(stat_line, "%*d", &pid);
                        
                        // Skip command part and parse the rest
                        int items_after = sscanf(cmd_end + 2, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %llu %lu",
                                            &state, &utime, &stime, &starttime, &vsize);
                        
                        if (items_after == 5) {
                            // Calculate CPU usage
                            total_time = utime + stime;
                            
                            // Convert jiffies to seconds
                            double seconds = (double)total_time / sysconf(_SC_CLK_TCK);
                            
                            // Calculate seconds the process has been running
                            double process_uptime = seconds_since_boot - ((double)starttime / sysconf(_SC_CLK_TCK));
                            
                            // Calculate CPU percentage
                            double cpu_usage = 0;
                            if (process_uptime > 0) {
                                cpu_usage = 100 * (seconds / process_uptime);
                            }
                            
                            // Calculate memory percentage
                            double mem_percent = ((double)vsize / (sysinfo_data.totalram)) * 100.0;
                            
                            // Save process info
                            processes[*count].pid = pid;
                            strncpy(processes[*count].command, cmd_buf, sizeof(processes[*count].command) - 1);
                            processes[*count].command[sizeof(processes[*count].command) - 1] = '\0';
                            processes[*count].cpu_percent = cpu_usage;
                            processes[*count].mem_percent = mem_percent;
                            processes[*count].vsize = vsize / (1024 * 1024); // Convert to MB
                            
                            (*count)++;
                        }
                    }
                }
                fclose(stat_file);
            }
        }
    }
    
    closedir(proc_dir);
    
    // Sort processes by CPU usage
    qsort(processes, *count, sizeof(process_info_t), compare_cpu_usage);
}

void kill_process(pid_t pid) {
    if (kill(pid, SIGTERM) != 0) {
        // Try SIGKILL if SIGTERM fails
        if (kill(pid, SIGKILL) != 0) {
            mvprintw(0, 0, "Failed to kill process %d: %s", pid, strerror(errno));
        }
    }
}

void monitor(void) {
    process_info_t processes[MAX_PROCESSES];
    int process_count = 0;
    int selected_row = 0;
    int ch;
    
    // Initialize ncurses
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE); // Make getch blocking for better performance
    curs_set(0); // Hide cursor
    
    // Define color pairs
    init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Headers
    init_pair(2, COLOR_CYAN, COLOR_BLACK);    // Normal rows
    init_pair(3, COLOR_BLACK, COLOR_CYAN);    // Selected row
    init_pair(4, COLOR_RED, COLOR_BLACK);     // High CPU/MEM
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);  // Medium CPU/MEM
    init_pair(6, COLOR_WHITE, COLOR_BLUE);    // Status bar
    
    timeout(0); // Non-blocking getch for refresh

    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        
        // Get process list and system stats
        get_process_list(processes, &process_count, MAX_PROCESSES);
        
        char cpu_info[64], mem_info[128];
        get_system_stats(cpu_info, mem_info, sizeof(cpu_info));
        
        clear();
        
        // Draw status bar at the top
        attron(COLOR_PAIR(6));
        for (int i = 0; i < cols; i++) {
            mvaddch(0, i, ' ');
        }
        mvprintw(0, 1, "LightLimit Monitor | %s | %s | Press q:Quit k:Kill r:Refresh", 
                 cpu_info, mem_info);
        attroff(COLOR_PAIR(6));
        
        // Display header
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(1, 0, "  PID  ");
        mvprintw(1, 8, "CPU%%  ");
        mvprintw(1, 15, "MEM%%  ");
        mvprintw(1, 22, "MEM(MB)  ");
        mvprintw(1, 32, "COMMAND");
        attroff(COLOR_PAIR(1) | A_BOLD);
        
        // Draw separator line
        attron(A_BOLD);
        for (int i = 0; i < cols; i++) {
            mvaddch(2, i, '-');
        }
        attroff(A_BOLD);
        
        // Display processes
        int display_rows = rows - 4; // Adjust for header, status bar and bottom info
        int start_idx = (selected_row / display_rows) * display_rows;
        
        for (int i = 0; i < display_rows && (i + start_idx) < process_count; i++) {
            int row = i + 3; // Start after header
            int idx = i + start_idx;
            
            // Select color based on CPU/MEM usage
            int color_pair = 2; // Default
            
            if (processes[idx].cpu_percent > 50.0 || processes[idx].mem_percent > 50.0) {
                color_pair = 4; // High usage
            } else if (processes[idx].cpu_percent > 20.0 || processes[idx].mem_percent > 20.0) {
                color_pair = 5; // Medium usage
            }
            
            // Highlight selected row
            if (idx == selected_row) {
                color_pair = 3;
                attron(COLOR_PAIR(color_pair) | A_BOLD);
            } else {
                attron(COLOR_PAIR(color_pair));
            }
            
            mvprintw(row, 0, "%5d  ", processes[idx].pid);
            mvprintw(row, 8, "%5.1f  ", processes[idx].cpu_percent);
            mvprintw(row, 15, "%5.1f  ", processes[idx].mem_percent);
            mvprintw(row, 22, "%7lu  ", processes[idx].vsize);
            mvprintw(row, 32, "%.50s", processes[idx].command);
            
            if (idx == selected_row) {
                attroff(COLOR_PAIR(color_pair) | A_BOLD);
            } else {
                attroff(COLOR_PAIR(color_pair));
            }
        }
        
        // Bottom status line
        attron(COLOR_PAIR(6));
        for (int i = 0; i < cols; i++) {
            mvaddch(rows - 1, i, ' ');
        }
        mvprintw(rows - 1, 1, "Processes: %d | Selected: %d (%s)", 
                 process_count, 
                 selected_row < process_count ? processes[selected_row].pid : 0,
                 selected_row < process_count ? processes[selected_row].command : "none");
        attroff(COLOR_PAIR(6));
        
        refresh();
        
        // Process keyboard input with a timeout to refresh stats
        timeout(REFRESH_DELAY / 1000); // Convert microseconds to milliseconds
        ch = getch();
        
        if (ch == 'q' || ch == 'Q') {
            break;
        } else if (ch == 'k' || ch == 'K') {
            if (selected_row < process_count) {
                attron(COLOR_PAIR(4) | A_BOLD);
                mvprintw(rows - 3, 1, "Kill process %d (%s)? (y/n)", 
                         processes[selected_row].pid, processes[selected_row].command);
                attroff(COLOR_PAIR(4) | A_BOLD);
                refresh();
                
                timeout(-1); // Wait for response
                ch = getch();
                if (ch == 'y' || ch == 'Y') {
                    kill_process(processes[selected_row].pid);
                }
                timeout(0); // Back to non-blocking
            }
        } else if (ch == KEY_UP) {
            if (selected_row > 0) {
                selected_row--;
            }
        } else if (ch == KEY_DOWN) {
            if (selected_row < process_count - 1) {
                selected_row++;
            }
        } else if (ch == KEY_HOME) {
            selected_row = 0;
        } else if (ch == KEY_END) {
            selected_row = process_count - 1;
        } else if (ch == KEY_PPAGE) {
            selected_row -= display_rows;
            if (selected_row < 0) selected_row = 0;
        } else if (ch == KEY_NPAGE) {
            selected_row += display_rows;
            if (selected_row >= process_count) selected_row = process_count - 1;
        } else if (ch == 'r' || ch == 'R') {
            // Just refresh - will happen on next loop
        }
    }
    
    // Clean up
    endwin();
}

void print_help(const char *program_name) {
    printf("Usage: %s COMMAND [ARGS]\n\n", program_name);
    printf("Commands:\n");
    printf("  total <cpu_percentage>   Sets total CPU limit for all processes (0-100%%)\n");
    printf("  preference <core_list>   Sets CPU affinity (e.g., '0,1,3')\n");
    printf("  reset                    Resets CPU limits and cgroup\n");
    printf("  info                     Displays CPU and memory info\n");
    printf("  monitor                  Interactive process monitor with task management\n");
    printf("  uninstall                Removes the cgroup that lightlimit creates\n");
    printf("  help                     Displays this help message\n\n");
    printf("LightLimit: Advanced CPU management tool\n");
    printf("  - Most commands require root privileges\n");
    printf("  - In monitor mode, use arrow keys to navigate, 'k' to kill, 'q' to quit\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "help") == 0) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    } else if (strcmp(argv[1], "uninstall") == 0) {
        check_permissions();
        remove_cgroup();
        printf("LightLimit cgroup removed successfully.\n");
        return EXIT_SUCCESS;
    } else if (strcmp(argv[1], "reset") == 0) {
        reset_cpu_limit();
        return EXIT_SUCCESS;
    } else if (strcmp(argv[1], "info") == 0) {
        print_cpu_info();
        return EXIT_SUCCESS;
    } else if (strcmp(argv[1], "monitor") == 0 || strcmp(argv[1], "htop") == 0) {
        monitor();
        return EXIT_SUCCESS;
    } else if (strcmp(argv[1], "total") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Missing CPU percentage value.\n");
            return EXIT_FAILURE;
        }
        
        int cpu_percentage = atoi(argv[2]);
        if (cpu_percentage < 0 || cpu_percentage > 100) {
            fprintf(stderr, "Error: CPU percentage must be between 0 and 100.\n");
            return EXIT_FAILURE;
        }
        set_cpu_limit_total(cpu_percentage);
        return EXIT_SUCCESS;
    } else if (strcmp(argv[1], "preference") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Missing core list.\n");
            return EXIT_FAILURE;
        }
        set_cpu_preference(argv[2]);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Invalid command: %s\n", argv[1]);
    fprintf(stderr, "Run '%s help' for usage information.\n", argv[0]);
    return EXIT_FAILURE;
}
