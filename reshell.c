// https://github.com/nir9/welcome/blob/master/lnx/very-minimal-shell/shell.c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h> // ls
#include "error.h"
#include "replay.h"
#include "__LICENSE.h"

#define SHELL GREEN "%s" RESET ":" BLUE "%s" RESET " $ "

#define PROGRAM		"reshell"
#define COPYRIGHT 	"(C) 2026 Blood Rose Records"
#define SOURCE 		"<https://bloodrose.org>"
#define VERSION_STR	"v" STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_PATCH)
	#define VERSION_MAJOR 0
	#define VERSION_MINOR 0
	#define VERSION_PATCH 0
	#define STR(x) STRX(x)
	#define STRX(x) #x

#define STDIN_MAX	( 16 * 1024 )
#define HEAP_INIT	( 4 * 1024 )
#define BWARGS_MAX	16
#define PATHS_MAX	32
#define PATHS_LEN	256
#define OUTPUT		"session.txt"

#define STDIN		0
#define STDOUT		1
#define STDERR		2

// Shared Variables

char *			bwargs				[BWARGS_MAX];
char			paths				[PATHS_MAX][PATHS_LEN] = { 0 };

const char *	cmd_cd = 			"cd";
const char *	cmd_cwd =			"cwd";
const char *	cmd_clear = 		"clear";
const char *	cmd_exit = 			"exit";
const char *	cmd_ls = 			"ls";
const char *	cmd_pid = 			"pid";
const char *	cmd_printenv =		"printenv";
const char *	cmd_help =			"help";
const char *	cmd_license =		"license";
const char *	cmd_version =		"version";
const char *	record_start =		"record";
const char *	record_stop =		"save";

const char *	HELP = "" \
	PROGRAM " " VERSION_STR "\n" \
	"Usage: " PROGRAM " [command]\n" \
	"    " PROGRAM " is intended for recording commands and their output to a text file\n" \
	"    in a way that allows automated conversion to playable demos\n" \
	"Shell options:\n" \
	"    record     Begin recording session\n" \
	"    save       Save recorded session to text file\n" \
	"    pid        Print the working process ID of this shell\n" \
	"    cwd        Print the current working directory of this shell\n" \
	"    help       Display this help message\n" \
	"    version    Display version information\n" \
	"    license    Display the full license text\n";

// Interfaces

void			error				(int code);
int				file_read			(FILE *fp, char *buffer, int max);
int				file_write			(FILE *fp, char *buffer, int max);
bool			file_exists			(char *filename);
bool			env_pathfind		(char **environ);
size_t			env_pathcol			(char * stream);
void			env_print			(char **args, char **environ);
void			builtin_exit		(char * stream, int code, bool recording);
char **			builtin_binargs		(char * command);
char *			builtin_binpath		(char * binary, char * cwd);
void			builtin_cd			(char * command);
void			builtin_cwd			(char * cwd);
void 			builtin_clear		(void);
void			builtin_help		(void);
void			builtin_license		(void);
void			builtin_pid			(int pid);
bool			builtin_record		(bool state, char *buffer, size_t used);
bool			builtin_save		(bool state, char *buffer, size_t used);
char *			stream_write		(char *buffer, size_t *used, size_t *capacity, char *input, size_t max);
void			list_directory		(const char * path);

// Implementations

void			error				(int code)
{
	if (code >= ERR_OOB) { exit(-1); }
	printf(RED "%s" RESET "\n", ERRORS[code]);
}

int				file_read			(FILE *fp, char *buffer, int max)
{
    if ( fp == NULL ) { return false; }
    int cursor = 0; char c; while(cursor < max -1) {
      c = fgetc(fp);
      if( feof(fp) ) { break; }
      buffer[cursor] = c; cursor++;
   } fclose(fp); return cursor;
}

int				file_write			(FILE *fp, char *buffer, int max)
{
    if ( fp == NULL ) { return false; }
    int cursor; for (cursor = 0; cursor < max && buffer[cursor] != '\0'; cursor++) {
        if (fputc(buffer[cursor], fp) == EOF) { fclose(fp); return false; }
    } fclose(fp); return cursor;
}

bool			file_exists			(char *filename)
{ struct stat buffer; return (stat(filename, &buffer) == 0); }

bool			env_pathfind		(char **environ)
{
	int i; int path = -1;
	// First, find PATH variable
	for (i=0; environ[i]!=NULL; i++) {
		char * self = environ[i];
		int len = strlen(self);
		int cmp = strncmp(self, "PATH=", 5);
		if (len < 6) { continue; }
		if (cmp != 0) { continue; }
		path = i; break;
	}
	// If we found it, begin slicing it up
	if (path == -1) { return false; }
	int pindex = 0;
	// Cannot reliably stop scanning using delimiter, stop at actual boundary with next env
	char * cursor = (char *)( environ[path] + 5 );
	char * limit =	(char *)( environ[path+1] );
	while(true) {
		if (limit - cursor <= 0) { break; }
		char * dest = (char *)( &paths[pindex] );
		size_t capture = env_pathcol(cursor);
		if (capture == 0) { break; }
		strncpy(dest, cursor, capture);
		dest[capture] = 0;
		cursor += capture +1;
		capture = env_pathcol(cursor);
		pindex++;
	}
	return true;
}

size_t			env_pathcol			(char * stream)
{
	size_t i; for(i=0;i<512;i++) {
		char c = stream[i];
		switch(c) {
			default: continue;
			case ':': case ',': case '\r': case '\n': return i;
		}
	}	return i;
}

void			env_print			(char **args, char **environ)
{
    int i;
	printf("Path:\n");
	for (i=0; paths[i][0]!=0; i++) 		{ printf("   ... " GREEN "\"%s\"" RESET "\n", paths[i]); }
    printf("Process Arguments:\n");
    for (i=0; args[i]!=NULL; i++)		{ printf("   ... " GREEN "\"%s\"" RESET "\n", args[i]); }
    printf("Environment variables:\n");
    for (i=0; environ[i]!=NULL; i++)	{ printf("   ... " GREEN "\"%s\"" RESET "\n", environ[i]); }
}

char **			builtin_binargs		(char *command)
{
    int i; int count = 0; int len = strlen(command);
    for(i=0;i<len;i++) {
        if (command[i] == ' ') { command[i]='\0'; bwargs[count+1] = (void*)command+(i+1); count++; }
    }   bwargs[count+1] = NULL;
    bwargs[0] = (void*)command;
    return bwargs;
}

void			builtin_exit		(char * stream, int code, bool recording)
{
	if (recording == true) {
		printf("... Saving recording\n");
		FILE *out = fopen(OUTPUT, "w+"); if (!out) { exit(-1); }
		int done = file_write(out, stream, strlen(stream));
		replay(stream);
	}
	exit(code);
}

char *			builtin_binpath		(char *binary, char *cwd)
{
    int i; static char result[512] = { 0 };
    // first strip name at space or eol
    for (i=0;binary[i]!='\0';i++) {
        if (binary[i]==' ') { result[i] = '\0'; binary[i] = '\0'; break; }
		result[i] = binary[i];
    }

	// try the literal name first
    if ( file_exists(binary)) { return result; }
    // then iterate through PATH
	for(i=0; paths[i][0]!=0; i++) {
		char *local = paths[i];
		sprintf(result, "%s/%s", local, binary);
		if ( file_exists(result)) { return result; }
	}
    return NULL;
}

void 			builtin_cd			(char *command)
{
    char *dir = command + strlen(cmd_cd) + 1; // ignore the space
    if ( chdir(dir) != 0 ) { error(ERR_CD); }
	return;
}

void			builtin_cwd			(char *cwd)
{
	printf("%s\n", cwd);
}

void			builtin_clear		(void)
{
    int i; int j;
    for (i=0;i<80;i++) {
    for (j=0;j<8;j++)  {
        printf("    ");}
        printf("\n");}
    return;
}

void			builtin_license		(void)
{
	printf("%s %s\n%s\n",
		PROGRAM, VERSION_STR, LICENSE
	);
}

void			builtin_help		(void)
{
	printf("%s\n", HELP);
}

void			builtin_pid			(int pid)
{
	printf("%d\n", pid);
}

bool			builtin_record		(bool state, char *buffer, size_t used)
{
	// ERR_RECORD unused
	if ( state == true ) { error(ERR_MODE); }
	else { printf("%s\n", OK_RECORD); }
	return true;
}

bool			builtin_save		(bool state, char *buffer, size_t used)
{
	if ( state == false ) { error(ERR_MODE); return false; }
	FILE *fp = fopen(OUTPUT, "w+"); if (!fp) { error(ERR_SAVE); return false; }
	int done = file_write(fp, buffer, strlen(buffer));
	printf("%s", OK_SAVE);
	replay(buffer);
	return false;
}

char *			stream_write
(char *buffer, size_t *used, size_t *capacity, char *input, size_t max)
{
	int bufferlen = strlen(buffer);
	int len = strlen(input); if (len > max) { len = max; }
	size_t size = *capacity;
	while (len + *used >= size) {
		size *= 2;
	}
	if ( size != *capacity ) {
		*capacity = size;
		buffer = realloc(buffer, size);
	}
	char * destination = (char *)( buffer + bufferlen );
	strncpy(destination, input, len);
	*used = bufferlen + len;
	return buffer;
}

void			list_directory		(const char *path)
{
	// https://www.geeksforgeeks.org/c/c-program-list-files-sub-directories-directory/
    DIR *dir = opendir(path);
    if (!dir) { perror(path); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if ( strcmp(entry->d_name, ".")==0 ) { continue; }
        if ( strcmp(entry->d_name, "..")==0 ) { continue; }
        printf("%s\n", entry->d_name);
    }   closedir(dir);
    return;
}

int				main				(int argc, char **argv, char **environ)
{
    pid_t pid = getpid();
    printf("\e[=3h"); // set color 80x25 text mode
	char * output;
	char context[512];
	char command[512];
    char cwd[512];
	if (!env_pathfind(environ)) { error(ERR_PATH); }
	size_t used = 0;
	size_t capacity = HEAP_INIT;
	bool recording = false;
	output = malloc(HEAP_INIT);
	int wcmd = 1;
    while (1) {
        // Write the context info and get a command
        getcwd(cwd, sizeof(cwd));
        snprintf(context, sizeof(context), SHELL, getenv("USER"), cwd);
		write(1, context, strlen(context));
		// If not done executing ARGV, iteratively do so
		if ( wcmd < argc ) {
			int len = strlen(argv[wcmd]);
			strncpy(command, argv[wcmd], len);
			command[len] = 0;
			wcmd++;
		}
		// If done executing ARGV, get commands from user
		else {
			int count = read(0, command, sizeof(command)); if ( count <= 0 ) { continue; }
			command[count - 1] = 0; // /bin/ls\n -> /bin/ls\0
		}

		// Check for being and end recording *BEFORE* appending to recording stream
		if(strncmp(command, record_start, strlen(record_start))==0)	{ recording = builtin_record(recording, output, used); continue; }
		if(strncmp(command, record_stop, strlen(record_stop))==0)	{ recording = builtin_save(recording, output, used); continue; }

		// Append command to the working output file
		int len = strlen(command);
		if ( recording == true ) {
			char local[512];
			snprintf(local, sizeof(local), "$ %s\n", command);
			output = stream_write(output, &used, &capacity, local, strlen(local));
		}
        // Check for built-in commands like cd
        if(strncmp(command, cmd_exit, strlen(cmd_exit))==0) { builtin_exit(output, 0, recording); }
        if(strncmp(command, cmd_cd, strlen(cmd_cd))==0) { builtin_cd(command); continue; }
        if(strncmp(command, cmd_cwd, strlen(cmd_cwd))==0) { builtin_cwd(cwd); continue; }
		if(strncmp(command, cmd_clear, strlen(cmd_clear))==0) { builtin_clear(); continue; }
        if(strncmp(command, cmd_help, strlen(cmd_help))==0) { builtin_help(); continue; }
		if(strncmp(command, cmd_license, strlen(cmd_license))==0) { builtin_license(); continue; }
		if(strncmp(command, cmd_pid, strlen(cmd_pid))==0) { builtin_pid(pid); continue; }
        if(strncmp(command, cmd_printenv, strlen(cmd_printenv))==0) { env_print(argv, environ); continue; }
		if(strncmp(command, cmd_version, strlen(cmd_version))==0) { printf("%s %s\n", PROGRAM, VERSION_STR); continue; }
		// Fork the process and try to run the command
		int pipefiles[2]; // https://man7.org/linux/man-pages/man2/pipe.2.html
		if ( pipe(pipefiles) != 0 ) {
			error(ERR_PIPE); exit(1);
		}
		pid_t pid = fork();
		if (pid == 0) {
			// Redirect STDOUT, STDERR, close STDIN
			dup2(pipefiles[1], STDOUT_FILENO);
			dup2(pipefiles[1], STDERR_FILENO);
			close(pipefiles[0]);
            int code;
            char **args = builtin_binargs(command);
            char *path = builtin_binpath(command, cwd);
			if ( path == NULL ) { error(ERR_FILE); exit(1); }
            code = execve(path, args, environ); if ( code == -1 ) {
                free(path);
                printf(RED "%s: " RESET "%s\n", path, strerror(errno));
                exit(1);
            }
			break;
		} else if ( pid > 0 ) {
			// Capture the input stream as it is received
			close(pipefiles[1]);
			char inputbuffer[STDIN_MAX];
			ssize_t rbytes = 1;
			while ( rbytes > 0 ) {
				rbytes = read(pipefiles[0], inputbuffer, sizeof(inputbuffer));
				write(STDOUT_FILENO, inputbuffer, rbytes);
				if ( recording == true )
				{ output = stream_write(output, &used, &capacity, inputbuffer, rbytes); }
			}
			int status; waitpid(pid, &status, 0);
		} else {
            error(ERR_FORK);
			exit(1);
        }
	}
    builtin_exit(output, 0, recording);
}
