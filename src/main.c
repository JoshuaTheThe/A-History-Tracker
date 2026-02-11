#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libgen.h>
#include <sys/stat.h>

#include <time.h>

#include <dirent.h>
#include <fnmatch.h>

#define AFHT_Directory ".fh"
#define AFHT_CommitDirectory ".fh/commit"
#define AFHT_VersionsDirectory ".fh/versions"
#define AFHT_HeadFile ".fh/HEAD"
#define AFHT_IgnoreFile ".fh/IGNORE"

void mkdir_p(const char *path)
{
	char tmp[1024];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);

	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	for (p = tmp + 1; *p; p++)
	{
		if (*p == '/')
		{
			*p = 0;
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
}

void AFHT_Init(size_t *i, int arg_c, char **arg_v)
{
	(void)i;
	(void)arg_c;
	(void)arg_v;
	struct stat st = {0};

	if (stat(AFHT_Directory, &st) == -1)
	{
		mkdir(AFHT_Directory, 0700);
		mkdir(AFHT_CommitDirectory, 0700);
		mkdir(AFHT_VersionsDirectory, 0700);
		FILE *fp = fopen(AFHT_HeadFile, "w");
		fclose(fp);
	}
	else
	{
		fprintf(stderr, " [ERROR] directory \'%s\' Already exists\n", AFHT_Directory);
	}
}

void AFHT_Add(size_t *i, int arg_c, char **arg_v)
{
	(*i) += 1;
	if ((*i) >= (size_t)arg_c)
	{
		fprintf(stderr, " [ERROR] No file given to add\n");
		return;
	}

	const char *const path = arg_v[*i];
	FILE *fp = fopen(path, "rb");
	if (!fp)
	{
		fprintf(stderr, " [ERROR] Could not open file\n");
		return;
	}

	char cpath[1024] = {0};
	snprintf(cpath, 1024, "%s/%s", AFHT_CommitDirectory, path);

	// Create directory structure
	char *dir = strdup(cpath);
	mkdir_p(dirname(dir));
	free(dir);

	FILE *f2 = fopen(cpath, "wb");
	if (!f2)
	{
		fprintf(stderr, " [ERROR] Could not open commit copy\n");
		fclose(fp);
		return;
	}

	char buffer[8192];
	size_t bytes;
	while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
	{
		fwrite(buffer, 1, bytes, f2);
	}

	fclose(f2);
	fclose(fp);
}

void AFHT_Commit(size_t *i, int arg_c, char **arg_v)
{
	(*i) += 1;
	const char *message = "no message";
	if ((*i) < (size_t)arg_c)
	{
		message = arg_v[*i];
	}

	char random[12], timestamp[12];
	unsigned long long t = (unsigned long long)time(NULL);

	snprintf(random, sizeof(random), "%08llx", (((long long)rand()) << 32) | (long long)rand());
	snprintf(timestamp, sizeof(timestamp), "%08llx", t);

	char name[128];
	snprintf(name, sizeof(name), "%s-%s", random, timestamp);

	char temp_path[256];
	snprintf(temp_path, sizeof(temp_path), "%s/versions/%s.temp", AFHT_Directory, name);

	char final_path[256];
	snprintf(final_path, sizeof(final_path), "%s/versions/%s.tar.gz", AFHT_Directory, name);

	mkdir(temp_path, 0755);

	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "cp -r %s/* %s/ 2>/dev/null", AFHT_CommitDirectory, temp_path);
	system(cmd); // Shut up, it works

	snprintf(cmd, sizeof(cmd), "tar -czf %s -C %s .", final_path, temp_path);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_path);
	system(cmd);

	char meta_path[256];
	snprintf(meta_path, sizeof(meta_path), "%s/versions/%s.meta", AFHT_Directory, name);
	FILE *meta = fopen(meta_path, "w");
	if (meta)
	{
		char parent[256] = "none";
		FILE *head = fopen(AFHT_HeadFile, "r");
		if (head)
		{
			fgets(parent, sizeof(parent), head);
			fclose(head);
		}

		fprintf(meta, "parent: %s\n", parent);
		fprintf(meta, "time: %llu\n", t);
		fprintf(meta, "msg: %s\n", message);
		fclose(meta);
	}

	FILE *head = fopen(AFHT_HeadFile, "w");
	if (head)
	{
		fprintf(head, "%s", name);
		fclose(head);
	}

	fprintf(stderr, " [INFO] Committed as %s\n", name);
	fprintf(stderr, " [INFO] %s\n", message);
}

void AFHT_Fetch(size_t *i, int arg_c, char **arg_v)
{
	(void)i;
	(void)arg_c;
	(void)arg_v;

	char head[256] = {0};
	FILE *fp = fopen(AFHT_HeadFile, "r");
	if (!fp)
	{
		fprintf(stderr, " [ERROR] No HEAD found\n");
		return;
	}
	fgets(head, sizeof(head), fp);
	fclose(fp);

	head[strcspn(head, "\n")] = 0;

	if (strlen(head) == 0)
	{
		fprintf(stderr, " [ERROR] HEAD is empty\n");
		return;
	}

	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		 "tar -xzf %s/versions/%s.tar.gz -C .",
		 AFHT_Directory, head);
	system(cmd);

	fprintf(stderr, " [INFO] Fetched %s to working directory\n", head);
}

void AFHT_Checkout(size_t *i, int arg_c, char **arg_v)
{
	(*i) += 1;
	if ((*i) >= (size_t)arg_c)
	{
		fprintf(stderr, " [ERROR] No commit specified\n");
		return;
	}

	const char *commit = arg_v[*i];
	FILE *head = fopen(AFHT_HeadFile, "w");
	if (head)
	{
		fprintf(head, "%s", commit);
		fclose(head);
	}

	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		 "tar -xzf %s/versions/%s.tar.gz -C .",
		 AFHT_Directory, commit);
	system(cmd);

	fprintf(stderr, " [INFO] Checked out %s\n", commit);
}

void AFHT_Log(size_t *i, int arg_c, char **arg_v)
{
	(void)i;
	(void)arg_c;
	(void)arg_v;

	char head[256] = {0};
	FILE *fp = fopen(AFHT_HeadFile, "r");
	if (fp)
	{
		fgets(head, sizeof(head), fp);
		fclose(fp);
		head[strcspn(head, "\n")] = 0;
	}

	fprintf(stderr, " [INFO] HEAD: %s\n", strlen(head) ? head : "(none)");
	fprintf(stderr, " [INFO] Commits:\n");

	char current[256];
	strcpy(current, head);

	while (strlen(current) > 0 && strcmp(current, "none") != 0)
	{
		char meta_path[512];
		snprintf(meta_path, sizeof(meta_path),
			 "%s/versions/%s.meta", AFHT_Directory, current);

		FILE *meta = fopen(meta_path, "r");
		if (!meta)
			break;

		char line[256];
		char msg[256] = "???";
		char time_str[256] = "???";
		char parent[256] = "none";

		while (fgets(line, sizeof(line), meta))
		{
			if (strncmp(line, "msg:", 4) == 0)
				strcpy(msg, line + 5);
			if (strncmp(line, "time:", 5) == 0)
				strcpy(time_str, line + 6);
			if (strncmp(line, "parent:", 7) == 0)
				sscanf(line + 7, "%s", parent);
		}
		fclose(meta);

		msg[strcspn(msg, "\n")] = 0;
		time_str[strcspn(time_str, "\n")] = 0;

		fprintf(stderr, "  %s  %s  %s\n", current, time_str, msg);
		strcpy(current, parent);
	}
}

void add_recursive(const char *base, const char *relative, int *ignore_count, char ignore_patterns[256][256])
{
	char fullpath[1024];
	if (strlen(relative) == 0)
		snprintf(fullpath, sizeof(fullpath), "%s", base);
	else
		snprintf(fullpath, sizeof(fullpath), "%s/%s", base, relative);
	DIR *dir = opendir(fullpath);
	if (!dir)
		return;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0 ||
		    strcmp(entry->d_name, ".fh") == 0)
			continue;

		char relpath[1024];
		if (strlen(relative) == 0)
			snprintf(relpath, sizeof(relpath), "%s%s", base, entry->d_name);
		else
			snprintf(relpath, sizeof(relpath), "%s%s/%s", base, relative, entry->d_name);

		int ignore = 0;
		for (int p = 0; p < *ignore_count; p++)
		{
			if (fnmatch(ignore_patterns[p], entry->d_name, 0) == 0 ||
			    fnmatch(ignore_patterns[p], relpath, 0) == 0)
			{
				ignore = 1;
				break;
			}
		}

		if (ignore)
			continue;

		char subpath[2048];
		snprintf(subpath, sizeof(subpath), "%s/%s", fullpath, entry->d_name);

		struct stat st;
		stat(subpath, &st);

		if (S_ISDIR(st.st_mode))
		{
			add_recursive(base, relpath, ignore_count, ignore_patterns);
		}
		else
		{
			char cpath[2048];
			snprintf(cpath, sizeof(cpath), "%s/%s", AFHT_CommitDirectory, relpath);

			char *dir = strdup(cpath);
			mkdir_p(dirname(dir));
			free(dir);

			FILE *src = fopen(subpath, "rb");
			FILE *dst = fopen(cpath, "wb");

			if (src && dst)
			{
				char buffer[8192];
				size_t bytes;
				while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
				{
					fwrite(buffer, 1, bytes, dst);
				}
				fprintf(stderr, " [ADD] %s\n", relpath);
			}

			if (src)
				fclose(src);
			if (dst)
				fclose(dst);
		}
	}
	closedir(dir);
}

void AFHT_AddDir(size_t *i, int arg_c, char **arg_v)
{
	(*i) += 1;
	if ((*i) >= (size_t)arg_c)
	{
		fprintf(stderr, " [ERROR] No directory given to add\n");
		return;
	}

	const char *const dirpath = arg_v[*i];

	char ignore_patterns[256][256] = {0};
	int ignore_count = 0;

	FILE *ignore_fp = fopen(AFHT_IgnoreFile, "r");
	if (ignore_fp)
	{
		char line[256];
		while (fgets(line, sizeof(line), ignore_fp) && ignore_count < 256)
		{
			line[strcspn(line, "\n")] = 0;
			if (line[0] && line[0] != '#')
			{
				strcpy(ignore_patterns[ignore_count++], line);
			}
		}
		fclose(ignore_fp);
	}

	add_recursive(dirpath, "", &ignore_count, ignore_patterns);
	fprintf(stderr, " [INFO] Added directory %s\n", dirpath);
}

struct
{
	const char *const cmd;
	void (*use)(size_t *i, int arg_c, char **arg_v);
} Commands[] = {
    {.cmd = "/init", .use = AFHT_Init},
    {.cmd = "/add", .use = AFHT_Add},
    {.cmd = "/com", .use = AFHT_Commit},
    {.cmd = "/fetch", .use = AFHT_Fetch},
    {.cmd = "/checkout", .use = AFHT_Checkout},
    {.cmd = "/log", .use = AFHT_Log},
    {.cmd = "/add.r", .use = AFHT_AddDir},
};

int main(int arg_c, char **arg_v)
{
	srand(time(NULL));
	for (size_t i = 0; i < (size_t)arg_c; ++i)
	{
		for (size_t j = 0; j < sizeof(Commands) / sizeof(Commands[0]); ++j)
		{
			if (!strncmp(Commands[j].cmd, arg_v[i], 1000))
				Commands[j].use(&i, arg_c, arg_v);
		}
	}
	return 0;
}
