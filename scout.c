#include <sys/stat.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <pwd.h>
#include "utils.h"

/* enums */
enum {PREV, CURR, NEXT, LOAD, RELOAD};
enum {TOP, BOT, UP, DOWN, LEFT, RIGHT};
enum
{
	/* color pairings for file types */
	CP_DEFAULT = 1,
	CP_EXECUTABLE,
	CP_DIRECTORY,
	CP_ARCHIVE,
	CP_VIDEO,
	CP_AUDIO,
	CP_IMAGE,
	CP_SOCK,
	CP_FIFO,
	CP_CHR,
	CP_BLK,

	/* color pairings for everything else */
	CP_ERROR = 99,
	CP_TOPBARUSER,
	CP_TOPBARPATH,
	CP_TOPBARFILE,
	CP_BOTBARPERM,
	CP_BOTBARUSER,
	CP_BOTBARDATE,
	CP_BOTBARLINK,
};

/* structs */
typedef struct entr ENTR;
typedef struct node NODE;
typedef struct sdir SDIR;

struct entr
{
	int type;
	int isatu; /* is accessible to user */
	int issym; /* is symlink */
	char *name;
	char *size;
	char *perms;
	char *users;
	char *dates;
	char *lpath;
};

struct node
{
	struct node *prev;
	struct node *next;

	SDIR *buf;
	ENTR *file;
};

struct sdir
{
	NODE *sel;
	NODE *list;

	char *path;
	int selline;
	int entrycnt;
	NODE **entries;
	char *selected;
};

typedef struct scout
{
	SDIR *dir;
	int width;
	int height;
	int upperthrs;
	int lowerthrs;
	WINDOW *win;
} SCOUT;

/* function declarations */
static int scoutAddFile(ENTR **, char *);
static int scoutBuildWindows(void);
static int scoutDestroyWindows(void);
static int scoutCommandLine(char *);
static int scoutFreeDir(SDIR *);
static int scoutFreeInfo(ENTR *);
static int scoutGetFileInfo(ENTR *);
static int scoutGetFileSize(ENTR *);
static int scoutGetFileType(ENTR *);
static int scoutInitializeCurses(void);
static int scoutLoadDir(int, int);
static int scoutFreeDirSize(int);
static int scoutMove(int);
static int scoutPrintEntry(WINDOW *, ENTR *, int, int, int);
static int scoutPrintList(int);
static int scoutPrintListPrep(int);
static int scoutPrintInfo(void);
static int scoutReadDir(int);
static int scoutQuit(void);
static int scoutQsortCMP(const void *, const void *);
static int scoutSearch(char *);
static int scoutSetup(char *);
static void scoutTermResizeHandler(int);

/* variables */
static char *username;
static int scoutready;
static int running = 1;
static SDIR *bufferDIR;
static SCOUT *scout[3];

/* configuration */
#include "config.h"

int scoutAddFile(ENTR **entry, char *path)
{
	ENTR *temp;
	struct stat fstat;

	if (lstat(path, &fstat) != OK)
		return ERR;

	if ((temp = (ENTR *) calloc(1, sizeof(ENTR))) == NULL)
		return ERR;

	if ((temp->name = malloc(sizeof(char) * (strlen(path) + 1))) == NULL)
		return ERR;

	strcpy(temp->name, path);
	scoutGetFileType(temp);
	*entry = temp;

	return OK;
}

int scoutBuildWindows(void)
{
	int i;
	int height, width;
	int starty, startx;

	height = LINES - 2;
	width  = COLS / 6;
	starty = 1;
	startx = 0;
	
	if (LINES < minRows || COLS < minCols)
	{
		wclear(stdscr);
		wattron(stdscr, COLOR_PAIR(CP_ERROR));
		mvwprintw(stdscr, LINES / 2, (COLS - strlen(errorTinyTerm)) / 2, errorTinyTerm);
		wattroff(stdscr, COLOR_PAIR(CP_ERROR));
		wrefresh(stdscr);

		scoutready = ERR;
		return ERR;
	}

	for (i = 0; i < 3; i++)
	{
		scout[i]->width     = width *= i + 1;
		scout[i]->height    = height;
		scout[i]->upperthrs = height / 6;
		scout[i]->lowerthrs = height - (height / 6);

		if ((scout[i]->win = newwin(height, width, starty, startx)) == NULL)
			return ERR;
		startx += width;
	}
	
	scoutready = OK;
	return OK;
}

int scoutDestroyWindows(void)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (scout[i]->win != NULL)
		{
			wclear(scout[i]->win);
			wrefresh(scout[i]->win);
			delwin(scout[i]->win);
			scout[i]->win = NULL;
		}
	}	

	clear();
	endwin();

	if (running)
		refresh();

	return OK;
}

int scoutCommandLine(char *cmd)
{
	//HOLY TODO!!!
	int incmdmode = 1;
	int cury, curx, c, i, j;
	char str[PATH_MAX];

	wmove(stdscr, LINES - 1, 0);
	wclrtoeol(stdscr);

	i = j = 0;
	str[i++] = ':';
	if (cmd != NULL)
	{
		while (cmd[j] != '\0')
		{
			str[i++] = cmd[j++];
		}
	}
	str[i++] = ' ';
		strcpy(str, cmd);
	wprintw(stdscr, "%s ", str);
	getyx(stdscr, cury, curx);
	wrefresh(stdscr);
	curs_set(1);

	while (incmdmode && (c = wgetch(stdscr)) != EOF)
	{
		switch (c)
		{
			case KEY_LEFT:
				if (curx > 0)
					wmove(stdscr, cury, --curx);
				break;

			case KEY_RIGHT:
				if (curx <= i)
					wmove(stdscr, cury, ++curx);
				break;

			case KEY_BACKSPACE:
				if (curx > 1)
				{
					wmove(stdscr, cury, --curx);
					wdelch(stdscr);
				}

				break;

			case KEY_ENTER:
				str[i] = '\0';
				break;

			case 27: //slow as all hell!
				incmdmode = 0;
				break;

			default:
				if (c >= 32 && c <= 126)
				{
					curx++;
					str[i++] = c;
					waddch(stdscr, c);
				}

				break;
		}
	}

	scoutPrintInfo();
	curs_set(0);
	return OK;
}

int scoutFreeDir(SDIR *dir)
{
	NODE *entry;

	if (dir == NULL)
		return ERR;

	free(dir->path);
	free(dir->selected);
	entry = dir->list;
	while (entry != NULL)
	{
		free(entry->file->size);
		free(entry->file->name);
		free(entry->file);
		if (entry->next != NULL)
		{
			entry = entry->next;
			free(entry->prev);
		}
		else
		{
			free(entry);
			entry = NULL;
		}
	}
	free(dir);
	bufferDIR = NULL;

	return OK;
}

int scoutFreeDirSize(int dir)
{
	NODE *entry;

	if (dir != CURR && scout[dir]->dir->sel != NULL)
	{
		entry = scout[dir]->dir->list;
		while (entry != NULL)
		{
			free(entry->file->size);
			entry->file->size = NULL;
			entry = entry->next;
		}
	}

	if (scout[CURR]->dir->sel != NULL)
	{
		entry = scout[CURR]->dir->list;
		while (entry != NULL)
		{
			scoutGetFileSize(entry->file);
			entry = entry->next;
		}
	}

	return OK;
}

int scoutFreeInfo(ENTR *file)
{	
	free(file->perms);
	file->perms = NULL;
	free(file->users);
	file->users = NULL;
	free(file->dates);
	file->dates = NULL;
	free(file->lpath);
	file->lpath = NULL;

	return OK;
}

int scoutGetFileInfo(ENTR *entry)
{
	int i = 0;
	time_t time;
	struct tm stime;
	struct stat fstat;
	char permsbuf[12];
	char ctimebuf[18];
	struct passwd *pws;
	char truepath[PATH_MAX];

	if (lstat(entry->name, &fstat) != OK)
		return ERR;

	if ((fstat.st_mode & S_IFMT) == S_IFLNK)
	{
		if (realpath(entry->name, truepath) != NULL)
		{
			if (lstat(truepath, &fstat) == OK)
				permsbuf[i++] = 'l';
			else if (lstat(entry->name, &fstat) != OK)
				return ERR;
		}
		else
			strcpy(truepath, errorSymBroken);
	}

	if (i == 0)
	{
		switch (fstat.st_mode & S_IFMT)
		{
			case S_IFREG:
				permsbuf[i++] = '-';
				break;
			case S_IFDIR:
				permsbuf[i++] = 'd';
				break;
			case S_IFLNK:
				permsbuf[i++] = 'l';
				break;
			case S_IFCHR:
				permsbuf[i++] = 'c';
				break;
			case S_IFBLK:
				permsbuf[i++] = 'b';
				break;
			case S_IFSOCK:
				permsbuf[i++] = 's';
				break;
			case S_IFIFO:
				permsbuf[i++] = 'f';
				break;

			default:
				permsbuf[i++] = '-';
				break;
		}
	}

	permsbuf[i++] = fstat.st_mode & S_IRUSR ? 'r' : '-';
	permsbuf[i++] = fstat.st_mode & S_IWUSR ? 'w' : '-';
	permsbuf[i++] = fstat.st_mode & S_IXUSR ? 'x' : '-';
	permsbuf[i++] = fstat.st_mode & S_IRGRP ? 'r' : '-';
	permsbuf[i++] = fstat.st_mode & S_IWGRP ? 'w' : '-';
	permsbuf[i++] = fstat.st_mode & S_IXGRP ? 'x' : '-';
	permsbuf[i++] = fstat.st_mode & S_IROTH ? 'r' : '-';
	permsbuf[i++] = fstat.st_mode & S_IWOTH ? 'w' : '-';
	permsbuf[i++] = fstat.st_mode & S_IXOTH ? 'x' : '-';
	permsbuf[i] = '\0';

	if ((entry->perms = malloc(sizeof(char) * (strlen(permsbuf) + 1))) == NULL)
		return ERR;
	strcpy(entry->perms, permsbuf);

	if ((pws = getpwuid(fstat.st_uid)) == NULL)
		return ERR;
	if ((entry->users = malloc(sizeof(char) * (strlen(pws->pw_name) + 1))) == NULL)
		return ERR;
	strcpy(entry->users, pws->pw_name);

	time = fstat.st_mtime;
	localtime_r(&time, &stime);
	strftime(ctimebuf, 18, "%Y-%m-%d %H:%M", &stime);
	if ((entry->dates = malloc(sizeof(char) * (strlen(ctimebuf) + 1))) == NULL)
		return ERR;
	strcpy(entry->dates, ctimebuf);

	if (entry->issym)
	{
		if ((entry->lpath = malloc(sizeof(char) * (strlen(truepath) + 1))) == NULL)
			return ERR;
		strcpy(entry->lpath, truepath);
	}
	
	return OK;
}

int scoutGetFileSize(ENTR *entry)
{
	int i;
	DIR *pdir;
	float fsize;
	char sbuf[60];
	char sizebuf[64];
	struct dirent *d;
	struct stat fstat;
	unsigned dsize = 0;
	char truepath[PATH_MAX];
	char sizearr[] = "BKMGTPEZY";

	if (lstat(entry->name, &fstat) != OK)
	{
		TOTALFAILURE:
		if ((entry->size = malloc(sizeof(char) * 2)) == NULL)
			return ERR;

		strcpy(entry->size, "?");
		entry->isatu = ERR;
		return ERR;		
	}

	sizebuf[0] = '\0';
	if ((fstat.st_mode & S_IFMT) == S_IFLNK)
	{
		if (realpath(entry->name, truepath) != NULL)
		{
			if (lstat(truepath, &fstat) == OK)
				strcat(sizebuf, "-> ");
			else if (lstat(entry->name, &fstat) != OK)
				goto TOTALFAILURE;
		}
		else
			strcat(sizebuf, "-> ");
	}

	switch (fstat.st_mode & S_IFMT)
	{
		case S_IFREG:
			fsize = fstat.st_size;
			for (i = 0; i < 9 && fsize > 1024.00; i++)
				fsize /= 1024.00;

			sprintf(sbuf, (i > 0) ? "%.1f %c" : "%.0f %c", fsize, sizearr[i]);
			strcat(sizebuf, sbuf);
			break;
		case S_IFDIR:
			if ((pdir = opendir(entry->issym ? truepath : entry->name)) != NULL)
			{
				while ((d = readdir(pdir)) != NULL)
				{
					if (strcmp(d->d_name, ".") == OK 
					|| strcmp(d->d_name, "..") == OK)
						continue;
					else
						dsize++;
				}
				sprintf(sbuf, "%d", dsize);
				strcat(sizebuf, sbuf);
				closedir(pdir);
			}
			else
			{
				strcat(sizebuf, "N/A");
				entry->isatu = ERR;
			}
			break;
		case S_IFBLK:
			strcat(sizebuf, "block");
			break;
		case S_IFSOCK:
			strcat(sizebuf, "sock");
			break;
		case S_IFIFO:
			strcat(sizebuf, "fifo");
			break;
		case S_IFCHR:
			strcat(sizebuf, "dev");
			break;

		default:
			strcat(sizebuf, "N/A");
			entry->isatu = ERR;
			break;
	}

	if ((entry->size = malloc(sizeof(char) * (strlen(sizebuf) + 1))) == NULL)
		return ERR;

	strcpy(entry->size, sizebuf);
	return OK;
}

int scoutGetFileType(ENTR *entry)
{
	int i;
	char *ext;
	struct stat fstat;
	char truepath[PATH_MAX];

	if (lstat(entry->name, &fstat) != OK)
		return ERR;

	entry->issym = 0;
	if ((fstat.st_mode & S_IFMT) == S_IFLNK)
	{
		if (realpath(entry->name, truepath) != NULL)
		{
			if (lstat(truepath, &fstat) == OK)
				entry->issym = 1;
			else if (lstat(entry->name, &fstat) != OK)
				return ERR;
		}
		else
			entry->issym = 1;
	}

	switch (fstat.st_mode & S_IFMT)
	{
		case S_IFDIR:
			entry->type = CP_DIRECTORY;
			break;
		case S_IFSOCK:
			entry->type = CP_SOCK;
			break;
		case S_IFIFO:
			entry->type = CP_FIFO;
			break;
		case S_IFCHR:
			entry->type = CP_CHR;
			break;
		case S_IFBLK:
			entry->type = CP_BLK;
			break;
		case S_IFREG:
			if ((fstat.st_mode & S_IXUSR) 
			&& (fstat.st_mode & S_IXOTH) 
			&& (fstat.st_mode & S_IXGRP))
			{
				entry->type = CP_EXECUTABLE;
				return OK;
			}

			if ((ext = strrchr(entry->name, '.')) == NULL || *(++ext) == '\0')
			{
				entry->type = CP_DEFAULT;
				return OK;
			}

			for (i = 0; i < ARRLENGTH(extVideo); i++)
			{
				if (strcmp(ext, extVideo[i]) == OK)
				{
					entry->type = CP_VIDEO;
					return OK;
				}
			}

			for (i = 0; i < ARRLENGTH(extAudio); i++)
			{
				if (strcmp(ext, extAudio[i]) == OK)
				{
					entry->type = CP_AUDIO;
					return OK;
				}
			}

			for (i = 0; i < ARRLENGTH(extImage); i++)
			{
				if (strcmp(ext, extImage[i]) == OK)
				{
					entry->type = CP_IMAGE;
					return OK;
				}
			}

			for (i = 0; i < ARRLENGTH(extArchive); i++)
			{
				if (strcmp(ext, extArchive[i]) == OK)
				{
					entry->type = CP_ARCHIVE;
					return OK;
				}
			}

			entry->type = CP_DEFAULT;
			break;

		default: entry->type = CP_DEFAULT;
	}

	return OK;
}

int scoutInitializeCurses(void)
{
	int i;
	int temp;

	if (initscr() == NULL)
		return ERR;

	cbreak();
	noecho();
	curs_set(0);
	start_color();
	use_default_colors();

	temp = ARRLENGTH(nColors);
	for (i = 0; i < temp; i++)
		init_pair(nColors[i][0], nColors[i][1], nColors[i][2]);

	refresh();

	keypad(stdscr, TRUE);

	return OK;
}

int scoutLoadDir(int dir, int mode)
{
	int i = 0;
	switch (dir)
	{
		case NEXT:
			if (mode == LOAD)
				if ((scout[NEXT]->dir = (SDIR *) calloc(1, sizeof(SDIR))) == NULL)
					return ERR;

			if (scout[CURR]->dir->sel == NULL || scout[CURR]->dir->sel->file->type != CP_DIRECTORY)
			{
				wclear(scout[NEXT]->win);
				wrefresh(scout[NEXT]->win);
				return OK;
			}

			if (scout[CURR]->dir->sel->file->isatu != OK)
			{
				wclear(scout[NEXT]->win);
				wattron(scout[NEXT]->win, COLOR_PAIR(CP_ERROR));
				mvwprintw(scout[NEXT]->win, 0, 0, errorNoAccess);
				wattrset(scout[NEXT]->win, COLOR_PAIR(CP_ERROR));
				wrefresh(scout[NEXT]->win);
				return OK;
			}

			if (mode == LOAD)
			{
				if ((scout[NEXT]->dir->path = malloc(sizeof(char) * (strlen(scout[CURR]->dir->path) + strlen(scout[CURR]->dir->sel->file->name) + 3))) == NULL)
					return ERR;

				if (scout[CURR]->dir->path[1] != '\0')
					sprintf(scout[NEXT]->dir->path, "%s/%s", scout[CURR]->dir->path, scout[CURR]->dir->sel->file->name);
				else
					sprintf(scout[NEXT]->dir->path, "/%s", scout[CURR]->dir->sel->file->name);

				scoutReadDir(NEXT);
			}

			scoutPrintListPrep(NEXT);
			scoutPrintList(NEXT);
			return OK;
		
		case PREV:
			if (mode == LOAD)
				if ((scout[PREV]->dir = (SDIR *) calloc(1, sizeof(SDIR))) == NULL)
					return ERR;
			
			if (scout[CURR]->dir->path[1] == '\0')
			{
				wclear(scout[PREV]->win);
				wrefresh(scout[PREV]->win);
				return OK;
			}

			if (mode == LOAD)
			{
				while (scout[CURR]->dir->path[++i] != '\0');
				while (scout[CURR]->dir->path[--i] != '/');
				
				if ((scout[PREV]->dir->path = malloc(sizeof(char) * (i + 2))) == NULL)
					return ERR;

				strncpy(scout[PREV]->dir->path, scout[CURR]->dir->path, i + 1);

				/* Ternary operator accounts for "/" directory */
				scout[PREV]->dir->path[i ? i : 1] = '\0';
				scoutReadDir(PREV);
			}

			scoutPrintListPrep(PREV);
			scoutPrintList(PREV);
			return OK;

		case CURR:
			if (mode == LOAD)
			{
				scoutReadDir(CURR);
				scoutFreeDirSize(CURR);
			}

			scoutPrintListPrep(CURR);
			scoutPrintList(CURR);
			return OK;
		
		default:
			return ERR;
	}

	return ERR;
}

int scoutMove(int dir)
{
	int temp;
	NODE *entry;

	switch (dir)
	{
		case TOP:
			if (scout[CURR]->dir->sel == NULL)
				return OK;

			scout[CURR]->dir->sel = scout[CURR]->dir->list;
			scout[CURR]->dir->selline = 0;

			bufferDIR = scout[NEXT]->dir; // TODO Improve buffering system

			scoutPrintList(CURR);
			scoutLoadDir(NEXT, LOAD);
			break;

		case BOT:
			if (scout[CURR]->dir->sel == NULL)
				return OK;
			
			for (temp = scout[CURR]->dir->selline; scout[CURR]->dir->sel->next != NULL; temp++)
				scout[CURR]->dir->sel = scout[CURR]->dir->sel->next;
				
			if (temp > scout[CURR]->height - 1)
				scout[CURR]->dir->selline = scout[CURR]->height - 1;
			else
				scout[CURR]->dir->selline = temp;

			bufferDIR = scout[NEXT]->dir; // TODO Improve buffering system

			scoutPrintList(CURR);
			scoutLoadDir(NEXT, LOAD);
			break;

		case UP:
			if (scout[CURR]->dir->sel == NULL)
				return OK;

			if (scout[CURR]->dir->sel->prev == NULL)
				return OK;

			bufferDIR = scout[NEXT]->dir; // TODO Improve buffering system

			scout[CURR]->dir->sel = scout[CURR]->dir->sel->prev;

			if (scout[CURR]->dir->selline <= scout[CURR]->upperthrs)
			{
				for (entry = scout[CURR]->dir->sel, temp = scout[CURR]->upperthrs; temp > 0 && entry->prev != NULL; temp--)
					entry = entry->prev;

				if (temp > 0)
					scout[CURR]->dir->selline--;
			}
			else
				scout[CURR]->dir->selline--;

			scoutPrintList(CURR);
			scoutLoadDir(NEXT, LOAD);		
			break;

		case DOWN:
			if (scout[CURR]->dir->sel == NULL)
				return OK;

			if (scout[CURR]->dir->sel->next == NULL)
				return OK;

			bufferDIR = scout[NEXT]->dir; // TODO Improve buffering system

			scout[CURR]->dir->sel = scout[CURR]->dir->sel->next;

			if (scout[CURR]->dir->selline >= scout[CURR]->lowerthrs - 1)
			{
				for (entry = scout[CURR]->dir->sel, temp = scout[CURR]->upperthrs; temp > 0 && entry->next != NULL; temp--)
					entry = entry->next;

				if (temp > 0)
					scout[CURR]->dir->selline++;
			}
			else
				scout[CURR]->dir->selline++;

			scoutPrintList(CURR);
			scoutLoadDir(NEXT, LOAD);
			break;

		case LEFT:
			if (scout[CURR]->dir->path[1] == '\0')
				return OK;

			bufferDIR = scout[NEXT]->dir; // TODO Improve buffering system

			scout[NEXT]->dir = scout[CURR]->dir;
			scout[CURR]->dir = scout[PREV]->dir;
			chdir(scout[CURR]->dir->path);
			scoutFreeDirSize(NEXT);
			scoutPrintList(CURR);
			scoutPrintList(NEXT);
			scoutLoadDir(PREV, LOAD);
			break;

		case RIGHT:
			if (scout[CURR]->dir->sel == NULL)
				return OK;

			if (scout[CURR]->dir->sel->file->type != CP_DIRECTORY)
				return OK;

			if (scout[CURR]->dir->sel->file->isatu != OK)
				return OK;

			bufferDIR = scout[PREV]->dir; // TODO Improve buffering system

			scout[PREV]->dir = scout[CURR]->dir;
			scout[CURR]->dir = scout[NEXT]->dir;
			chdir(scout[CURR]->dir->path);
			scoutFreeDirSize(PREV);
			scoutPrintList(PREV);
			scoutPrintList(CURR);
			scoutLoadDir(NEXT, LOAD);
			break;

		default:
			return ERR;
	}

	scoutPrintInfo();
	scoutFreeDir(bufferDIR); // TODO Improve buffering system

	return OK;
}

int scoutPrintEntry(WINDOW *win, ENTR *entry, int line, int length, int selected)
{
	char *extstr;
	char string[length];
	int srclen, extlen, buflen;

	if (entry == NULL)
	{
		wattron(win, COLOR_PAIR(CP_ERROR));
		mvwprintw(win, line, 0, errorDirEmpty);
		wattrset(win, COLOR_PAIR(CP_ERROR));
		return OK;
	}

	buflen = 1;
	string[--length] = '\0';
	string[--length] = ' ';
	if (entry->size != NULL)
	{
		srclen = strlen(entry->size);
		while (srclen > 0)
			string[--length] = entry->size[--srclen];
		string[--length] = ' ';
	}

	srclen = strlen(entry->name);
	while (length > srclen + buflen)
		string[--length] = ' ';

	if (entry->name[0] != '.' && (extstr = strrchr(entry->name, '.')) != NULL)
	{
		extlen = strlen(extstr);
		while (--extlen >= 0)
			string[--length] = entry->name[--srclen];
	}

	if (srclen > length - buflen)
	{
		string[--length] = '~';
		srclen = length - buflen;
	}

	while (srclen > 0)
		string[--length] = entry->name[--srclen];

	while (length > 0)
		string[--length] = ' ';

	if (selected)
		wattron(win, A_REVERSE);

	if (selected || entry->type == CP_DIRECTORY || entry->type == CP_EXECUTABLE || entry->type >= 8)
		wattron(win, A_BOLD);

	wattron(win, COLOR_PAIR(entry->type));
	mvwprintw(win, line, 0, string);
	wattrset(win, A_NORMAL);

	return OK;
}

int scoutPrintList(int dir)
{
	int line;
	NODE *entry;

	wclear(scout[dir]->win);
	if (scoutready != OK)
	{
		wrefresh(scout[dir]->win);
		return OK;
	}

	if ((entry = scout[dir]->dir->sel) == NULL)
	{
		scoutPrintEntry(scout[dir]->win, NULL, 0, scout[dir]->width, 0);
		wrefresh(scout[dir]->win);
		return OK;
	}

	for (line = scout[dir]->dir->selline; --line >= 0 && entry->prev != NULL; entry = entry->prev);

	for (line = 0; line < scout[dir]->dir->selline; entry = entry->next)
		scoutPrintEntry(scout[dir]->win, entry->file, line++, scout[dir]->width, 0);

	scoutPrintEntry(scout[dir]->win, entry->file, line++, scout[dir]->width, 1);
	entry = entry->next;

	for (; line < scout[dir]->height && entry != NULL; entry = entry->next)
		scoutPrintEntry(scout[dir]->win, entry->file, line++, scout[dir]->width, 0);

	wrefresh(scout[dir]->win);
	return OK;
}

int scoutPrintListPrep(int dir)
{
	int i;
	NODE *entry;

	if (scout[dir]->dir->selline != 0)
		return ERR;

	if ((entry = scout[dir]->dir->sel) == NULL)
		return ERR;

	for (i = scout[dir]->upperthrs; i > 0 && entry->next != NULL; i--, entry = entry->next);
	entry = scout[dir]->dir->sel;

	while (scout[dir]->dir->selline < scout[dir]->lowerthrs - 1 + i)
	{
		if (entry->prev == NULL)
			break;

		scout[dir]->dir->selline++;
		entry = entry->prev;
	}

	return OK;
}

int scoutPrintInfo(void)
{
	/* Cleanup */
	wmove(stdscr, 0, 0);
	wclrtoeol(stdscr);
	wmove(stdscr, LINES - 1, 0);
	wclrtoeol(stdscr);

	if (scoutready != OK)
	{
		wrefresh(stdscr);
		return OK;
	}

	/* Top bar stuff */
	wmove(stdscr, 0, 0);
	wattron(stdscr, A_BOLD);
	wattron(stdscr, COLOR_PAIR(CP_TOPBARUSER));
	printw(username);
	waddch(stdscr, ' ');
	wattroff(stdscr, COLOR_PAIR(CP_TOPBARUSER));
	wattron(stdscr, COLOR_PAIR(CP_TOPBARPATH));
	printw(scout[CURR]->dir->path);
	if (scout[CURR]->dir->path[1] != '\0')
		waddch(stdscr, '/');
	wattroff(stdscr, COLOR_PAIR(CP_TOPBARPATH));
	if (scout[CURR]->dir->sel != NULL)
	{
		wattron(stdscr, COLOR_PAIR(CP_TOPBARFILE));
		printw(scout[CURR]->dir->sel->file->name);
		wattroff(stdscr, COLOR_PAIR(CP_TOPBARFILE));
	}
	waddch(stdscr, '\n');
	wattroff(stdscr, A_BOLD);

	/* Bottom bar stuff */
	wmove(stdscr, LINES - 1, 0);
	if (scout[CURR]->dir->sel != NULL 
	&& scoutGetFileInfo(scout[CURR]->dir->sel->file) != ERR)
	{
		
		wattron(stdscr, COLOR_PAIR(CP_BOTBARPERM));
		wprintw(stdscr, scout[CURR]->dir->sel->file->perms);
		wattroff(stdscr, COLOR_PAIR(CP_BOTBARPERM));
		waddch(stdscr, ' ');
		wattron(stdscr, COLOR_PAIR(CP_BOTBARUSER));
		wprintw(stdscr, scout[CURR]->dir->sel->file->users);
		wattroff(stdscr, COLOR_PAIR(CP_BOTBARUSER));
		waddch(stdscr, ' ');
		wattron(stdscr, COLOR_PAIR(CP_BOTBARDATE));
		wprintw(stdscr, scout[CURR]->dir->sel->file->dates);
		wattroff(stdscr, COLOR_PAIR(CP_BOTBARDATE));
		waddch(stdscr, ' ');
		if (scout[CURR]->dir->sel->file->issym)
		{
			wattron(stdscr, COLOR_PAIR(CP_BOTBARLINK));
			wprintw(stdscr, "-> ", scout[CURR]->dir->sel->file->lpath);
			wattroff(stdscr, COLOR_PAIR(CP_BOTBARLINK));
			if (strcmp(scout[CURR]->dir->sel->file->lpath, errorSymBroken) == OK)
			{
				wattron(stdscr, COLOR_PAIR(CP_ERROR));
				wprintw(stdscr, errorSymBroken);
				wattroff(stdscr, COLOR_PAIR(CP_ERROR));
			}
			else
			{
				wattron(stdscr, COLOR_PAIR(CP_BOTBARLINK));
				wprintw(stdscr, scout[CURR]->dir->sel->file->lpath);
				wattroff(stdscr, COLOR_PAIR(CP_BOTBARLINK));
			}
			
		}
		waddch(stdscr, '\n');
		scoutFreeInfo(scout[CURR]->dir->sel->file);
	}

	wrefresh(stdscr);
	return OK;
}

int scoutReadDir(int dir)
{
	DIR *pdir;
	ENTR *entry;
	char *seldir;
	struct dirent *d;

	if ((pdir = opendir(scout[dir]->dir->path)) == NULL)
		return ERR;

	chdir(scout[dir]->dir->path);

	if (dir == PREV)
	{
		seldir = strrchr(scout[CURR]->dir->path, '/');
		if ((scout[dir]->dir->selected = malloc(sizeof(char) * strlen(seldir++))) == NULL)
			return ERR;

		strcpy(scout[dir]->dir->selected, seldir);
	}
	
	while ((d = readdir(pdir)) != NULL)
	{
		if (strcmp(d->d_name, ".") == OK 
		|| strcmp(d->d_name, "..") == OK)
			continue;

		if (scoutAddFile(&entry, d->d_name) != OK)
			continue; //error

		if ((scout[dir]->dir->entries = realloc(scout[dir]->dir->entries, sizeof(NODE *) * (scout[dir]->dir->entrycnt + 1))) == NULL)
			return ERR;

		if ((scout[dir]->dir->entries[scout[dir]->dir->entrycnt] = malloc(sizeof(NODE))) == NULL)
			return ERR;
		
		scout[dir]->dir->entries[scout[dir]->dir->entrycnt]->file = entry;
		scout[dir]->dir->entries[scout[dir]->dir->entrycnt]->buf  = NULL;
		scout[dir]->dir->entrycnt++;
	}

	if (scout[dir]->dir->entries != NULL)
		qsort(scout[dir]->dir->entries, scout[dir]->dir->entrycnt, sizeof(NODE *), scoutQsortCMP);

/*	FILE *fp = fopen("log", "w+");
	for (int i = 0; i < scout[dir]->dir->entrycnt; i++)
		fprintf(fp, "%s\n", scout[dir]->dir->entries[i]->file->name);
	fclose(fp); */

	if (dir != CURR)
		chdir(scout[CURR]->dir->path);

	closedir(pdir);
	return OK;
}

int scoutRun(void)
{
	int c;
	while (running && (c = wgetch(stdscr)) != EOF)
	{
		if (scoutready != OK)
		{
			if (c == 'q' || c == 'Q')
				running = 0;
			continue;
		}

		switch(c)
		{
			case 'k':
			case KEY_UP:
				scoutMove(UP);
				break;

			case 'j':
			case KEY_DOWN:
				scoutMove(DOWN);
				break;

			case 'h':
			case KEY_LEFT:
				scoutMove(LEFT);
				break;

			case 'l':
			case KEY_RIGHT:
				scoutMove(RIGHT);
				break;

			case 'g':
				if ((c = wgetch(stdscr)) == 'g')
					scoutMove(TOP);
				else
					; //other
				break;

			case 'G':
				scoutMove(BOT);
				break;


			case 'a':
				scoutCommandLine("rename");
				break;

			case 's':
				scoutCommandLine("shell");
				break;

			case '/':
				scoutCommandLine("search");
				break;

			case ';':
			case ':':
				scoutCommandLine(NULL);
				break;

			case 'q':
			case 'Q':
				running = 0;
				break;

			case 'r':
			case 'R':
				raise(SIGWINCH);
				break;

			default: break;
		}
	}

	return OK;
}

int scoutQuit(void)
{
	scoutFreeDir(scout[PREV]->dir);
	scoutFreeDir(scout[CURR]->dir);
	scoutFreeDir(scout[NEXT]->dir);
	scoutDestroyWindows();
	free(scout[PREV]);
	free(scout[CURR]);
	free(scout[NEXT]);
	free(username);

	return OK;
}

int scoutQsortCMP(const void *A, const void *B)
{
	ENTR *fileA = (*(NODE **) A)->file;
	ENTR *fileB = (*(NODE **) B)->file;

	if (fileA->type == CP_DIRECTORY && fileB->type == CP_DIRECTORY)
		return utilsNameCMP(fileA->name, fileB->name);
	
	if (fileA->type != CP_DIRECTORY && fileB->type != CP_DIRECTORY)
		return utilsNameCMP(fileA->name, fileB->name);

	if (fileA->type == CP_DIRECTORY && fileB->type != CP_DIRECTORY)
		return -1;

	return 1;
}

int scoutSetup(char *path)
{
	int i;
	char hostname[64];
	struct passwd *pw;
	char truepath[PATH_MAX];

	if (realpath(path, truepath) == NULL)
		return ERR;

	for (i = 0; i < 3; i++)
		if ((scout[i] = (SCOUT *) calloc(1, sizeof(SCOUT))) == NULL)
			return ERR;

	if ((scout[CURR]->dir = (SDIR *) calloc(1, sizeof(SDIR))) == NULL)
		return ERR;
	
	if ((scout[CURR]->dir->path = malloc(sizeof(char) * (strlen(truepath) + 1))) == NULL)
		return ERR;

	strcpy(scout[CURR]->dir->path, truepath);

	pw = getpwuid(geteuid());
	gethostname(hostname, sizeof(hostname));

	if ((username = malloc(sizeof(char) * (strlen(pw->pw_name) + strlen(hostname) + 2))) == NULL)
		return ERR;

	sprintf(username, "%s@%s", pw->pw_name, hostname);

	scoutInitializeCurses();
	scoutBuildWindows();

	scoutLoadDir(CURR, LOAD);
	scoutLoadDir(PREV, LOAD);
	scoutLoadDir(NEXT, LOAD);
	scoutPrintInfo();

	return OK;
}

int scoutSearch(char *needle)
{
	//HOLY TODO!!!
	NODE *entry;

	if ((entry = scout[CURR]->dir->sel) == NULL)
		return ERR;

	while (entry != NULL)
	{
		if (strcmp(entry->file->name, needle) == OK)
		{
			scout[CURR]->dir->sel = entry;
			break;
		}

		entry = entry->next;
	}
	return OK;
}

void scoutTermResizeHandler(int null)
{
	int i;

	scoutDestroyWindows();
	scoutBuildWindows();
	scoutPrintInfo();
	curs_set(0);

	for (i = 0; i < 3; i++)
		if (scout[i]->dir != NULL)
			scout[i]->dir->selline = 0;

	scoutLoadDir(PREV, RELOAD);
	scoutLoadDir(CURR, RELOAD);
	scoutLoadDir(NEXT, RELOAD);
}

int main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp(argv[1], "-v"))
	{
		fprintf(stdout, "scout version %s\n", VERSION);
		exit(EXIT_SUCCESS);
	}

	if (argc == 2 && !strcmp(argv[1], "-h"))
	{
		fprintf(stdout, "Usage: scout [path]\n       -h help\n       -v version\n");
		exit(EXIT_SUCCESS);
	}

	if (scoutSetup((argc > 1) ? argv[1] : ".") == ERR)
	{
		fprintf(stderr, "Error: invalid argument(s), try -h\n");
		exit(EXIT_FAILURE);
	}
	
	signal(SIGWINCH, scoutTermResizeHandler);

	scoutRun();

	scoutQuit();
}