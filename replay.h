/*
	For demonstration purposes,
	we're outputting an HTML+JS+CSS player.
	However, later down the line, the goal should be
	to create a texture and render frame-by-frame
	into a folder, then use something like ffmpeg
	to squish it into a GIF or other animation
*/

#include "__replay.html.h"
#define REPLAYER "session.htm"

// Interfaces

void			error				(int code);
int				file_read			(FILE *fp, char *buffer, int max);
int				file_write			(FILE *fp, char *buffer, int max);

bool replay (const char * stream)
{
	size_t len = strlen(stream);
	char *buffer = malloc(32 * 1024 * 1024);
	strcat(buffer, REPLAY_HTML);
	strcat(buffer, "<p id='input' class='input'>\n");
	char *end = (buffer + strlen(buffer));
	printf("%ld\n", (end - buffer));
	int i; for(i=0;i<len;i++) {
		char c = stream[i];
		if (c=='<') { strcat(buffer, "&lt;"); end += 4; }
		else if (c=='>') { strcat(buffer, "&gt;"); end += 4; }
		else { *end = c; end++; }
	}
	strcat(buffer, "\n</p>\n");
	FILE *fp = fopen(REPLAYER, "w+"); if (!fp) { error(ERR_REPLAYER); return false; }
	int done = file_write(fp, buffer, strlen(buffer));
	return true;
}
