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
enum {LOAD, RELOAD};
enum {PREV, CURR, NEXT};
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
	CP_HEADERUSER,
	CP_HEADERPATH,
	CP_HEADERFILE,
	CP_FOOTERPERM,
	CP_FOOTERUSER,
	CP_FOOTERDATE,
	CP_FOOTERLINK,
};

/* structs */
typedef struct entr
{
	int type;
	int isatu; /* is accessible to user */
	int ismrk; /* is selected */
	int issym; /* is symlink */
	int istgd; /* is tagged */
	char *name;
	char *size;
	char *perms;
	char *users;
	char *dates;
	char *lpath;
} ENTR;

typedef struct sdir
{	
	char *path;
	int selentry;
	int firstentry;
	int entrycount;
	ENTR **entries;
} SDIR;

typedef struct cach
{
	char *sel;
	char *path;
	char **marked;
	int markedcount;
	struct cach *next;
} CACH;

/* function declarations */
static int scoutAddFile(ENTR **, char *);
static int scoutBuildWindows(void);
static int scoutDestroyWindows(void);
static int scoutCacheDir(SDIR *);
static int scoutCacheSearch(SDIR *);
static int scoutCompareEntries(const void *, const void *);
static int scoutCommandLine(char *);
static int scoutFindEntry(SDIR *, ENTR *);
static int scoutFreeDir(SDIR **);
static int scoutGetFileInfo(ENTR *);
static int scoutGetFileSize(ENTR *);
static int scoutGetFileType(ENTR *);
static int scoutInitializeCurses(void);
static int scoutLoadDir(int, int);
static int scoutMarkEntry(ENTR **, int );
static int scoutMove(int);
static int scoutPrintInfo(void);
static int scoutPrintList(SDIR *, WINDOW *);
static int scoutPrintRewindList(SDIR *);
static int scoutPrintStringizeEntry(ENTR *, char *, int, int, int);
static int scoutReadDir(SDIR *);
static int scoutQuit(void);
static int scoutSetup(char *);
static void scoutSignalHandler(int);

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
	CACH *cache[HSIZE];
} *scout;

/* configuration */
#include "config.h"

int scoutAddFile(ENTR **entry, char *path)
{
	ENTR *temp;
	struct stat fstat;

	if (lstat(path, &fstat) != OK)
		return ERR;

	temp = utilsCalloc(1, sizeof(ENTR));
	temp->name = utilsMalloc(sizeof(char *) * (strlen(path) + 1));
	strcpy(temp->name, path);
	scoutGetFileType(temp);
	*entry = temp;

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

int scoutCacheSearch(SDIR *dir)
{
	int len;
	int i, j;
	CACH *temp;
	unsigned int hash;

	len = strlen(dir->path);
	for (i = 0; i < 3 && len > 0; i++, len--);
	hash = utilsCalcHash(&dir->path[len]);
	
	for (temp = scout->cache[hash]; temp != NULL; temp = temp->next)
	{
		if (strcmp(dir->path, temp->path) == 0)
		{
			if (temp->sel != NULL)
				if ((dir->selentry = scoutFindEntry(dir, temp->sel)) == ERR)
					dir->selentry = 0;
			
			if (temp->marked != NULL)
				for (i = 0; i < temp->markedcount; i++)
					if ((j = scoutFindEntry(dir, temp->marked[i])) != ERR)
						dir->entries[j]->ismrk = 1;

			return OK;
		}
	}

	return ERR;
}

int scoutCacheDir(SDIR *dir)
{
	int i;
	int len;
	CACH *temp;
	unsigned int hash;

	if (dir->entries == NULL)
		return OK;

	len = strlen(dir->path);
	for (i = 0; i < 3 && len > 0; i++, len--);
	hash = utilsCalcHash(&dir->path[len]);
	temp = scout->cache[hash];

	while (temp != NULL)
	{
		if (strcmp(dir->path, temp->path) == OK)
		{
			if (temp->marked != NULL)
				for (i = 0; i < temp->markedcount; i++)
					utilsFree(temp->marked[i]);

			utilsFree(temp->sel);
			utilsFree(temp->marked);
			temp->markedcount = 0;
			break;
		}
		temp = temp->next;
	}

	if (temp == NULL)
	{
		temp = utilsCalloc(1, sizeof(CACH));
		temp->path = utilsMalloc(sizeof(char *) * (strlen(dir->path) + 1));
		strcpy(temp->path, dir->path);
		temp->next = scout->cache[hash];
		scout->cache[hash] = temp;
	}

	if (dir->selentry != 0)
	{
		temp->sel = utilsMalloc(sizeof(char *) * (strlen(dir->entries[dir->selentry]->name) + 1));
		strcpy(temp->sel, dir->entries[dir->selentry]->name);
	}

	for (i = 0; i < dir->entrycount; i++)
	{
		if (dir->entries[i]->ismrk)
		{
			temp->marked = utilsRealloc(temp->marked, sizeof(char *) * (temp->markedcount + 1));
			temp->marked[temp->markedcount] = utilsMalloc(sizeof(char *) * (strlen(dir->entries[i]->name) + 1));
			strcpy(temp->marked[temp->markedcount++], dir->entries[i]->name);
		}
	}
	
	if (temp->sel == NULL && temp->marked == NULL)
	{
		temp = scout->cache[hash];
		if (strcmp(dir->path, temp->path) == 0)
			scout->cache[hash] = temp->next;
		else
		{
			while (temp->next != NULL)
			{
				if (strcmp(dir->path, temp->next->path) == 0)
					break;
				temp = temp->next;
			}
			temp->next = temp->next->next;
			temp = temp->next;
		}
		utilsFree(temp->path);
		utilsFree(temp);
	}

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

int scoutCompareEntries(const void *A, const void *B)
{
	ENTR *entryA = *(ENTR **) A;
	ENTR *entryB = *(ENTR **) B;

	if (entryA->type == CP_DIRECTORY && entryB->type == CP_DIRECTORY)
		return utilsNameCMP(entryA->name, entryB->name);
	
	if (entryA->type != CP_DIRECTORY && entryB->type != CP_DIRECTORY)
		return utilsNameCMP(entryA->name, entryB->name);

	if (entryA->type == CP_DIRECTORY && entryB->type != CP_DIRECTORY)
		return -1;

	return 1;
}

int scoutFindEntry(SDIR *dir, ENTR *entry)
{
	int loop = 0;
	int i, j, k, l;

	i = 0;
	j = dir->entrycount;
	l = (j - i) / 2;
	
	while ((k = scoutCompareEntries(&entry, &dir->entries[l])) != 0)
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

		if (loop++ > dir->entrycount)
			return ERR;
	}

	return l;
}

int scoutFreeDir(SDIR **pdir)
{
	int i = 0;
	SDIR *dir;

	if ((dir = *pdir) == NULL)
		return ERR;

	if (dir->entries != NULL)
	{
		while (i < dir->entrycount)
		{
			utilsFree(dir->entries[i]->size);
			utilsFree(dir->entries[i]->name);
			utilsFree(dir->entries[i++]);
		}
		utilsFree(dir->entries);
	}
	utilsFree(dir->path);
	utilsFree(dir);

	*pdir = NULL;
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

	entry->perms = utilsMalloc(sizeof(char *) * (strlen(permsbuf) + 1));
	strcpy(entry->perms, permsbuf);

	if ((pws = getpwuid(fstat.st_uid)) == NULL)
		return ERR;
	entry->users = utilsMalloc(sizeof(char *) * (strlen(pws->pw_name) + 1));
	strcpy(entry->users, pws->pw_name);

	time = fstat.st_mtime;
	localtime_r(&time, &stime);
	strftime(ctimebuf, 18, "%Y-%m-%d %H:%M", &stime);
	entry->dates = utilsMalloc(sizeof(char *) * (strlen(ctimebuf) + 1));
	strcpy(entry->dates, ctimebuf);

	if (entry->issym && truepath[0] != '\0')
	{
		entry->lpath = utilsMalloc(sizeof(char *) * (strlen(truepath) + 1));
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
		entry->size = utilsMalloc(sizeof(char *) * 2);
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

	entry->size = utilsMalloc(sizeof(char *) * (strlen(sizebuf) + 1));
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

int scoutLoadDir(int dir, int mode)
{
	int i, j;
	ENTR dummy;
	ENTR *selentry;

	switch (dir)
	{
		case CURR:
			if (mode == LOAD)
				scoutReadDir(scout->dir[CURR]);

			scoutPrintRewindList(scout->dir[CURR]);

			for (i = scout->dir[CURR]->firstentry, j = 0; i < scout->dir[CURR]->entrycount && j < scout->lines; i++, j++)
				if (scout->dir[CURR]->entries[i]->size == NULL)
					scoutGetFileSize(scout->dir[CURR]->entries[i]);

			scoutPrintList(scout->dir[CURR], scout->win[CURR]);
			return OK;

		case NEXT:
			if (mode == LOAD)
				scout->dir[NEXT] = utilsCalloc(1, sizeof(SDIR));

			if (scout->dir[CURR]->entries != NULL)
				selentry = scout->dir[CURR]->entries[scout->dir[CURR]->selentry];
			else
				selentry = NULL;

			if (selentry == NULL || selentry->type != CP_DIRECTORY)
			{
				wclear(scout->win[NEXT]);
				wrefresh(scout->win[NEXT]);
				return OK;
			}

			if (selentry->isatu != OK)
			{
				wclear(scout->win[NEXT]);
				wattron(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
				mvwprintw(scout->win[NEXT], 0, 0, errorNoAccess);
				wattrset(scout->win[NEXT], COLOR_PAIR(CP_ERROR));
				wrefresh(scout->win[NEXT]);
				return OK;
			}

			if (mode == LOAD)
			{
				scout->dir[NEXT]->path = utilsMalloc(sizeof(char *) * (strlen(selentry->name) + 3));
				if (scout->dir[CURR]->path[1] != '\0')
					sprintf(scout->dir[NEXT]->path, "%s/%s", scout->dir[CURR]->path, selentry->name);
				else
					sprintf(scout->dir[NEXT]->path, "/%s", selentry->name);
				scoutReadDir(scout->dir[NEXT]);
				scoutCacheSearch(scout->dir[NEXT]);
			}

			scoutPrintRewindList(scout->dir[NEXT]);
			scoutPrintList(scout->dir[NEXT], scout->win[NEXT]);
			return OK;

		case PREV:
			if (mode == LOAD)
				scout->dir[PREV] = utilsCalloc(1, sizeof(SDIR));
					
			if (scout->dir[CURR]->path[1] == '\0')
			{
				wclear(scout->win[PREV]);
				wrefresh(scout->win[PREV]);
				return OK;
			}

			if (mode == LOAD)
			{
				i = 0;
				while (scout->dir[CURR]->path[++i] != '\0');
				while (scout->dir[CURR]->path[--i] != '/');
						
				scout->dir[PREV]->path = utilsMalloc(sizeof(char *) * (i + 2));
				strncpy(scout->dir[PREV]->path, scout->dir[CURR]->path, i + 1);
				scout->dir[PREV]->path[i ? i : 1] = '\0';
				scoutReadDir(scout->dir[PREV]);

				if (scoutCacheSearch(scout->dir[PREV]) != OK)
				{
					dummy.type = CP_DIRECTORY;
					dummy.name = strrchr(scout->dir[CURR]->path, '/') + 1;
					scout->dir[PREV]->selentry = scoutFindEntry(scout->dir[PREV], &dummy);
				}
			}

			scoutPrintRewindList(scout->dir[PREV]);
			scoutPrintList(scout->dir[PREV], scout->win[PREV]);
			return OK;
		
		default:
			return ERR;
	}
	return ERR;
}

int scoutMarkEntry(ENTR **entries, int selentry)
{
	if (entries == NULL)
		return ERR;

	entries[selentry]->ismrk = !entries[selentry]->ismrk;
	return OK;
}

int scoutMove(int dir)
{
	int i, j;
	SDIR *buf;
	switch (dir)
	{
		case TOP:
			if (scout->dir[CURR]->entries == NULL)
				return ERR;

			if (scout->dir[CURR]->selentry == 0)
				return ERR;

			buf = scout->dir[NEXT];

			for (i = scout->dir[CURR]->firstentry, j = 0; i < scout->dir[CURR]->entrycount && j < scout->lines; i++, j++)
				utilsFree(scout->dir[CURR]->entries[i]->size);

			scout->dir[CURR]->firstentry = scout->dir[CURR]->selentry = 0;
			
			scoutLoadDir(CURR, RELOAD);
			scoutLoadDir(NEXT, LOAD);

			break;

		case BOT:
			if (scout->dir[CURR]->entries == NULL)
				return ERR;

			if (scout->dir[CURR]->selentry == scout->dir[CURR]->entrycount - 1)
				return ERR;

			buf = scout->dir[NEXT];

			for (i = scout->dir[CURR]->firstentry, j = 0; i < scout->dir[CURR]->entrycount && j < scout->lines; i++, j++)
				utilsFree(scout->dir[CURR]->entries[i]->size);

			scout->dir[CURR]->firstentry = scout->dir[CURR]->selentry = scout->dir[CURR]->entrycount - 1;
			for (i = 0; scout->dir[CURR]->firstentry != 0 && i < scout->lines - 1; i++)
				--scout->dir[CURR]->firstentry;
			
			scoutLoadDir(CURR, RELOAD);
			scoutLoadDir(NEXT, LOAD);

			break;

		case UP:
			if (scout->dir[CURR]->entries == NULL)
				return ERR;

			if (scout->dir[CURR]->selentry == 0)
				return ERR;

			buf = scout->dir[NEXT];

			if (scout->dir[CURR]->selentry - scout->dir[CURR]->firstentry <= scout->topthrsh)
			{
				if (scout->dir[CURR]->firstentry != 0)
				{
					scout->dir[CURR]->firstentry--;
					scoutGetFileSize(scout->dir[CURR]->entries[scout->dir[CURR]->firstentry]);
					utilsFree(scout->dir[CURR]->entries[scout->dir[CURR]->firstentry + scout->lines]->size);
				}
			}

			scout->dir[CURR]->selentry--;
			scoutLoadDir(CURR, RELOAD);
			scoutLoadDir(NEXT, LOAD);
		
			break;

		case DOWN:
			if (scout->dir[CURR]->entries == NULL)
				return ERR;

			if (scout->dir[CURR]->selentry == scout->dir[CURR]->entrycount - 1)
				return ERR;

			buf = scout->dir[NEXT]; 

			if (scout->dir[CURR]->selentry - scout->dir[CURR]->firstentry >= scout->botthrsh)
			{
				if (scout->dir[CURR]->entrycount - scout->dir[CURR]->selentry > scout->topthrsh + 1)
				{
					scoutGetFileSize(scout->dir[CURR]->entries[scout->dir[CURR]->firstentry + scout->lines]);
					utilsFree(scout->dir[CURR]->entries[scout->dir[CURR]->firstentry]->size);
					scout->dir[CURR]->firstentry++;
				}
			}

			scout->dir[CURR]->selentry++;
			scoutLoadDir(CURR, RELOAD);
			scoutLoadDir(NEXT, LOAD);
			
			break;

		case LEFT:
			if (scout->dir[CURR]->path[1] == '\0')
				return ERR;

			buf = scout->dir[NEXT];

			for (i = scout->dir[CURR]->firstentry, j = 0; i < scout->dir[CURR]->entrycount && j < scout->lines; i++, j++)
				utilsFree(scout->dir[CURR]->entries[i]->size);

			scout->dir[NEXT] = scout->dir[CURR];
			scout->dir[CURR] = scout->dir[PREV];
			chdir(scout->dir[CURR]->path);
			scoutLoadDir(CURR, RELOAD);
			scoutLoadDir(NEXT, RELOAD);
			scoutLoadDir(PREV, LOAD);

			break;

		case RIGHT:
			if (scout->dir[CURR]->entries == NULL)
				return ERR;

			if (scout->dir[CURR]->entries[scout->dir[CURR]->selentry]->type != CP_DIRECTORY)
				return ERR;

			if (scout->dir[CURR]->entries[scout->dir[CURR]->selentry]->isatu != OK)
				return ERR;

			buf = scout->dir[PREV];

			for (i = scout->dir[CURR]->firstentry, j = 0; i < scout->dir[CURR]->entrycount && j < scout->lines; i++, j++)
				utilsFree(scout->dir[CURR]->entries[i]->size);

			scout->dir[PREV] = scout->dir[CURR];
			scout->dir[CURR] = scout->dir[NEXT];
			chdir(scout->dir[CURR]->path);
			scoutLoadDir(PREV, RELOAD);
			scoutLoadDir(CURR, RELOAD);
			scoutLoadDir(NEXT, LOAD);

			break;

		default:
			return ERR;
	}

	scoutPrintInfo();
	scoutCacheDir(buf);
	scoutFreeDir(&buf);

	return OK;
}

int scoutPrintInfo(void)
{
	ENTR *selentry;

	/* Cleanup */
	wmove(stdscr, 0, 0);
	wclrtoeol(stdscr);
	wmove(stdscr, LINES - 1, 0);
	wclrtoeol(stdscr);
	
	if (scout->dir[CURR]->entries != NULL)
		selentry = scout->dir[CURR]->entries[scout->dir[CURR]->selentry];
	else
		selentry = NULL;

	/* Header */
	wmove(stdscr, 0, 0);
	wattron(stdscr, A_BOLD);
	wattron(stdscr, COLOR_PAIR(CP_HEADERUSER));
	wprintw(stdscr, "%s ", scout->username);
	wattroff(stdscr, COLOR_PAIR(CP_HEADERUSER));
	wattron(stdscr, COLOR_PAIR(CP_HEADERPATH));
	wprintw(stdscr, scout->dir[CURR]->path[1] != '\0' ? "%s/" : "%s", scout->dir[CURR]->path);
	wattroff(stdscr, COLOR_PAIR(CP_HEADERPATH));
	if (selentry != NULL)
	{
		wattron(stdscr, COLOR_PAIR(CP_HEADERFILE));
		printw(selentry->name);
		wattroff(stdscr, COLOR_PAIR(CP_HEADERFILE));
	}
	wattroff(stdscr, A_BOLD);

	/* Footer */
	wmove(stdscr, LINES - 1, 0);
	if (selentry != NULL 
	&& scoutGetFileInfo(selentry) != ERR)
	{
		wattron(stdscr, COLOR_PAIR(CP_FOOTERPERM));
		wprintw(stdscr, "%s ", selentry->perms);
		wattroff(stdscr, COLOR_PAIR(CP_FOOTERPERM));
		wattron(stdscr, COLOR_PAIR(CP_FOOTERUSER));
		wprintw(stdscr, "%s ", selentry->users);
		wattroff(stdscr, COLOR_PAIR(CP_FOOTERUSER));
		wattron(stdscr, COLOR_PAIR(CP_FOOTERDATE));
		wprintw(stdscr, "%s ", selentry->dates);
		wattroff(stdscr, COLOR_PAIR(CP_FOOTERDATE));
		if (selentry->issym)
		{
			if (selentry->lpath != NULL)
			{
				wattron(stdscr, COLOR_PAIR(CP_FOOTERLINK));
				wprintw(stdscr, "-> %s", selentry->lpath);
				wattroff(stdscr, COLOR_PAIR(CP_FOOTERLINK));
			}
			else
			{
				wattron(stdscr, COLOR_PAIR(CP_ERROR));
				wprintw(stdscr, "-> %s", errorSymBroken);
				wattroff(stdscr, COLOR_PAIR(CP_ERROR));
			}
		}
		utilsFree(selentry->perms);
		utilsFree(selentry->users);
		utilsFree(selentry->dates);
		utilsFree(selentry->lpath);
	}

	wrefresh(stdscr);
	return OK;
}

int scoutPrintList(SDIR *dir, WINDOW *win)
{
	ENTR *entry;
	char *string;
	int i, j, len;

	if (dir->entries == NULL)
	{
		wclear(win);
		wattron(win, COLOR_PAIR(CP_ERROR));
		mvwprintw(win, 0, 0, errorDirEmpty);
		wattrset(win, COLOR_PAIR(CP_ERROR));
		wrefresh(win);
		return OK;
	}

	if ((len = getmaxx(win)) <= 0)
		return ERR;
	string = utilsMalloc(sizeof(char *) * len);

	wclear(win);
	for (i = 0, j = dir->firstentry; i < scout->lines && j < dir->entrycount; i++, j++)
	{
		entry = dir->entries[j];
		scoutPrintStringizeEntry(entry, string, len, entry->ismrk, entry->istgd);

		if (j == dir->selentry)
			wattron(win, A_REVERSE);
		if (entry->ismrk || entry->type == CP_DIRECTORY || entry->type == CP_EXECUTABLE || entry->type >= 8)
			wattron(win, A_BOLD);

		wattron(win, COLOR_PAIR(entry->type));
		mvwprintw(win, i, 0, string);
		wattrset(win, A_NORMAL);
	}
	wrefresh(win);
	utilsFree(string);

	return OK;
}

int scoutPrintRewindList(SDIR *dir)
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

int scoutPrintStringizeEntry(ENTR *entry, char *str, int len, int ismrk, int istgd)
{
	char *extstr;
	int buf, res, loss;
	int extlen, namelen, sizelen;

	str[--len] = '\0';
	res = loss = extlen = sizelen = 0;

	if (res < len)
		str[res++] = istgd ? '*' : ' ';
	else
		return ERR;

	if (res < len)
	{
		if (ismrk)
			str[res++] = ' ';
	}
	else
		return ERR;

	if (res < len)
		str[--len] = ' ';
	else
		return ERR;
	
	if (entry->size != NULL)
	{
		sizelen = strlen(entry->size);
		if (len - sizelen - res - 2 > 0)
		{
			while (sizelen > 0)
				str[--len] = entry->size[--sizelen];
			str[--len] = ' ';
		}
	}

	namelen = strlen(entry->name);
	if ((buf = len - res - namelen) > 0)
		while (buf-- > 0)
			str[--len] = ' ';

	
	if ((extstr = strrchr(entry->name, '.')) != NULL && (extstr - entry->name) > 0)
	{
		extlen = strlen(extstr);
		namelen -= extlen;
		while (extlen > 0)
		{
			if (len - extlen - res - 3 >= 0)
			{
				if (loss)
				{
					str[--len] = '~';
					--extlen;
					loss = 0;
				}
				while (extlen > 0)
					str[--len] = extstr[--extlen];
				break;
			}
			else
			{
				--extlen;
				++loss;
			}
		}
	}

	if (len - res - 1 < 0)
		return ERR;

	while (namelen > 0)
	{
		if (len - res - namelen >= 0)
		{
			if (loss)
			{
				str[--len] = '~';
				--namelen;
				loss = 0;
			}
			while (namelen > 0)
				str[--len] = entry->name[--namelen];
			break;
		}
		else
		{
			--namelen;
			++loss;
		}
	}

	return OK;
}

int scoutReadDir(SDIR *dir)
{
	DIR *pdir;
	ENTR *entry;
	struct dirent *d;

	if ((pdir = opendir(dir->path)) == NULL)
		return ERR;

	chdir(dir->path);
	
	while ((d = readdir(pdir)) != NULL)
	{
		if (strcmp(d->d_name, ".") == OK 
		|| strcmp(d->d_name, "..") == OK)
			continue;

		if (scoutAddFile(&entry, d->d_name) != OK)
			continue; // error to log!

		dir->entries = utilsRealloc(dir->entries, sizeof(ENTR *) * (dir->entrycount + 1));
		dir->entries[dir->entrycount++] = entry;
	}

	if (dir->entries != NULL)
		qsort(dir->entries, dir->entrycount, sizeof(ENTR *), scoutCompareEntries);

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

			case ' ':
				scoutMarkEntry(scout->dir[CURR]->entries, scout->dir[CURR]->selentry);
				if (scoutMove(DOWN) == ERR)
					scoutLoadDir(CURR, RELOAD);
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
	int i, j;
	CACH *temp, *buf;

	scoutFreeDir(&scout->dir[PREV]);
	scoutFreeDir(&scout->dir[CURR]);
	scoutFreeDir(&scout->dir[NEXT]);
	scoutDestroyWindows();

	for (i = 0; i < HSIZE; i++)
	{
		temp = scout->cache[i];
		while (temp != NULL)
		{
			buf = temp;
			if (temp->marked != NULL)
				for (j = 0; j < temp->markedcount; j++)
					utilsFree(temp->marked[j]);

			utilsFree(temp->marked);
			utilsFree(temp->path);
			utilsFree(temp->sel);
			temp = temp->next;
			utilsFree(buf);
		}
	}
	utilsFree(scout->username);
	utilsFree(scout);

	return OK;
}

int scoutSetup(char *path)
{
	char hostname[64];
	struct passwd *pw;
	char truepath[PATH_MAX];

	if (realpath(path, truepath) == NULL)
		return ERR;

	scout = utilsCalloc(1, sizeof(struct mainstruct));
	scout->dir[CURR] = utilsCalloc(1, sizeof(SDIR));
	scout->dir[CURR]->path = utilsMalloc(sizeof(char *) * (strlen(truepath) + 1));
	strcpy(scout->dir[CURR]->path, truepath);

	pw = getpwuid(geteuid());
	gethostname(hostname, sizeof(hostname));
	scout->username = utilsMalloc(sizeof(char *) * (strlen(pw->pw_name) + strlen(hostname) + 2));
	sprintf(scout->username, "%s@%s", pw->pw_name, hostname);

	scoutInitializeCurses();
	scoutBuildWindows();

	scoutLoadDir(CURR, LOAD);
	scoutLoadDir(NEXT, LOAD);
	scoutLoadDir(PREV, LOAD);
	scoutPrintInfo();

	return OK;
}

void scoutSignalHandler(int sigval)
{
	int i, j;

	scoutDestroyWindows();
	scoutBuildWindows();
	scoutPrintInfo();
	curs_set(0);

	for (i = scout->dir[CURR]->firstentry, j = 0; i < scout->dir[CURR]->entrycount && j < scout->lines; i++, j++)
		utilsFree(scout->dir[CURR]->entries[i]->size);

	for (i = 0; i < 3; i++)
		if (scout->dir[i] != NULL)
			scout->dir[i]->firstentry = 0;

	scoutLoadDir(CURR, RELOAD);
	scoutLoadDir(NEXT, RELOAD);
	scoutLoadDir(PREV, RELOAD);
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

	signal(SIGWINCH, scoutSignalHandler);

	scoutRun();

	exit(scoutQuit());
}