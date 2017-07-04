/*
 *  mpq-extract.c -- functions for extract files from a given mpq archive.
 *
 *  Copyright (c) 2003-2008 Maik Broemme <mbroemme@plusserver.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id: mpq-extract.c,v 1.18 2004/02/12 00:39:17 mbroemme Exp $
 */

/*
 *	listfile support added by Gavin Massey on 16/8/2010
*/

/* mpq-tools configuration includes. */
#include "config.h"

/* generic includes. */
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* needed to create directories */
#include <sys/stat.h>
#include <errno.h>

/* libmpq includes. */
#include <mpq.h>

/* define new print functions for error. */
#define ERROR(...) fprintf(stderr, __VA_ARGS__);

/* define new print functions for notification. */
#define NOTICE(...) printf(__VA_ARGS__);

/* define size of off_t */
#if _FILE_OFFSET_BITS == 64
#define OFFTSTR "lli"
#else
#define OFFTSTR "li"
#endif

typedef struct listfile_s
{
	unsigned int num_entries;
	char **entries; /* entries[file_number] PATH_MAX is string size */
	int is_loaded;
} listfile_s;

/* yeah, it's a global, sue me! */
listfile_s listfile;

/* convert Windows paths to *nix */
void convert_path(char *filename) {
	int i;
	for(i = 0; i < strlen(filename); i++)
		if(filename[i] == '\\')
			filename[i] = '/';
}
/* create nested dirs from path */
int create_dirs(int fd, char *path, mode_t modes) {
	char curr_path[PATH_MAX] = {0,};
	char *start = strchr(path, '/');
	int res = 0;
	while(start) {
		memcpy(curr_path, path, (start-path));
		if(mkdirat(fd, (const char *)curr_path, modes) != 0) {
			res = errno;
			if(res == EACCES) {
				printf("error: not allowed to create dir %s\n", curr_path);
				return EACCES;
			}
			else if(res == ENOTDIR) {
				printf("error: something majorly bad happened\n");
				return ENOTDIR;
			}
		}
		start = strchr(start+1, '/');
	}
	return 0;
}

/* read and create listfile structure */
void get_listfile(mpq_archive_s *mpq_archive, unsigned int total_files) {
	off_t listfile_size = 0;
	uint32_t listfile_number = 0;
	uint8_t *list;
	int i = 0;
	char *file;
	/* check if we can use the listfile */
	if(libmpq__file_number(mpq_archive, "(listfile)", &listfile_number) != 0) {
		NOTICE("MPQ has no listfile\n"); /* unlikely but ya never know */
	}
	else {
		libmpq__file_unpacked_size(mpq_archive, listfile_number, &listfile_size);
		list = malloc(listfile_size);
		libmpq__file_read(mpq_archive, listfile_number, list, listfile_size, NULL);

		listfile.num_entries = total_files;
		listfile.entries = calloc(total_files, sizeof(char*));

		file = strtok((char*)list, "\r\n");
		for(i = 0; i < total_files; i++)
		{
			/* must check for this first! */
			if(i == listfile_number) {
				listfile.entries[i] = calloc(1, PATH_MAX); /* so it's automatically null terminated */
				memcpy(listfile.entries[i], "listfile.txt", strlen("listfile.txt"));
				i++;
			}

			if(!file)
				break;

			listfile.entries[i] = calloc(1, PATH_MAX); /* so it's automatically null terminated */
			if(strlen(file) < PATH_MAX)
				memcpy(listfile.entries[i], file, strlen(file));
			else
				memcpy(listfile.entries[i], file, PATH_MAX);

			file = strtok(NULL, "\r\n");
		}
		/* in case it never gets it like with d2sfx.mpq and d2exp.mpq */
		if(!listfile.entries[listfile_number]) {
			listfile.entries[listfile_number] = calloc(1, PATH_MAX);
			memcpy(listfile.entries[listfile_number], "listfile.txt", strlen("listfile.txt"));
		}

		if(i != total_files)
			NOTICE("error: listfile incomplete\n");

		free(list);
		listfile.is_loaded = 1;
	}
}

void destroy_listfile()
{
	if(!listfile.is_loaded)
		return;

	int i;
	for(i = 0; i < listfile.num_entries; i++)
		free(listfile.entries[i]);

	free(listfile.entries);
	listfile.num_entries = 0;
	listfile.is_loaded = 0;
}

/* this function show the usage. */
int mpq_extract__usage(char *program_name) {

	/* show the help. */
	NOTICE("Usage: %s [OPTION] [ARCHIVE]...\n", program_name);
	NOTICE("Extracts files from a mpq-archive. (Example: %s d2speech.mpq)\n", program_name);
	NOTICE("\n");
	NOTICE("  -h, --help		shows this help screen\n");
	NOTICE("  -v, --version		shows the version information\n");
	NOTICE("  -e, --extract		extract files from the given mpq archive\n");
	NOTICE("  -l, --list		list the contents of the mpq archive\n");
	NOTICE("\n");
	NOTICE("Please report bugs to the appropriate authors, which can be found in the\n");
	NOTICE("version information. All other things can be send to <%s>\n", PACKAGE_BUGREPORT);

        /* if no error was found, return zero. */
        return 0;
}

/* this function shows the version information. */
int mpq_extract__version(char *program_name) {

	/* show the version. */
	NOTICE("%s (mopaq) %s (libmpq %s)\n", program_name, VERSION, libmpq__version());
	NOTICE("Written by %s\n", AUTHOR);
	NOTICE("\n");
	NOTICE("This is free software; see the source for copying conditions.  There is NO\n");
	NOTICE("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");

	/* if no error was found, return zero. */
	return 0;
}

/* XXX: this is a hack to make mpq-tools compile until proper
 * listfile support is added. */
int32_t libmpq__file_name(mpq_archive_s *mpq_archive, uint32_t file_number, char *filename, size_t filename_size) {

	memset(filename, 0, PATH_MAX);
	if(listfile.is_loaded) {
		if(listfile.entries[file_number]) {
			memcpy(filename, listfile.entries[file_number], PATH_MAX);
			return PATH_MAX;
		}
	}

	int32_t result = 0;

	if ((result = snprintf(filename, filename_size, "file%06i.xxx", file_number)) < 0) {
		return LIBMPQ_ERROR_FORMAT;
	}
	return result;
}

/* this function will list the archive content. */
int mpq_extract__list(char *mpq_filename, unsigned int file_number, unsigned int number, unsigned int files) {

	/* some common variables. */
	int result               = 0;
	off_t size_packed        = 0;
	off_t size_unpacked      = 0;
	unsigned int total_files = 0;
	unsigned int encrypted   = 0;
	unsigned int compressed  = 0;
	unsigned int imploded    = 0;
	unsigned int i;
	static char filename[PATH_MAX];

	mpq_archive_s *mpq_archive;
	/* open the mpq-archive. */
	if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) < 0) {

		/* something on open file failed. */
		return result;
	}

	/* fetch number of files. */
	libmpq__archive_files(mpq_archive, &total_files);
	get_listfile(mpq_archive, total_files);

	/* check if we should process all files. */
	if (file_number != -1) {

		/* check if processing multiple files. */
		if (number > 0 && files > 1 && number < files) {

			/* show empty line. */
			NOTICE("\n");
		}

		if (file_number > total_files - 1) {
			return LIBMPQ_ERROR_EXIST;
		}

		/* fetch information. */
		libmpq__file_size_packed(mpq_archive, file_number, &size_packed);
		libmpq__file_size_unpacked(mpq_archive, file_number, &size_unpacked);
		libmpq__file_encrypted(mpq_archive, file_number, &encrypted);
		libmpq__file_compressed(mpq_archive, file_number, &compressed);
		libmpq__file_imploded(mpq_archive, file_number, &imploded);
		libmpq__file_name(mpq_archive, file_number, filename, PATH_MAX);

		/* show the file information. */
		NOTICE("file number:			%i/%i\n", file_number, total_files);
		NOTICE("file packed size:		%" OFFTSTR "\n", size_packed);
		NOTICE("file unpacked size:		%" OFFTSTR "\n", size_unpacked);
		NOTICE("file compression ratio:		%.2f%%\n", (100 - fabs(((float)size_packed / (float)size_unpacked * 100))));
		NOTICE("file compressed:		%s\n", compressed ? "yes" : "no");
		NOTICE("file imploded:			%s\n", imploded ? "yes" : "no");
		NOTICE("file encrypted:			%s\n", encrypted ? "yes" : "no");
		NOTICE("file name:			%s\n", filename);
	} else {
		/* show header. */
		NOTICE("number   ucmp. size   cmp. size   ratio   cmp   imp   enc   filename\n");
		NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");

		/* loop through all files. */
		for (i = 0; i < total_files; i++) {

			/* cleanup variables. */
			size_packed   = 0;
			size_unpacked = 0;
			encrypted     = 0;
			compressed    = 0;
			imploded      = 0;

			/* fetch sizes. */
			libmpq__file_size_packed(mpq_archive, i, &size_packed);
			libmpq__file_size_unpacked(mpq_archive, i, &size_unpacked);
			libmpq__file_encrypted(mpq_archive, i, &encrypted);
			libmpq__file_compressed(mpq_archive, i, &compressed);
			libmpq__file_imploded(mpq_archive, i, &imploded);

			libmpq__file_name(mpq_archive, i, filename, PATH_MAX);

			/* show file information. */
			NOTICE("  %4i   %10" OFFTSTR "   %9" OFFTSTR " %6.0f%%   %3s   %3s   %3s   %s\n",
				i,
				size_packed,
				size_unpacked,
				(100 - fabs(((float)size_packed / (float)size_unpacked * 100))),
				compressed ? "yes" : "no",
				imploded ? "yes" : "no",
				encrypted ? "yes" : "no",
				filename
			);
		}

		/* cleanup variables. */
		size_packed   = 0;
		size_unpacked = 0;

		/* fetch sizes. */
		libmpq__archive_size_packed(mpq_archive, &size_packed);
		libmpq__archive_size_unpacked(mpq_archive, &size_unpacked);

		/* show footer. */
		NOTICE("------   ----------   ---------   -----   ---   ---   ---   --------\n");
		NOTICE("  %4i   %10" OFFTSTR "   %9" OFFTSTR " %6.0f%%   %s\n",
			total_files,
			size_packed,
			size_unpacked,
			(100 - fabs(((float)size_packed / (float)size_unpacked * 100))),
			mpq_filename);
	}
	destroy_listfile();
	/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
	libmpq__archive_close(mpq_archive);

	/* if no error was found, return zero. */
	return 0;
}

/* this function extract a single file from archive. */
int mpq_extract__extract_file(mpq_archive_s *mpq_archive, unsigned int file_number, FILE *fp) {

	/* some common variables. */
	static char filename[PATH_MAX];
	unsigned char *out_buf;
	off_t transferred = 0;
	off_t out_size    = 0;
	int result        = 0;

	libmpq__file_name(mpq_archive, file_number, filename, PATH_MAX);

	/* get/show filename to extract. */
	if (filename == NULL) {

		/* filename was not found. */
		return LIBMPQ_ERROR_EXIST;
	}

	NOTICE("extracting %s\n", filename);

	libmpq__file_size_unpacked(mpq_archive, file_number, &out_size);

	if ((out_buf = malloc(out_size)) == NULL)
		return LIBMPQ_ERROR_MALLOC;

	if ((result = libmpq__file_read(mpq_archive, file_number, out_buf, out_size, &transferred)) < 0)
		return result;

	fwrite(out_buf, 1, out_size, fp);

	/* free output buffer. */
	free(out_buf);

	/* if no error was found, return zero. */
	return 0;
}

/* this function will extract the archive content. */
int mpq_extract__extract(char *mpq_filename, unsigned int file_number) {

	/* some common variables. */
	mpq_archive_s *mpq_archive;
	static char filename[PATH_MAX];
	unsigned int i;
	unsigned int total_files = 0;
	int result               = 0;
	FILE *fp;

	/* open the mpq-archive. */
	if ((result = libmpq__archive_open(&mpq_archive, mpq_filename, -1)) < 0) {

		/* something on open archive failed. */
		return result;
	}
	/* fetch number of files. */
	libmpq__archive_files(mpq_archive, &total_files);

	/* create listfile structure */
	get_listfile(mpq_archive, total_files);

	/* check if we should process all files. */
	if (file_number != -1) {

		/* get filename. */
		libmpq__file_name(mpq_archive, file_number, filename, PATH_MAX);

		if (filename == NULL) {

			/* filename was not found. */
			return LIBMPQ_ERROR_EXIST;
		}
		convert_path(filename);

		if(create_dirs(AT_FDCWD, filename, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
			destroy_listfile();
			return LIBMPQ_ERROR_OPEN; /* might as well be */
		}

		/* open file for writing. */
		if ((fp = fopen(filename, "wb")) == NULL) {
			destroy_listfile();
			/* open file failed. */
			return LIBMPQ_ERROR_OPEN;
		}

		/* extract file. */
		if ((result = mpq_extract__extract_file(mpq_archive, file_number, fp)) < 0) {

			/* close file. */
			if ((fclose(fp)) < 0) {

				/* close file failed. */
				return LIBMPQ_ERROR_CLOSE;
			}
			destroy_listfile();
			/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
			libmpq__archive_close(mpq_archive);

			/* something on extracting file failed. */
			return result;
		}

		/* close file. */
		if ((fclose(fp)) < 0) {

			/* close file failed. */
			return LIBMPQ_ERROR_CLOSE;
		}
	} else {

		/* loop through all files. */
		for (i = 0; i < total_files; i++) {

			/* get filename. */
			libmpq__file_name(mpq_archive, i, filename, PATH_MAX);

			/* check if file exist. */
			if (filename == NULL) {

				/* filename was not found. */
				return LIBMPQ_ERROR_EXIST;
			}

			convert_path(filename);

			if(create_dirs(AT_FDCWD, filename, S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
				destroy_listfile();
				return LIBMPQ_ERROR_OPEN; /* might as well be */
			}

			/* open file for writing. */
			if ((fp = fopen(filename, "wb")) == NULL) {

				/* open file failed. */
				return LIBMPQ_ERROR_OPEN;
			}

			/* extract file. */
			if ((result = mpq_extract__extract_file(mpq_archive, i, fp)) < 0) {

				/* close file. */
				if ((fclose(fp)) < 0) {

					/* close file failed. */
					return LIBMPQ_ERROR_CLOSE;
				}
				destroy_listfile();
				/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
				libmpq__archive_close(mpq_archive);

				/* something on extracting file failed. */
				return result;
			}

			/* close file. */
			if ((fclose(fp)) < 0) {

				/* close file failed. */
				return LIBMPQ_ERROR_CLOSE;
			}
		}
	}

	destroy_listfile();
	/* always close file descriptor, file could be opened also if it is no valid mpq archive. */
	libmpq__archive_close(mpq_archive);

	/* if no error was found, return zero. */
	return 0;
}

/* the main function starts here. */
int main(int argc, char **argv) {
	/* common variables for the command line. */
	int result;
	int opt;
	int option_index = 0;
	static char const short_options[] = "hvelf:";
	static struct option const long_options[] = {
		{"help",	no_argument,		0,	'h'},
		{"version",	no_argument,		0,	'v'},
		{"extract",	no_argument,		0,	'e'},
		{"list",	no_argument,		0,	'l'},
		{0,		0,			0,	0}
	};
	optind = 0;
	opterr = 0;

	/* some common variables. */
	char *program_name;
	char mpq_filename[PATH_MAX];
	unsigned int action = 0;
	unsigned int count;

	/* get program name. */
	program_name = argv[0];
	if (program_name && strrchr(program_name, '/')) {
		program_name = strrchr(program_name, '/') + 1;
	}

	/* if no command line option was given, show some info. */
	if (argc <= 1) {

		/* show some info on how to get help. :) */
		ERROR("%s: no action was given\n", program_name);
		ERROR("Try `%s --help' for more information.\n", program_name);

		/* exit with error. */
		exit(1);
	}

	/* if no command line option was given, show some info. */
	if (argc <= 1) {

		/* show some info on how to get help. :) */
		ERROR("%s: no action was given\n", program_name);
		ERROR("Try `%s --help' for more information.\n", program_name);

		/* exit with error. */
		exit(1);
	}

	/* parse command line. */
	while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {

		/* check if all command line options are parsed. */
		if (opt == -1) {
			break;
		}

		/* parse option. */
		switch (opt) {
			case 'h':
				mpq_extract__usage(program_name);
				exit(0);
			case 'v':
				mpq_extract__version(program_name);
				exit(0);
			case 'l':
				action = 1;
				continue;
			case 'e':
				action = 2;
				continue;
			default:

				/* show some info on how to get help. :) */
				ERROR("%s: unrecognized option `%s'\n", program_name, argv[optind - 1]);
				ERROR("Try `%s --help' for more information.\n", program_name);

				/* exit with error. */
				exit(1);
		}
	}

	if (!action) {
		ERROR("%s: no action given.\n", program_name);

		ERROR("Try `%s --help' for more information.\n", program_name);

		/* exit with error. */
		exit(1);
	}

	if (optind >= argc) {
		ERROR("%s: no archive given.\n", program_name);

		ERROR("Try `%s --help' for more information.\n", program_name);

		/* exit with error. */
		exit(1);
	}

	/* we assume first parameter which is left as archive. */
	strncpy(mpq_filename, argv[optind++], PATH_MAX);

	/* count number of files to process in archive. */
	count = argc - optind;

	/* process file names. */
	do {
		unsigned int file_number = 0;

		if (argv[optind]) {
			file_number = strtol (argv[optind], NULL, 10);

			/* check whether we were given a (valid) file number. */
			if (!file_number) {
				ERROR("%s: invalid file number '%s'\n", program_name, argv[optind]);
				exit(1);
			}
		}

		/* check if we should list archive only. */
		if (action == 1) {

			/* process archive. */
			result = mpq_extract__list(mpq_filename, file_number - 1, argc - optind, count);
		}

		/* check if we should extract archive content. */
		if (action == 2) {
			/* extract archive content. */
			result = mpq_extract__extract(mpq_filename, file_number - 1);
		}

		/* check if archive was correctly opened. */
		if (result == LIBMPQ_ERROR_OPEN) {

			/* open archive failed. */
			ERROR("%s: '%s' no such file or directory\n", program_name, mpq_filename);

			/* if archive did not exist, we can stop everything. :) */
			exit(1);
		}

		/* check if file in archive exist. */
		if (result == LIBMPQ_ERROR_EXIST) {

			/* file was not found in archive. */
			ERROR("%s: '%s' no such file or directory in archive '%s'\n", program_name, argv[optind], mpq_filename);

			/* if file did not exist, we continue to next file. */
			continue;
		}
	} while (++optind < argc);

	/* execution was successful. */
	exit(0);
}
