#include "eyefi-config.h"

#include <unistd.h>
#include <fcntl.h>

static int atoo(char o)
{
	if ((o >= '0') && (o <= '7'))
		return atoh(o);
	return -1;
}

static int octal_esc_to_chr(char *input)
{
	int i=0;
	int ret = 0;
	int len = strlen(input);

	//intf("%s('%s')\n", __func__, input);
	if (input[0] != '\\')
		return -1;
	if (len < 4)
		return -1;

	for (i=1; i < len ; i++) {
		if (i > 3)
			break;
		int tmp = atoo(input[i]);
		//intf("tmp: %d\n", tmp);
		if (tmp < 0)
			return tmp;
		ret <<= 3;
		ret += tmp;
	}
	return ret;
}

static char *replace_escapes(char *str)
{
	int i;
	int output = 0;
	debug_printf(4, "%s(%s)\n", __func__, str);
	for (i=0; i < strlen(str); i++) {
		int esc = octal_esc_to_chr(&str[i]);
		if (esc >= 0) {
			str[output++] = esc;
			i += 3;
			continue;
		}
		str[output++] = str[i];
	}
	str[output] = '\0';
	debug_printf(4, "replaced escapes in: '%s' bytes of output: %d\n", str, output);
	return str;
}

int fd_flush(int fd)
{
	int ret;
	fsync(fd);
	fdatasync(fd);
	ret = posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
	if (ret)
		perror("posix_fadvise() failed");
	return ret;
}

int fs_is(char *fs, char *fs_name)
{
	return (strcmp(fs, fs_name) == 0);
}

#define LINEBUFSZ 1024
static char *check_mount_line(int line_nr, char *line)
{
	char dev[LINEBUFSZ];
	char mnt[LINEBUFSZ];
	char fs[LINEBUFSZ];
	char opt[LINEBUFSZ];
	int garb1;
	int garb2;
	int read;
	read = sscanf(&line[0], "%s %s %s %s %d %d",
			&dev[0], &mnt[0], &fs[0], &opt[0],
			&garb1, &garb2);
	// only look at fat filesystems:
	if (!fs_is(fs, "msdos") && !fs_is(fs, "vfat")) {
		debug_printf(4, "fs[%d] at '%s' is not fat, skipping...\n",
				line_nr, mnt);
		return NULL;
	}
	// Linux's /proc/mounts has spaces like this \040
	replace_escapes(&mnt[0]);
	char *file = eyefi_file_on(REQM, &mnt[0]);
	debug_printf(4, "looking for EyeFi file here: '%s'\n", file);

	struct stat statbuf;
	int statret;
	statret = stat(file, &statbuf);
	free(file);
	if (statret) {
		debug_printf(3, "fs[%d] at: %s is not an Eye-Fi card, skipping...\n",
				line_nr, &mnt[0]);
		debug_printf(4, "statret: %d\n", statret);
		return NULL;
	}
	return strdup(&mnt[0]);
}

char *locate_eyefi_mount(void)
{
	static char eyefi_mount[PATHNAME_MAX]; // PATH_MAX anyone?
	FILE *mounts;

	char line[LINEBUFSZ];
	int fs_nr = -1;

	if (strlen(eyefi_mount))
		return &eyefi_mount[0];

       	mounts = fopen("/proc/mounts", "r");

	while (fgets(&line[0], 1023, mounts)) {
		char *mnt = check_mount_line(fs_nr++, line);
		if (!mnt)
			continue;
		strcpy(&eyefi_mount[0], mnt);
		free(mnt);
		debug_printf(1, "located EyeFi card at: '%s'\n", eyefi_mount);
		break;
	}
	fclose(mounts);

	if (strlen(eyefi_mount))
		return &eyefi_mount[0];

	debug_printf(0, "unable to locate Eye-Fi card\n");
	if (eyefi_debug_level < 5) {
		debug_printf(0, "Please check that your card is inserted and mounted\n");
		debug_printf(0, "If you still have issues, please re-run with the '-d5' option and report the output\n");
	} else {
		debug_printf(0, "----------------------------------------------\n");
		debug_printf(0, "Debug information:\n");
		system("cat /proc/mounts >&2");
	}
	exit(1);
	return NULL;
}

void eject_card(void)
{
	char cmd[PATHNAME_MAX];
	sprintf(cmd, "umount '%s'", locate_eyefi_mount());
	debug_printf(1, "ejecting card: '%s'\n", cmd);
	system(cmd);
	exit(0);
}
