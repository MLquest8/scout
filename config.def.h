static const char *extVideo[]   = {"mp4", "avi", "mov"};
static const char *extAudio[]   = {"mp3", "wav", "ogg"};
static const char *extImage[]   = {"jpg", "gif", "png"};
static const char *extArchive[] = {"zip", "rar", "7z"};

static const int enablelog  = 1;
static const char *logfile  = "-log";

static const char *errorDirEmpty  = "EMPTY";
static const char *errorNoAccess  = "ACCESS DENIED";
static const char *errorSymBroken = "UNRESOLVABLE SYMLINK";

static const int nColors[][3] = {
	{CP_DEFAULT, COLOR_DEFAULT, COLOR_DEFAULT},
	{CP_EXECUTABLE, COLOR_GREEN, COLOR_DEFAULT},
	{CP_DIRECTORY, COLOR_BLUE, COLOR_DEFAULT},
	{CP_ARCHIVE, COLOR_RED, COLOR_DEFAULT},
	{CP_VIDEO, COLOR_MAGENTA, COLOR_DEFAULT},
	{CP_AUDIO, COLOR_MAGENTA, COLOR_DEFAULT},
	{CP_IMAGE, COLOR_YELLOW, COLOR_DEFAULT},
	{CP_CHR, COLOR_RED, COLOR_DEFAULT},
	{CP_BLK, COLOR_RED, COLOR_DEFAULT},
	{CP_SOCK, COLOR_RED, COLOR_DEFAULT},
	{CP_FIFO, COLOR_RED, COLOR_DEFAULT},

	{CP_HEADERUSER, COLOR_GREEN, COLOR_DEFAULT},
	{CP_HEADERPATH, COLOR_BLUE, COLOR_DEFAULT},
	{CP_HEADERFILE, COLOR_DEFAULT, COLOR_DEFAULT},
	{CP_FOOTERPERM, COLOR_CYAN, COLOR_DEFAULT},
	{CP_FOOTERUSER, COLOR_DEFAULT, COLOR_DEFAULT},
	{CP_FOOTERDATE, COLOR_DEFAULT, COLOR_DEFAULT},
	{CP_FOOTERLINK, COLOR_CYAN, COLOR_DEFAULT},

	{CP_ERROR, COLOR_WHITE, COLOR_RED},
};