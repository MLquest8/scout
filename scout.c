#include <sys/stat.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <stdarg.h>
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
	SDIR *buf;
	ENTR *file;
};

struct sdir
{	
	char *path;
	int selentry;
	int firstentry;
	int entrycount;
	NODE **entries;
	char *selected;
};

/* function declarations */
static int scoutAddFile(ENTR **, char *);
static int scoutBuildWindows(void);
static int scoutDestroyWindows(void);
static int scoutCompareEntries(const void *, const void *);
static int scoutCommandLine(char *);
static int scoutFreeDir(SDIR **);
static int scoutFreeInfo(ENTR *);
static int scoutFreeDirSize(SDIR *);
static int scoutGetDirSize(SDIR *);
static int scoutGetFileInfo(ENTR *);
static int scoutGetFileSize(ENTR *);
static int scoutGetFileType(ENTR *);
static int scoutInitializeCurses(void);
static int scoutLoadCURR(int);
static int scoutLoadNEXT(int);
static int scoutLoadPREV(int);
static int scoutMatchSelEntry(SDIR *);
static int scoutMove(int);
static int scoutPrepareString(ENTR *, char *, int);
static int scoutPrintList(SDIR *, WINDOW *);
static int scoutPrintListPrep(SDIR *);
static int scoutPrintInfo(void);
static int scoutReadDir(SDIR *);
static int scoutQuit(void);
static int scoutSetup(char *);
static void scoutTermResHandler(int);

/* variables */
static int running = 1;
static struct mainstruct
{
	int cols;
	int lines;
	int topthrsh;
	int botthrsh;
	SDIR *dir[3];
	WINDOW *win[3];
	char *username;
} *scout;

static SDIR *bufferDIR;

/* configuration */
#include "config.h"

int scoutAddFile(ENTR **file, char *path)
{
	ENTR *temp;
	struct stat fstat;

	if (lstat(path, &fstat) != OK)
		return ERR;

	if ((temp = (ENTR *) calloc(1, sizeof(ENTR))) == NULL)
		return ERR;

	if ((temp->name = malloc(sizeof(char *) * (strlen(path) + 1))) == NULL)
		return ERR;

	strcpy(temp->name, path);
	scoutGetFileType(temp);
	*file = temp;

	return OK;
}

int scoutBuildWindows(void)
{
	int x;
	int cols;

	scout->cols = COLS;
	scout->lines = LINES - 2;
	scout->topthrsh = scout->lines / 6;
	scout->botthrsh = scout->lines - scout->topthrsh - 1;

	x = 0;
	cols = (COLS / 2) / 4;
	if ((scout->win[PREV] = newwin(LINES - 2, cols, 1, x)) == NULL)
		return ERR;
	
	x += cols;
	cols = (COLS / 2) - cols;
	if ((scout->win[CURR] = newwin(LINES - 2, cols, 1, x)) == NULL)
		return ERR;

	x += cols;
	cols = COLS / 2;
	if ((scout->win[NEXT] = newwin(LINES - 2, cols, 1, x)) == NULL)
		return ERR;
	
	return OK;
}

int scoutDestroyWindows(void)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		if (scout->win[i] != NULL)
		{
			wclear(scout->win[i]);
			wrefresh(scout->win[i]);
			delwin(scout->win[i]);
			scout->win[i] = NULL;
		}
	}

	clear();
	endwin();

	if (!running)
		return OK;
	
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

int scoutCompareEntries(const void *entryA, const void *entryB)
{
	ENTR *fileA = (*(NODE **) entryA)->file;
	ENTR *fileB = (*(NODE **) entryB)->file;

	if (fileA->type == CP_DIRECTORY && fileB->type == CP_DIRECTORY)
		return utilsNameCMP(fileA->name, fileB->name);
	
	if (fileA->type != CP_DIRECTORY && fileB->type != CP_DIRECTORY)
		return utilsNameCMP(fileA->name, fileB->name);

	if (fileA->type == CP_DIRECTORY && fileB->type != CP_DIRECTORY)
		return -1;

	return 1;
}

int scoutFreeDir(SDIR **pdir)
{
	int i = 0;
	SDIR *dir;
	ENTR *file;

	if ((dir = *pdir) == NULL)
		return ERR;

	free(dir->path);
	free(dir->selected);
	if (dir->entries != NULL)
	{
		while (i < dir->entrycount)
		{
			scoutFreeDir(&dir->entries[i]->buf);
			file = dir->entries[i]->file;
			scoutFreeInfo(file);
			free(file->size);
			free(file->name);
			free(file);
			free(dir->entries[i++]);
		}
		free(dir->entries);
	}
	free(dir);
	*pdir = NULL;
	
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

int scoutFreeDirSize(SDIR *dir)
{
	int i;

	for (i = 0; i < dir->entrycount; i++)
	{
		free(dir->entries[i]->file->size);
		dir->entries[i]->file->size = NULL;
	}

	return OK;
}

int scoutGetDirSize(SDIR *dir)
{
	int i;

	for (i = 0; i < dir->entrycount; i++)
		scoutGetFileSize(dir->entries[i]->file);

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
			truepath[0] = '\0'; /* marks broken symlinks */
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

	if ((entry->perms = malloc(sizeof(char *) * (strlen(permsbuf) + 1))) == NULL)
		return ERR;
	strcpy(entry->perms, permsbuf);

	if ((pws = getpwuid(fstat.st_uid)) == NULL)
		return ERR;
	if ((entry->users = malloc(sizeof(char *) * (strlen(pws->pw_name) + 1))) == NULL)
		return ERR;
	strcpy(entry->users, pws->pw_name);

	time = fstat.st_mtime;
	localtime_r(&time, &stime);
	strftime(ctimebuf, 18, "%Y-%m-%d %H:%M", &stime);
	if ((entry->dates = malloc(sizeof(char *) * (strlen(ctimebuf) + 1))) == NULL)
		return ERR;
	strcpy(entry->dates, ctimebuf);

	if (entry->issym && truepath[0] != '\0')
	{
		if ((entry->lpath = malloc(sizeof(char *) * (strlen(truepath) + 1))) == NULL)
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
		if ((entry->size = malloc(sizeof(char *) * 2)) == NULL)
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

	if ((entry->size = malloc(sizeof(char *) * (strlen(sizebuf) + 1))) == NULL)
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
	/* something something TODO */
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

int scoutLoadCURR(int mode)
{
	NODE *selentry;

	if (mode == LOAD)
	{
		scoutReadDir(scout->dir[CURR]);
		scoutGetDirSize(scout->dir[CURR]);
	}

	if (scout->dir[CURR]->entries != NULL)
		selentry = scout->dir[CURR]->entries[scout->dir[CURR]->selentry];
	else
		selentry = NULL;

	if (selentry == NULL)
	{
		wclear(scout->win[NEXT]);
		wattron(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
		mvwprintw(scout->win[NEXT], 0, 0, errorDirEmpty);
		wattrset(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
		wrefresh(scout->win[NEXT]);
		return OK;
	}

	scoutPrintList(scout->dir[CURR], scout->win[CURR]);
	return OK;
}

int scoutLoadNEXT(int mode)
{
	NODE *selentry;

	if (mode == LOAD)
		if ((scout->dir[NEXT] = calloc(1, sizeof(SDIR))) == NULL)
			return ERR;

	if (scout->dir[CURR]->entries != NULL)
		selentry = scout->dir[CURR]->entries[scout->dir[CURR]->selentry];
	else
		selentry = NULL;

	if (selentry == NULL)
	{
		wclear(scout->win[NEXT]);
		wattron(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
		mvwprintw(scout->win[NEXT], 0, 0, errorDirEmpty);
		wattrset(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
		wrefresh(scout->win[NEXT]);
		return OK;
	}

	if (selentry->file->isatu != OK)
	{
		wclear(scout->win[NEXT]);
		wattron(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
		mvwprintw(scout->win[NEXT], 0, 0, errorNoAccess);
		wattrset(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
		wrefresh(scout->win[NEXT]);
		return OK;
	}

	if (selentry->file->type != CP_DIRECTORY)
	{
		wclear(scout->win[NEXT]);
		wrefresh(scout->win[NEXT]);
		return OK;
	}

	if (mode == LOAD)
	{
		if ((scout->dir[NEXT]->path = malloc(sizeof(char *) * (strlen(selentry->file->name) + 3))) == NULL)
			return ERR;

		if (scout->dir[CURR]->path[1] != '\0')
			sprintf(scout->dir[NEXT]->path, "%s/%s", scout->dir[CURR]->path, selentry->file->name);
		else
			sprintf(scout->dir[NEXT]->path, "/%s", selentry->file->name);

		scoutReadDir(scout->dir[NEXT]);
	}

	scoutPrintList(scout->dir[NEXT], scout->win[NEXT]);
	return OK;
}

int scoutLoadPREV(int mode)
{
	int i = 0;
	char *temp;

	if (mode == LOAD)
		if ((scout->dir[PREV] = calloc(1, sizeof(SDIR))) == NULL)
			return ERR;
			
	if (scout->dir[CURR]->path[1] == '\0')
	{
		wclear(scout->win[PREV]);
		wrefresh(scout->win[PREV]);
		return OK;
	}

	if (mode == LOAD)
	{
		while (scout->dir[CURR]->path[++i] != '\0');
		while (scout->dir[CURR]->path[--i] != '/');
				
		if ((scout->dir[PREV]->path = malloc(sizeof(char *) * (i + 2))) == NULL)
			return ERR;

		/* Ternary operator accounts for "/" directory */
		strncpy(scout->dir[PREV]->path, scout->dir[CURR]->path, i + 1);
		scout->dir[PREV]->path[i ? i : 1] = '\0';

		scoutReadDir(scout->dir[PREV]);

		temp = strrchr(scout->dir[CURR]->path, '/');
		if ((scout->dir[PREV]->selected = malloc(sizeof(char *) * strlen(temp++))) == NULL)
			return ERR;
		strcpy(scout->dir[PREV]->selected, temp);
		scoutMatchSelEntry(scout->dir[PREV]);
	}

	scoutPrintList(scout->dir[PREV], scout->win[PREV]);
	return OK;
}

int scoutMatchSelEntry(SDIR *dir)
{
	int i, j, k, l;
	NODE *dummyptr;
	NODE dummynode;
	ENTR dummyfile;

	dummyptr = &dummynode;
	dummynode.file = &dummyfile;
	dummyfile.type = CP_DIRECTORY;
	dummyfile.name = dir->selected;

	i = 0;
	j = dir->entrycount;
	l = (j - i) / 2;
	
	while ((k = scoutCompareEntries(&dummyptr, &dir->entries[l])) != 0)
	{
		if (k < 0)
		{
			j = l;
			l = (j - i) / 2;
		}
		else
		{
			i = l;
			l = l + (j - i) / 2;
		}
	}

	dir->selentry = l;
	return OK;
}

int scoutMove(int dir)
{
	switch (dir)
	{
		case TOP:
			if (scout->dir[CURR]->entries == NULL)
				return OK;

			if (scout->dir[CURR]->selentry == 0)
				return OK;

			bufferDIR = scout->dir[NEXT]; // TODO Improve buffering system

			scout->dir[CURR]->firstentry = scout->dir[CURR]->selentry = 0;
			
			scoutLoadCURR(RELOAD);
			scoutLoadNEXT(LOAD);

			break;

		case BOT:
			if (scout->dir[CURR]->entries == NULL)
				return OK;

			if (scout->dir[CURR]->selentry == scout->dir[CURR]->entrycount - 1)
				return OK;

			bufferDIR = scout->dir[NEXT]; // TODO Improve buffering system

			scout->dir[CURR]->selentry = scout->dir[CURR]->entrycount - 1;
			scout->dir[CURR]->firstentry = 0; /* handled elsewhere */
			
			scoutLoadCURR(RELOAD);
			scoutLoadNEXT(LOAD);

			break;

		case UP:
			if (scout->dir[CURR]->entries == NULL)
				return OK;

			if (scout->dir[CURR]->selentry == 0)
				return OK;

			bufferDIR = scout->dir[NEXT]; // TODO Improve buffering system

			if (scout->dir[CURR]->selentry - scout->dir[CURR]->firstentry <= scout->topthrsh)
				if (scout->dir[CURR]->firstentry != 0)
					scout->dir[CURR]->firstentry--;

			scout->dir[CURR]->selentry--;

			scoutLoadCURR(RELOAD);
			scoutLoadNEXT(LOAD);
		
			break;

		case DOWN:
			if (scout->dir[CURR]->entries == NULL)
				return OK;

			if (scout->dir[CURR]->selentry == scout->dir[CURR]->entrycount - 1)
				return OK;

			bufferDIR = scout->dir[NEXT]; // TODO Improve buffering system

			if (scout->dir[CURR]->selentry - scout->dir[CURR]->firstentry >= scout->botthrsh)
				if (scout->dir[CURR]->entrycount - scout->dir[CURR]->selentry > scout->topthrsh + 1)
					scout->dir[CURR]->firstentry++;

			scout->dir[CURR]->selentry++;

			scoutLoadCURR(RELOAD);
			scoutLoadNEXT(LOAD);
			
			break;

		case LEFT:
			if (scout->dir[CURR]->path[1] == '\0')
				return OK;

			bufferDIR = scout->dir[NEXT]; // TODO Improve buffering system

			scoutFreeDirSize(scout->dir[CURR]);
			scout->dir[NEXT] = scout->dir[CURR];
			scout->dir[CURR] = scout->dir[PREV];

			chdir(scout->dir[CURR]->path);
			scoutGetDirSize(scout->dir[CURR]);

			scoutLoadCURR(RELOAD);
			scoutLoadNEXT(RELOAD);
			scoutLoadPREV(LOAD);

			break;

		case RIGHT:
			if (scout->dir[CURR]->entries == NULL)
				return OK;

			if (scout->dir[CURR]->entries[scout->dir[CURR]->selentry]->file->type != CP_DIRECTORY)
				return OK;

			if (scout->dir[CURR]->entries[scout->dir[CURR]->selentry]->file->isatu != OK)
				return OK;

			bufferDIR = scout->dir[PREV]; // TODO Improve buffering system

			scoutFreeDirSize(scout->dir[CURR]);
			scout->dir[PREV] = scout->dir[CURR];
			scout->dir[CURR] = scout->dir[NEXT];

			chdir(scout->dir[CURR]->path);
			scoutGetDirSize(scout->dir[CURR]);

			scoutLoadPREV(RELOAD);
			scoutLoadCURR(RELOAD);
			scoutLoadNEXT(LOAD);

			break;

		default:
			return ERR;
	}

	scoutPrintInfo();
	scoutFreeDir(&bufferDIR); // TODO Improve buffering system

	return OK;
}

int scoutPrepareString(ENTR *file, char *str, int len)
{
	int res;
	int i, j;
	int buflen;
	int extlen, extanc;
	int namelen, nameanc;
	int sizelen, sizeanc;
	char *extstr = NULL;

	res = 0;
	str[res++] = ' ';
	str[len--] = '\0';
	str[len--] = ' ';

	if (file->size != NULL)
		sizeanc = sizelen = strlen(file->size);
	else
		sizeanc = sizelen = 0;

	if (file->name[0] != '.' && (extstr = strrchr(file->name, '.')) != NULL)
		extanc = extlen = strlen(extstr);
	else
		extanc = extlen = 0;
	
	nameanc = namelen = strlen(file->name) - extlen;
	
	buflen = len - namelen - extlen - sizelen - res;
	
	while (buflen < 0)
	{
		if (namelen <= 0)
			break;
		++buflen;
		--namelen;
	}
	while (buflen < 0)
	{
		if (extlen <= 0)
			break;
		++buflen;
		--extlen;
	}
	if (buflen < 0)
	{
		buflen += sizelen;
		sizelen = 0;
		while (buflen > 0)
		{
			if (extanc != 0)
			{
				if (extlen == extanc)
					break;
				--buflen;
				++extlen;
			}
			else
			{
				if (namelen == nameanc)
					break;
				--buflen;
				++namelen;
			}
		}
	}

	i = res;
	for (j = 0; j < namelen; j++, i++)
		str[i] = file->name[j];
	if (nameanc != namelen)
		str[i++] = '~';

	if (extstr != NULL)
	{
		for (j = 0; j < extlen; j++, i++)
			str[i] = extstr[j];
		if (extanc != extlen)
			str[i++] = '~';
	}

	if (buflen > 0)
		for (j = 0; j <= buflen; j++, i++)
			str[i] = ' ';

	if (file->size != NULL && sizelen == sizeanc)
		for (j = 0; j < sizelen; j++, i++)
			str[i] = file->size[j];

	return OK;
}

int scoutPrintList(SDIR *dir, WINDOW *win)
{
	ENTR *file;
	char *string;
	int i, j, len;

	if ((len = getmaxx(win)) < 3)
		return ERR;

	if ((string = malloc(sizeof(char *) * len)) == NULL)
		return ERR;

	scoutPrintListPrep(dir);

	wclear(win);
	for (i = 0, j = dir->firstentry; i < scout->lines && j < dir->entrycount; i++, j++)
	{
		file = dir->entries[j]->file;
		scoutPrepareString(file, string, len - 1);

		if (j == dir->selentry)
			wattron(win, A_REVERSE | A_BOLD);
		else if (file->type == CP_DIRECTORY || file->type == CP_EXECUTABLE || file->type >= 8)
			wattron(win, A_BOLD);

		wattron(win, COLOR_PAIR(file->type));
		mvwprintw(win, i, 0, string);
		wattrset(win, A_NORMAL);
	}
	wrefresh(win);
	free(string);

	return OK;
}

int scoutPrintListPrep(SDIR *dir)
{
	int temp1, temp2;

	if (dir->firstentry != 0)
		return ERR;

	if (dir->entries == NULL)
		return ERR;

	if (dir->selentry  >= scout->lines)
	{
		temp1 = dir->selentry - scout->lines + 1;
		temp2 = dir->entrycount - dir->selentry - 1;
		if (temp2 > scout->topthrsh)
			dir->firstentry = temp1 + scout->topthrsh;
		else
			dir->firstentry = temp1 + temp2;
	}
	else if (dir->selentry > scout->botthrsh && dir->entrycount > scout->lines)
	{
		temp1 = dir->selentry - scout->botthrsh;
		temp2 = dir->entrycount - scout->lines;
		if (temp2 >= temp1)
			dir->firstentry = temp1;
		else
			dir->firstentry = temp2;
	}
	else
		dir->firstentry = 0;

	return OK;
}

int scoutPrintInfo(void)
{
	NODE *selentry;

	/* Cleanup */
	wmove(stdscr, 0, 0);
	wclrtoeol(stdscr);
	wmove(stdscr, LINES - 1, 0);
	wclrtoeol(stdscr);
	
	if (scout->dir[CURR]->entries != NULL)
		selentry = scout->dir[CURR]->entries[scout->dir[CURR]->selentry];
	else
		selentry = NULL;

	/* Top bar stuff */
	wmove(stdscr, 0, 0);
	wattron(stdscr, A_BOLD);
	wattron(stdscr, COLOR_PAIR(CP_TOPBARUSER));
	printw(scout->username);
	waddch(stdscr, ' ');
	wattroff(stdscr, COLOR_PAIR(CP_TOPBARUSER));
	wattron(stdscr, COLOR_PAIR(CP_TOPBARPATH));
	printw(scout->dir[CURR]->path);
	if (scout->dir[CURR]->path[1] != '\0')
		waddch(stdscr, '/');
	wattroff(stdscr, COLOR_PAIR(CP_TOPBARPATH));
	if (selentry != NULL)
	{
		wattron(stdscr, COLOR_PAIR(CP_TOPBARFILE));
		printw(selentry->file->name);
		wattroff(stdscr, COLOR_PAIR(CP_TOPBARFILE));
	}
	waddch(stdscr, '\n');
	wattroff(stdscr, A_BOLD);

	/* Bottom bar stuff */
	wmove(stdscr, LINES - 1, 0);
	if (selentry != NULL 
	&& scoutGetFileInfo(selentry->file) != ERR)
	{
		wattron(stdscr, COLOR_PAIR(CP_BOTBARPERM));
		wprintw(stdscr, selentry->file->perms);
		wattroff(stdscr, COLOR_PAIR(CP_BOTBARPERM));
		waddch(stdscr, ' ');
		wattron(stdscr, COLOR_PAIR(CP_BOTBARUSER));
		wprintw(stdscr, selentry->file->users);
		wattroff(stdscr, COLOR_PAIR(CP_BOTBARUSER));
		waddch(stdscr, ' ');
		wattron(stdscr, COLOR_PAIR(CP_BOTBARDATE));
		wprintw(stdscr, selentry->file->dates);
		wattroff(stdscr, COLOR_PAIR(CP_BOTBARDATE));
		waddch(stdscr, ' ');
		if (selentry->file->issym)
		{
			wattron(stdscr, COLOR_PAIR(CP_BOTBARLINK));
			wprintw(stdscr, "-> ", selentry->file->lpath);
			wattroff(stdscr, COLOR_PAIR(CP_BOTBARLINK));
			if (selentry->file->lpath != NULL)
			{
				wattron(stdscr, COLOR_PAIR(CP_BOTBARLINK));
				wprintw(stdscr, selentry->file->lpath);
				wattroff(stdscr, COLOR_PAIR(CP_BOTBARLINK));
			}
			else
			{
				wattron(stdscr, COLOR_PAIR(CP_ERROR));
				wprintw(stdscr, errorSymBroken);
				wattroff(stdscr, COLOR_PAIR(CP_ERROR));
			}
			
		}
		waddch(stdscr, '\n');
		scoutFreeInfo(selentry->file);
	}

	wrefresh(stdscr);
	return OK;
}

int scoutReadDir(SDIR *dir)
{
	DIR *pdir;
	ENTR *file;
	struct dirent *d;

	if ((pdir = opendir(dir->path)) == NULL)
		return ERR;

	chdir(dir->path);
	
	while ((d = readdir(pdir)) != NULL)
	{
		if (strcmp(d->d_name, ".") == OK 
		|| strcmp(d->d_name, "..") == OK)
			continue;

		if (scoutAddFile(&file, d->d_name) != OK)
			continue; // error to log!

		if ((dir->entries = realloc(dir->entries, sizeof(NODE *) * (dir->entrycount + 1))) == NULL)
			return ERR;

		if ((dir->entries[dir->entrycount] = malloc(sizeof(NODE))) == NULL)
			return ERR;
		
		dir->entries[dir->entrycount]->file = file;
		dir->entries[dir->entrycount]->buf = NULL;
		dir->entrycount++;
	}

	if (dir->entries != NULL)
		qsort(dir->entries, dir->entrycount, sizeof(NODE *), scoutCompareEntries);

	if (dir != scout->dir[CURR])
		chdir(scout->dir[CURR]->path);

	closedir(pdir);
	return OK;
}

int scoutRun(void)
{
	int c;
	while (running && (c = wgetch(stdscr)) != EOF)
	{
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
				running = !running;
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
	scoutFreeDir(&scout->dir[PREV]);
	scoutFreeDir(&scout->dir[CURR]);
	scoutFreeDir(&scout->dir[NEXT]);
	scoutDestroyWindows();
	free(scout->username);
	free(scout);

	return OK;
}

int scoutSetup(char *path)
{
	char hostname[64];
	struct passwd *pw;
	char truepath[PATH_MAX];

	if (realpath(path, truepath) == NULL)
		return ERR;

	if ((scout = calloc(1, sizeof(struct mainstruct))) == NULL)
		return ERR;

	if ((scout->dir[CURR] = calloc(1, sizeof(SDIR))) == NULL)
		return ERR;
	
	if ((scout->dir[CURR]->path = malloc(sizeof(char *) * (strlen(truepath) + 1))) == NULL)
		return ERR;
	strcpy(scout->dir[CURR]->path, truepath);

	pw = getpwuid(geteuid());
	gethostname(hostname, sizeof(hostname));
	if ((scout->username = malloc(sizeof(char *) * (strlen(pw->pw_name) + strlen(hostname) + 2))) == NULL)
		return ERR;
	sprintf(scout->username, "%s@%s", pw->pw_name, hostname);

	scoutInitializeCurses();
	scoutBuildWindows();

	scoutLoadCURR(LOAD);
	scoutLoadNEXT(LOAD);
	scoutLoadPREV(LOAD);
	scoutPrintInfo();

	return OK;
}

void scoutTermResHandler(int null)
{
	int i;

	scoutDestroyWindows();
	scoutBuildWindows();
	scoutPrintInfo();
	curs_set(0);

	for (i = 0; i < 3; i++)
		if (scout->dir[i] != NULL)
			scout->dir[i]->firstentry = 0;

	scoutLoadCURR(RELOAD);
	scoutLoadNEXT(RELOAD);
	scoutLoadPREV(RELOAD);
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
	
	signal(SIGWINCH, scoutTermResHandler);

	scoutRun();

	scoutQuit();
}