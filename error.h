// Color Codes

#define RED			"\e[0;31m"
#define GREEN		"\e[0;32m"
#define BLUE		"\e[0;34m"
#define RESET		"\e[0;37m"

// Error Messages

enum {
	ERR_OK,
	ERR_PATH,
	ERR_FILE,
	ERR_CD,
	ERR_FORK,
	ERR_PIPE,
	ERR_POPEN,
	ERR_MODE,
	ERR_RECORD,
	ERR_SAVE,
	ERR_REPLAYER,
	ERR_OOB
};

const char * OK_RECORD = GREEN "[OK]  " RED "NOW RECORDING SESSION" RESET "\n";
const char * OK_SAVE = GREEN "[OK]  " RESET "SAVING SESSION TO FILE\n";
const char ERRORS [ERR_OOB][64] = {
	[ERR_PATH] = 	"$PATH not found in environment variables",
	[ERR_FILE] = 	"File not found",
	[ERR_CD] = 		"Invalid change directory path",
	[ERR_FORK] =	"Unable to fork process",
	[ERR_PIPE] =	"Error creating pipe",
	[ERR_POPEN] =	"Error spawning child process",
	[ERR_MODE] =	"Already in mode",
	[ERR_RECORD] =	"Cannot begin session recording",
	[ERR_SAVE] =	"Error when saving session to file",
	[ERR_REPLAYER]=	"Error when saving session replayer",
};
