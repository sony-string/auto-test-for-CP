#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <getopt.h>

#include <string>
#include <iostream>
#include <fstream>
#include <format>
using namespace std;


extern int fileno(FILE*);


#ifdef DEBUG
static void dlog(const char* format, ...) {
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}
#else
/**
 * @brief A wrapper of `printf()`, when Macro `DEBUG` is defined. If not defined, implementation is `void`;
 * @arg format 
 * @arg ... vargs
 * @return void
 */
static void dlog(const char* format, ...) {
	return;
}
#endif


/** current file descriptor index of standard output */
static int fd_stdout = 1;
/**
 * @brief Redirect from file descriptor of stdout forward `to`. This can be reverted by `redirect_stdout(stdout)`.
 * @arg to Destination of redirection.
 * @return void
 */
static void redirect_stdout(FILE* to) {
    if (to == stdout) {
        dup2(fd_stdout, fileno(to));
        close(fd_stdout);
        fd_stdout = fileno(to);
        return;        
    }
    int dup_stdout = dup(fd_stdout);
    dup2(fileno(to), fd_stdout);
    fd_stdout = dup_stdout;    
}


/** current file descriptor index of standard input */
static int fd_stdin = 0;
/**
 * @brief Redirect from file descriptor of stdin forward `to`. This can be reverted by `redirect_stdin(stdin)`.
 * @arg to Destination of redirection.
 * @return void
 */
static void redirect_stdin(FILE* to) {
    if (to == stdin) {
        dup2(fd_stdin, fileno(to));
        close(fd_stdin);
        fd_stdin = fileno(to);
        return;        
    }
    int dup_stdin = dup(fd_stdin);
    dup2(fileno(to), fd_stdin);
    fd_stdin = dup_stdin;    
}


constexpr static int FILE_NAME_LENGTH = 64;

static int     TC = 1;

static int    no_time = 1;
static int    no_correctness = 0;
static int    no_generator = 1;
static int    no_output_file = 1;
static int    has_target = 0;
static int    has_solver = 0;
static int    strict_judge = 0;
static int    help_opt_flag = 0;

static char    target_program[FILE_NAME_LENGTH] = {'.', '/', 'A', '\0'};
static char    generator[FILE_NAME_LENGTH] = {'.', '/', 'G', '\0'};
static char    solver[FILE_NAME_LENGTH] = {'.', '/', 'N', '\0'};
static char    output_file_name[FILE_NAME_LENGTH];


/**
 * @brief wait all child processes are done
 * @return void
 */
void wait_all_child_processes() {
    dlog("atexit\n");
    do {
        int status;
        wait(&status);        
    } while(errno != ECHILD);
}



int main(int argc, char* argv[]) {
    atexit(wait_all_child_processes);
    int pass_cnt = 0;
    int skip_cnt = 0;

    // get option
    {
        int option_index;

        static struct option long_options[] =
        {
            {"generator",       required_argument,      &no_generator, 0},
            {"target",          required_argument,      &has_target, 1},
            {"solver",          required_argument,      &has_solver, 1},
            {"strict_judge",    no_argument,            &strict_judge, 1},

            {"help",            no_argument,            0, 'h'},
            {"nr_tc",           required_argument,      0, 'n'},
            {"time",            no_argument,            0, 't'}, // no_time
            {"run_only",        no_argument,            0, 'r'}, // no_correctness
            {"output",          required_argument,      0, 'o'}, // no_output_file
            {0, 0, 0, 0}
        };
        int c;
        while ((c = getopt_long_only(argc, argv, "n:trho:",
                long_options, &option_index)) != -1) {

            switch (c) {
            case 0:
                if (strcmp(long_options[option_index].name, "generator") == 0) {
                    strncpy(generator, optarg, FILE_NAME_LENGTH - 1);
                } else if (strcmp(long_options[option_index].name, "target") == 0) {
                    strncpy(target_program, optarg, FILE_NAME_LENGTH - 1);
                } else if (strcmp(long_options[option_index].name, "solver") == 0) {
                    strncpy(solver, optarg, FILE_NAME_LENGTH - 1);
                }

                dlog("option %s", long_options[option_index].name);
                if (optarg)
                    dlog(" with arg %s", optarg);
                dlog("\n");
                break;

            case 'n':
                dlog("option -n\n");
                if (!(TC = atoi(optarg))) {
                    perror("--nr_tc / -n argument requires an integer as argument : TC is set by 1");
                }
                break;
            case 't':
                dlog("option -t\n");
                no_time = 0;
                break;
            case 'r':
                dlog("option -r\n");
                no_correctness = 1;
                break;
            case 'o':
                dlog("option -o with value `%s'\n", optarg);
                no_output_file = 0;
                strncpy(output_file_name, optarg, FILE_NAME_LENGTH - 1);
                break;
            case 'h':
                dlog("option -h\n");
                help_opt_flag = 1;
                break;

            case '?':
                /* getopt_long already printed an error message. */                
                break;
            default:
                abort ();
            }
        }
    }

    // if there is no target program, print help description
    if (!has_target || help_opt_flag) {
        printf("Usage: autotest --target=<FILE> [OPTION]...\n\n");
        printf("\t  --target=FILE\t\tFILE is target program to autotest\n");
        printf("\t  --generator=FILE\treplace .in with output from FILE\n");
        printf("\t  --solver=FILE\t\treplace .ans with output from FILE\n");
        printf("\t  --strict_judge\t(has NO EFFECT now)check whitespace(and \\t \\n) strictly\n");

        printf("\t-n --nr_tc=NUM\t\tNUM of test cases\n");
        printf("\t-t --time\t\tprint the time for target program to execute\n");
        printf("\t-r --run_only\t\tdo not run checker (do not check pass/fail\n");
        printf("\t-o --output=FILE\tredirect output to FILE (recommended than do this with '>' or '>>')\n");

        printf("\t-h --help\t\tdisplay this help and exit\n");

        exit(0);
    }

    FILE* output_file_fp;
    if (!no_output_file) output_file_fp = freopen(output_file_name, "w", stdout);

    for (int _TC = 1; _TC <= TC; _TC++) {        
        printf(":: TC %d :: \n", _TC);

        std::string in_file_name = std::format("./tc/{}.in", _TC);
        std::string out_file_name = std::format("./tc/{}.out", _TC);
        std::string ans_file_name = std::format("./tc/{}.ans", _TC);

        FILE* in_file_fp, * out_file_fp, * ans_file_fp;

        char* const argv_target[] = {target_program, NULL};
        struct timeval stime;
        struct timeval etime;
        int submitted;

        pid_t pid;

        if (!no_generator) {
            in_file_fp = fopen(in_file_name.c_str(), "w");
            if (!in_file_fp) {
                perror(in_file_name.c_str());
                goto skip;
            }
            redirect_stdout(in_file_fp);
            fclose(in_file_fp);

            char* const argv_gen[] = {generator, NULL};
            posix_spawn(&pid, generator, NULL, NULL, argv_gen, NULL);
            int gen;
            wait(&gen);
            
            redirect_stdout(stdout);
        }
        

        in_file_fp = fopen(in_file_name.c_str(), "r");
        if (!in_file_fp) {
            perror(in_file_name.c_str());
            goto skip;
        }
        redirect_stdin(in_file_fp);
        fclose(in_file_fp);

        out_file_fp = fopen(out_file_name.c_str(), "w");
        if (!out_file_fp) {
            perror(out_file_name.c_str());
            goto skip;
        }
        redirect_stdout(out_file_fp);
        fclose(out_file_fp);
        
        posix_spawn(&pid, target_program, NULL, NULL, argv_target, NULL);
        gettimeofday(&stime, NULL); // todo Program received signal SIGTTIN, Stopped (tty input).
        wait(&submitted);
        gettimeofday(&etime, NULL);

        redirect_stdin(stdin);
        redirect_stdout(stdout);

        if (!no_time) {
            if (etime.tv_usec - stime.tv_usec < 0) {
                etime.tv_usec += 1'000'000;
                etime.tv_sec -= 1;
            }            
            printf("run time : %ld.%06ld\n", etime.tv_sec - stime.tv_sec , etime.tv_usec - stime.tv_usec);
        }


        if (!no_correctness) {
            if (has_solver) {
                char* const argv_solver[] = {solver, NULL};

                in_file_fp = fopen(in_file_name.c_str(), "r");
                if (!in_file_fp) {
                    perror(in_file_name.c_str());
                    goto skip;
                }
                redirect_stdin(in_file_fp);
                fclose(in_file_fp);

                ans_file_fp = fopen(ans_file_name.c_str(), "w");
                if (!ans_file_fp) {
                    perror(ans_file_name.c_str());
                    goto skip;
                }
                redirect_stdout(ans_file_fp);
                fclose(ans_file_fp);

                posix_spawn(&pid, solver, NULL, NULL, argv_solver, NULL);
                int naive;
                wait(&naive);

                redirect_stdin(stdin);
                redirect_stdout(stdout);
            }
            
            // checker
            std::ifstream out_file_stream(out_file_name.c_str());
            if (!out_file_stream.is_open()) {
                perror(out_file_name.c_str());
                goto skip;
            }
            std::ifstream ans_file_stream(ans_file_name.c_str());
            if (!ans_file_stream.is_open()) {
                perror(ans_file_name.c_str());
                goto skip;
            }
            
            bool passed = true;
            std::string unexpected_and_expected;

            std::string out_read, ans_read;
            while (true) {
                bool is_out_eof = out_file_stream.eof(), is_ans_eof = ans_file_stream.eof();
                out_read.clear(); ans_read.clear();

                if (!is_out_eof) out_file_stream >> std::ws >> out_read;  
                if (!is_ans_eof) ans_file_stream >> std::ws >> ans_read;
                
                if (is_out_eof && is_ans_eof) break;
                bool cmp_result = (out_read == ans_read);                
                if (!cmp_result && passed) {
                    passed = cmp_result;                    
                    unexpected_and_expected = std::format("Expected : \"{}\" , Output \"{}\"", ans_read, out_read);
                    break;
                }
            }
            out_file_stream.close();
            ans_file_stream.close();

            if (!passed) {
                if (no_output_file)
                    printf("\033[0;31mFAILED\033[0m\t");
                else 
                    printf("FAILED\t");
                printf("%s\n", unexpected_and_expected.c_str());        
            } else {
                if (no_output_file) 
                    printf("\033[0;32mPASSED\033[0m\n");
                else 
                    printf("PASSED\n");
                pass_cnt++;
            }            
        }
        fflush(stdout);
        continue;
skip:
        skip_cnt++;
        if (no_output_file)
            printf("\033[30;1mSKIPPED\033[0m\n");
        else 
            printf("SKIPPED\n");
        fflush(stdout);
    }

    if (!no_correctness) {
        if (no_output_file)
            printf("\033[0;32mPASSED\033[0m %d  \033[0;31mFAILED\033[0m %d  \033[30;1mSKIPPED\033[0m %d  \033[0;36mTOTAL\033[0m %d\nACCURACY %.1f\n",
                pass_cnt, TC - pass_cnt, skip_cnt, TC, (float)pass_cnt / TC * 100);
        else
            printf("PASSED %d  FAILED %d  SKIPPED %d  TOTAL %d\nACCURACY %.1f\n",
                pass_cnt, TC - pass_cnt - skip_cnt, skip_cnt, TC,(float)pass_cnt / TC * 100);
    }

    if (!no_output_file) fclose(output_file_fp);
    return 0;
}