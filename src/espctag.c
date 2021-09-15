/*
    espctag.c : espctag
    v0.4 - 2012-08-29
    
    Copyright (C) 2010,2011,2012 Jérôme SONRIER <jsid@emor3j.fr.eu.org>

    This file is part of espctag.

    espctag is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    espctag is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with espctag.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <argp.h>
#include <argz.h>
#include <spctag.h>

#include "constants.h"


#define exit_or_cont(ret) {              \
		if( arguments.no_error ) \
			continue;        \
		else                     \
			exit( ret );     \
        }


void process_spc_file( FILE *spc_file );
void print_tag_type();
int rename_spc_file( char *file );
char *get_new_filename( char *new_filename, char* rename_format );
int is_rsn_file( FILE *spc_file );
int backup_rsn_file( char *filename );
int unpack_rsn_file( char* filename, char *dest );
int pack_rsn_file( char* filename, char *dest );
int del_tmp_dir( char *dirname );


/* This structure holds all that is necessary to print or set a tag */
typedef struct
{
	const char *label;		/* The tag label                                 */
	char* (*get_func)();		/* The function that return the tag value        */
	int (*set_func)(char*);		/* The function that set a new value for the tag */
	int enabled;			/* Non null if the tag must be print or set      */
	char *new_value;		/* The new value of the tag                      */
} tag_params;

/* This array holds all the tags */
static tag_params tags[] = {
	{ "Song title",        spctag_get_songtitle,       spctag_set_songtitle       },
	{ "Game title",        spctag_get_gametitle,       spctag_set_gametitle       },
	{ "Dumper name",       spctag_get_dumpername,      spctag_set_dumpername      },
	{ "Comments",          spctag_get_comments,        spctag_set_comments        },
	{ "Dump date",         spctag_get_dumpdate,        spctag_set_dumpdate        },
	{ "Length(s)",         spctag_get_length,          spctag_set_length          },
	{ "Fade length(ms)",   spctag_get_fadelength,      spctag_set_fadelength      },
	{ "Artist",            spctag_get_artist,          spctag_set_artist          },
	{ "Default channels",  spctag_get_defaultchannels, spctag_set_defaultchannels },
	{ "Emulator",          spctag_get_emulator,        spctag_set_emulator        }
};

/* espctag version */
const char *argp_program_version = "espctag-0.4";
/* Bugtracker URL */
const char *argp_program_bug_address = "https://sourceforge.net/tracker/?group_id=563796&atid=2286949";
/* Help string */
static char doc[] = "espctag is an ID666 tags editor."
		"\vYou must use at least --get or --set, but if no one is present, --get will be used.";
/* Synopsis */
static char args_doc[] = "FILE ...";

/* This structure holds all "global" options */
struct arguments
{
	int set;		/* Non null if we want to set tags                  */
	int get;		/* Non null if we only want to print tags           */
	int file_name;		/* Non null if file names must be print before tags */
	int field_name;		/* Non null if field names must print               */
	int type;		/* Non null if type (text or binary) must be print  */
	int rename;		/* Non null if file must be rename                  */
	char *rename_format;	/* Format of the new file name                      */
	int backup_rsn;		/* Make a backup when a RSN file is modified        */
	int no_error;		/* Non null if no error mode is on                  */
	int verbose;		/* Non null if verbose mode is on                   */
	int all;		/* Non null if the user use the --all switch        */
	char *argz;		/* SPC file names                                   */
	size_t argz_len;	/* Length of file names                             */
};

/* The options we understand. */
static struct argp_option options[] = {
	{ 0, 0, 0, 0, "Operations (mutually exclusive) :", 1 },
	{ "set",  's', 0, 0, "Set tags"   },
	{ "get",  'g', 0, 0, "Print tags" },
	{ 0, 0, 0, 0, "Global options :", 5 },
	{ "file",       'f', 0,               0, "Print file names"        },
	{ "field",      'n', 0,               0, "Don't print field names" },
	{ "type",       't', 0,               0, "Print tags format"       },
	{ "rename",     'r', "RENAME_FORMAT", 0, "Rename file"             },
	{ "backup-rsn", 'b', 0,               0, "Backup RSN files"        },
	{ "no-error",   'e', 0,               0, "Don't stop on errors"    },
	{ "verbose",    'v', 0,               0, "Verbose mode"            },
	{ 0, 0, 0, 0, "Field selection :", 10 },
	{ "all",      'a', 0,             0,                   "Print all tags"                   },
	{ "song",     'S', "SONG_TITLE",  OPTION_ARG_OPTIONAL, "Print/Set song title"             },
	{ "game",     'G', "GAME_TITLE",  OPTION_ARG_OPTIONAL, "Print/Set game title"             },
	{ "dumper",   'N', "DUMPER_NAME", OPTION_ARG_OPTIONAL, "Print/Set dumper name"            },
	{ "comments", 'C', "COMMENTS",    OPTION_ARG_OPTIONAL, "Print/Set comments"               },
	{ "date",     'D', "MM/DD/YYYY",  OPTION_ARG_OPTIONAL, "Print/Set dump date"              },
	{ "length",   'L', "LENGTH",      OPTION_ARG_OPTIONAL, "Print/Set length"                 },
	{ "fade",     'F', "LENGTH",      OPTION_ARG_OPTIONAL, "Print/Set fade length"            },
	{ "artist",   'A', "ARTIST",      OPTION_ARG_OPTIONAL, "Print/Set artist"                 },
	{ "channels", 'M', "CHANNELS",    OPTION_ARG_OPTIONAL, "Print/Set default channels state" },
	{ "emulator", 'E', "EMULATOR",    OPTION_ARG_OPTIONAL, "Print/Set emulator"               },
	{ 0 }
};

/* Option parser */
static error_t parse_opt( int key, char *arg, struct argp_state *state )
{
	/* Get the input argument from argp_parse, which we
	know is a pointer to our arguments structure. */
	struct arguments *arguments = state->input;

	switch (key) {
		case 's':
			arguments->set = 1;
			break;
		case 'g':
			arguments->get = 1;
			break;
		case 'f':
			arguments->file_name = 1;
			break;
		case 'n':
			arguments->field_name = 0;
			break;
		case 't':
			arguments->type = 1;
			break;
		case 'r':
			arguments->rename = 1;
			arguments->rename_format = arg;
			break;
		case 'b':
			arguments->backup_rsn = 1;
			break;
		case 'e':
			arguments->no_error = 1;
			break;
		case 'v':
			arguments->verbose = 1;
			break;
		case 'a':
			arguments->all = 1;
			tags[I_SONG_TITLE].enabled = 1;
			tags[I_GAME_TITLE].enabled = 1;
			tags[I_DUMPER_NAME].enabled = 1;
			tags[I_COMMENTS].enabled = 1;
			tags[I_DUMP_DATE].enabled = 1;
			tags[I_LENGTH].enabled = 1;
			tags[I_FADE_LENGTH].enabled = 1;
			tags[I_ARTIST].enabled = 1;
			tags[I_CHANNELS].enabled = 1;
			tags[I_EMULATOR].enabled = 1;
			break;
		case 'S':
			tags[I_SONG_TITLE].enabled = 1;
			if ( arg )
				tags[I_SONG_TITLE].new_value = arg;
			else
				tags[I_SONG_TITLE].new_value = "";
			break;
		case 'G':
			tags[I_GAME_TITLE].enabled = 1;
			if ( arg )
				tags[I_GAME_TITLE].new_value = arg;
			else
				tags[I_GAME_TITLE].new_value = "";
			break;
		case 'N':
			tags[I_DUMPER_NAME].enabled = 1;
			if ( arg )
				tags[I_DUMPER_NAME].new_value = arg;
			else
				tags[I_DUMPER_NAME].new_value = "";
			break;
		case 'C':
			tags[I_COMMENTS].enabled = 1;
			if ( arg )
				tags[I_COMMENTS].new_value = arg;
			else
				tags[I_COMMENTS].new_value = "";
			break;
		case 'D':
			tags[I_DUMP_DATE].enabled = 1;
			if ( arg )
				tags[I_DUMP_DATE].new_value = arg;
			else
				tags[I_DUMP_DATE].new_value = "";
			break;
		case 'L':
			tags[I_LENGTH].enabled = 1;
			if ( arg )
				tags[I_LENGTH].new_value = arg;
			else
				tags[I_LENGTH].new_value = "";
			break;
		case 'F':
			tags[I_FADE_LENGTH].enabled = 1;
			if ( arg )
				tags[I_FADE_LENGTH].new_value = arg;
			else
				tags[I_FADE_LENGTH].new_value = "";
			break;
		case 'A':
			tags[I_ARTIST].enabled = 1;
			if ( arg )
				tags[I_ARTIST].new_value = arg;
			else
				tags[I_ARTIST].new_value = "";
			break;
		case 'M':
			tags[I_CHANNELS].enabled = 1;
			if ( arg )
				tags[I_CHANNELS].new_value = arg;
			else
				tags[I_CHANNELS].new_value = "";
			break;
		case 'E':
			tags[I_EMULATOR].enabled = 1;
			if ( arg )
				tags[I_EMULATOR].new_value = arg;
			else
				tags[I_EMULATOR].new_value = "";
			break;
		case ARGP_KEY_INIT:
			arguments->argz = 0;
			arguments->argz_len = 0;
			break;
		case ARGP_KEY_NO_ARGS:
			argp_usage (state);
			break;
		case ARGP_KEY_ARG:
			argz_add( &arguments->argz, &arguments->argz_len, arg );
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };
struct arguments arguments;		/* Our arguments     */


int main( int argc, char **argv )
{
	FILE *file;			/* Our SPC file      */
	char *filename;			/* File name to open */
	const char *prev = NULL;
	int ret;

	/* Default options values. */
	arguments.set        = 0;
	arguments.get        = 0;
	arguments.file_name  = 0;
	arguments.field_name = 1;
	arguments.type       = 0;
	arguments.rename     = 0;
	arguments.backup_rsn = 0;
	arguments.no_error   = 0;
	arguments.verbose    = 0;
	arguments.all        = 0;

	/* Parse arguments */
	argp_parse( &argp, argc, argv, 0, 0, &arguments );

	/* Print version number */
	if ( arguments.verbose ) {
		printf( "Version: %s\n", argp_program_version );
	}

	/* Print an error message if the user use --get and --set at the same time */
	if ( arguments.set && arguments.get ) {
		fprintf( stderr, "You can not use get and set operations at the same time !\n" );
		exit( E_WRONG_ARG );
	}
	/* If no --set or --get are used, only print tags */
	if ( ! arguments.set && ! arguments.get )
		arguments.get = 1;

	/* Print an error message if the user use --all and --set at the same time */
	if ( arguments.all && arguments.set ) {
		fprintf( stderr, "You can not use --all and --set at the same time !\n" );
		exit( E_WRONG_ARG );
	}

	while( ( filename = argz_next( arguments.argz, arguments.argz_len, prev ) ) ) {
		char msgerror[strlen( filename ) + 1024];	/* Error string */

		/* Open file */
		if ( ( file = fopen( filename, "r+" ) ) == NULL ) {
			sprintf( msgerror, "Unable to open file '%s'!", filename );
			perror( msgerror );
			prev = filename;
			exit_or_cont( E_OPEN_FILE );
		}

		if( is_rsn_file( file ) ) {
			char dir_template[] = "/tmp/espctag-XXXXXX";
			char *tmp_dirname;
			char *cur_dirname = getcwd( NULL, 0 );
			DIR *tmp_dir;
			struct dirent *dir_entry;

			if ( arguments.file_name || arguments.verbose )
				printf("File(RSN) : %s\n", filename );

			/* Create tmp directory */
			if( ( tmp_dirname = mkdtemp ( dir_template ) ) == NULL ) {
				perror( "Unable to create temp directory!" );
				free( cur_dirname );
				prev = filename;
				exit_or_cont( E_CREATE_DIR );
			}

			/* Unpack RSN file */
			if( ( ret = unpack_rsn_file( filename, tmp_dirname ) ) != 0 ) {
				fprintf( stderr, "%s extraction failed!\n", filename );
				del_tmp_dir( tmp_dirname );
				free( cur_dirname );
				prev = filename;
				exit_or_cont( ret );
			}

			/* Move to tmp directory */
			if( chdir( tmp_dirname ) != 0 ) {
				perror( "Can not change to temp directory!" );
				del_tmp_dir( tmp_dirname );
				free( cur_dirname );
				prev = filename;
				exit_or_cont( E_CH_DIR );
			}
			/* Open tmp directory */
			if( ( tmp_dir = opendir( tmp_dirname ) ) == NULL ) {
				perror( "Unable to open temp directory!" );
				chdir( cur_dirname );
				free( cur_dirname );
				del_tmp_dir( tmp_dirname );
				prev = filename;
				exit_or_cont( E_OPEN_DIR );
			}
			/* Process all SPC files */
			while( dir_entry = readdir( tmp_dir ) ) {
				FILE *spc_file;

				/* Ignore file if it's' not a SCP files */
				char *ext = strstr( dir_entry->d_name, ".spc" );
				if( ext == NULL )
					continue;

				/* Print file name */
				if ( arguments.file_name || arguments.verbose )
					printf("File : %s\n", dir_entry->d_name );

				/* Open file */
				if ( ( spc_file = fopen( dir_entry->d_name, "r+" ) ) == NULL ) {
					sprintf( msgerror, "Unable to open file '%s'!", dir_entry->d_name );
					perror( msgerror );
					exit_or_cont( E_OPEN_FILE );
				}

				/* Init spctag */
				if ( ( ret = spctag_init( spc_file ) ) < 0 ) {
					fprintf( stderr, "Can not init libspctag!\n" );
					fclose( spc_file );
					exit_or_cont( ret );
				}

				/* Process SPC file */
				process_spc_file( spc_file );

				/* Close file */
				fclose( spc_file );

				/* Rename file */
				if( ( ret = rename_spc_file( dir_entry->d_name ) ) != 0 ) {
					spctag_free();
					exit_or_cont( ret );
				}

				/* Free scptag memory */
				spctag_free();
			}

			/* Close tmp directory */
			closedir( tmp_dir );

			/* Close RSN file */
			fclose( file );

			/* Repack file */
			if( arguments.set || arguments.rename ) {
				/* Backup original RSN file */
				if( arguments.backup_rsn ) {
					if( ( ret = backup_rsn_file( filename ) ) != 0 ) {
						fprintf( stderr, "Can not backup %s!\n", filename );
						chdir( cur_dirname );
						free( cur_dirname );
						del_tmp_dir( tmp_dirname );
						prev = filename;
						exit_or_cont( ret );
					}
				} else 
					/* Delete RSN file */
					unlink( filename );
				/* Compress RSN FILE */
				if( ( ret = pack_rsn_file( filename, tmp_dirname ) ) != 0 ) {
					fprintf( stderr, "%s compression failed!\n", filename );
					chdir( cur_dirname );
					free( cur_dirname );
					del_tmp_dir( tmp_dirname );
					prev = filename;
					exit_or_cont( ret );
				}
			}

			/* Delete temp directory */
			if( ( ret = del_tmp_dir( tmp_dirname ) ) != 0 ) {
				chdir( cur_dirname );
				free( cur_dirname );
				prev = filename;
				exit_or_cont( ret );
			}

			/* Return to old cwd */
			if( chdir( cur_dirname ) != 0 ) {
				perror( "Can not change from temp directory!" );
				free( cur_dirname );
				prev = filename;
				exit_or_cont( E_CH_DIR );
			}

			free( cur_dirname );
			
		} else {
			/* Print file name */
			if ( arguments.file_name || arguments.verbose )
				printf("File : %s\n", filename );

			/* Init spctag */
			if ( ( ret = spctag_init( file ) ) < 0 ) {
				fprintf( stderr, "Can not init libspctag!\n" );
				fclose( file );
				prev = filename;
				exit_or_cont( ret );
			}
		
			/* Process SPC file */
			process_spc_file( file );

			/* Close file */
			fclose( file );

			/* Rename file */
			if( ( ret = rename_spc_file( filename ) ) != 0 ) {
				spctag_free();
				prev = filename;
				exit_or_cont( ret );
			}

			/* Free scptag memory */
			spctag_free();
		}

		prev = filename;
	}

	/* Free argz memory */
	free( arguments.argz );

	return( SUCCESS );
}

void process_spc_file( FILE *spc_file )
{
	int i;

	/* Print tag type (text or binary) */
	print_tag_type();

	/* For every know tag ... */
	for ( i=0; i<sizeof(tags)/sizeof(tag_params); i++ ) {
		/* Do nothing if the tag is not selected */
		if ( ! tags[i].enabled )
			continue;

		/* Print or set tag */
		if ( arguments.set ) {
			if ( arguments.verbose ) {
				printf(
					"Change %s from \"%s\" to \"%s\"\n",
					tags[i].label,
					tags[i].get_func(),
					tags[i].new_value
				);
			}
			tags[i].set_func( tags[i].new_value );
		} else {
			if ( arguments.field_name )
				printf( "%s : ", tags[i].label );

			printf( "%s\n", tags[i].get_func() );
		}
	}

	/* Save the file if something have been set */
	if ( arguments.set )
		spctag_save( spc_file );
}

void print_tag_type()
{
	if ( arguments.type || arguments.verbose ) {
		if ( spctag_txt_tag )
			printf( "Tags format : Text\n" );
		else
			printf( "Tags format : Binary\n" );
	}
}

int rename_spc_file ( char* file )
{
	if ( arguments.rename ) {
		char msgerror[strlen( file ) + 1024];	/* Error string */

		/* Get dirname */
		char *file_path = strdup( file );
		char *dir_name = dirname( file_path );

		/* Get new file name */
		char new_filename[MAX_FILENAME_LENGTH] = "\0";
		get_new_filename( new_filename, arguments.rename_format );

		if( arguments.verbose ) {
			printf( "New file name: %s\n", new_filename );
		}

		/* Rename file */
		char new_filepath[strlen( dir_name ) + strlen( new_filename ) + 2];
		strcpy( new_filepath, dir_name );
		strcat( new_filepath, "/" );
		strcat( new_filepath, new_filename );
		if( ( rename( file, new_filepath ) ) != 0 ) {
			sprintf( msgerror, "Can not rename file '%s'", file );
			perror( msgerror );
			return( E_RENAME_FILE );
		}

		free( file_path );
	}

	return( 0 );
}

char *get_new_filename( char *new_filename, char* rename_format )
{
	char msgerror[1024];		/* Error string      */
	int c;

	for( c=0; c<strlen( rename_format ); c++ ) {
		if( rename_format[c] == '%' ) {
			char* (*get_func)();
			int to_upper = 0;
			int to_lower = 0;
			char slength[2] = "\0\0";
			int length = 0;

			/* Get next char */
			c++;

			if( rename_format[c] == '<' ) {
				to_upper = 1;
				c++;
			} else if( rename_format[c] == '>' ) {
				to_lower = 1;
				c++;
			}

			while( isdigit( rename_format[c] ) ) {
				/* Ignore all digits after the two first */
				if( length < 2 ) {
					slength[length] = rename_format[c];
					length++;
				}

				/* Get next char */
				c++;
			}
			length = atoi( slength );

			switch( rename_format[c] ) {
				case 's': get_func = tags[I_SONG_TITLE].get_func;  break;
				case 'g': get_func = tags[I_GAME_TITLE].get_func;  break;
				case 'n': get_func = tags[I_DUMPER_NAME].get_func; break;
				case 'c': get_func = tags[I_COMMENTS].get_func;    break;
				case 'd': get_func = tags[I_DUMP_DATE].get_func;   break;
				case 'l': get_func = tags[I_LENGTH].get_func;      break;
				case 'f': get_func = tags[I_FADE_LENGTH].get_func; break;
				case 'a': get_func = tags[I_ARTIST].get_func;      break;
				case 'm': get_func = tags[I_CHANNELS].get_func;    break;
				case 'e': get_func = tags[I_EMULATOR].get_func;    break;
				case '%':
					if( strlen( new_filename ) < MAX_FILENAME_LENGTH )
						new_filename[strlen( new_filename )] = rename_format[c];
					continue;
					break;
				default:
					fprintf( stderr, "Unknown rename format: %%%c\n", rename_format[c] );
					exit( E_WRONG_ARG );
			}

			char *tmp_tag = get_func();

			if ( to_upper ) {
				int i;
				for( i=0; i<strlen( tmp_tag ); i++ ) {
					tmp_tag[i] = toupper( tmp_tag[i] );
				}
			} else if ( to_lower ) {
				int i;
				for( i=0; i<strlen( tmp_tag ); i++ ) {
					tmp_tag[i] = tolower( tmp_tag[i] );
				}
			}

			int i;
			int offset = 0;
			int new_filename_length = strlen( new_filename );
			for( i=new_filename_length; i<new_filename_length+strlen( tmp_tag ); i++ ) {
				if( i<MAX_FILENAME_LENGTH && !( length > 0 && ( i - new_filename_length ) > length - 1 ) ) {
					/* Ignore '/' and '\0' */
					if( tmp_tag[i-new_filename_length] == '/' || tmp_tag[i-new_filename_length] == '\0' ) {
						offset++;
						continue;
					}
					new_filename[i-offset] = tmp_tag[i-new_filename_length];
				}
			}
		} else {
			/* Ignore '/' and '\0' */
			if( rename_format[c] == '/' || rename_format == '\0' )
				continue;

			if( strlen( new_filename ) < MAX_FILENAME_LENGTH )
				new_filename[strlen( new_filename )] = rename_format[c];
		}
	}

	return( new_filename );
}

int is_rsn_file ( FILE* file )
{
	char rar_number[] = { 0x52, 0x61, 0x72, 0x21, 0X1A, 0x07, 0x00 };
	int number_length = strlen( rar_number );
	char header[number_length];
	int i;

	fseek( file, 0, SEEK_SET );
	fread( &header, number_length, 1, file );
	
	for( i=0; i<= number_length; i++ ) {
		if( header[i] != rar_number[i] )
			return 0;
	}

	return 1;
}

int backup_rsn_file ( char* filename )
{
	char backup_filename[strlen( filename + 4)];
	strcat( backup_filename, filename );
	strcat( backup_filename, ".bak" );

	if( arguments.verbose )
		printf( "Save input RSN file as \"%s\"\n", backup_filename );

	if( ( rename( filename, backup_filename ) ) != 0 ) {
		perror( "Can not backup file!" );
		return( E_RENAME_FILE );
	}

	return( 0 );
}

int unpack_rsn_file ( char* filename, char *dest )
{
	int status;
	pid_t pid;

	if( ( pid = fork() ) == -1 ) {
		perror( "Can not fork!" );
		return( E_FORK );
	} else if( pid == 0 ) {
		/* This is the child process. Execute the unrar command. */
		char *unrar_args[] = { "unrar-nonfree", "e", "-y", filename, NULL };

		if( ! arguments.verbose ) {
			freopen( "/dev/null", "w", stdout );
			freopen( "/dev/null", "w", stderr );
		}

		if( chdir( dest ) != 0 ) {
			perror( "Can not change to temp directory!" );
			_exit( EXIT_FAILURE );
		}
		if( execvp( "unrar-nonfree", unrar_args ) == -1 ) {
			perror( "Can not execute unrar-nonfree!" );
			_exit( EXIT_FAILURE );
		}
		_exit( EXIT_FAILURE );
	} else {
		/* This is the parent process.  Wait for the child to complete.  */
		if( waitpid( pid, &status, 0 ) != pid ) {
			perror( "Error while waiting unrar-nonfree!" );
			return( E_FORK );
		}

		if( status != 0 ) {
			return( E_UNPACK_RSN );
		}
	}

	return( 0 );
}

int pack_rsn_file ( char* filename, char *dest )
{
	int status;
	pid_t pid;

	if( ( pid = fork() ) == -1 ) {
		perror( "Can not fork!" );
		return( E_FORK );
	} else if( pid == 0 ) {
		/* This is the child process. Execute the rar command. */
		char *rar_args[] = { "rar", "a", "-y", "-m5", "-s", filename, "*", NULL };

		if( ! arguments.verbose ) {
			freopen( "/dev/null", "w", stdout );
			freopen( "/dev/null", "w", stderr );
		}

		if( chdir( dest ) != 0 ) {
			perror( "Can not change to temp directory!" );
			_exit( EXIT_FAILURE );
		}
		if( execvp( "rar", rar_args ) == -1 ) {
			perror( "Can not execute rar!" );
			_exit( EXIT_FAILURE );
		}
		_exit( EXIT_FAILURE );
	} else {
		/* This is the parent process.  Wait for the child to complete.  */
		if( waitpid( pid, &status, 0 ) != pid ) {
			perror( "Error while waiting rar!" );
			return( E_FORK );
		}

		if( status != 0 ) {
			return( E_PACK_RSN );
		}
	}

	return( 0 );
}

int del_tmp_dir ( char* dirname )
{
	char *cur_dirname = getcwd( NULL, 0 );
	DIR *tmp_dir;
	struct dirent *dir_entry;

	/* Move to tmp directory */
	if( chdir( dirname ) != 0 ) {
		perror( "Can not change to temp directory!" );
		return( E_CH_DIR );
	}

	/* Open tmp directory */
	if( ( tmp_dir = opendir( dirname ) ) == NULL ) {
		perror( "Unable to open temp directory!" );
		return( E_OPEN_DIR );
	}

	/* Delete all files */
	while( dir_entry = readdir( tmp_dir ) ) {
		if( strcmp( dir_entry->d_name, "." ) == 0 || strcmp( dir_entry->d_name, ".." ) == 0 )
			continue;
		if( ( unlink( dir_entry->d_name ) ) != 0 ) {
			perror( "Can not delete tmp files!" );
			return( E_DEL_FILE );
		}
	}

	/* Move to old cwd */
	if( chdir( cur_dirname ) != 0 ) {
		perror( "Can not change from temp directory!" );
		free( cur_dirname );
		return( E_CH_DIR );
	}

	free( cur_dirname );

	/* Close directory */
	closedir( tmp_dir );

	/* Delete tmp directory */
	if( ( remove( dirname ) ) != 0 ) {
		perror( "Can not delete tmp directory!" );
		return( E_DEL_DIR );
	}

	return( 0 );
}
