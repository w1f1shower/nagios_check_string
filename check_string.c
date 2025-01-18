#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <argp.h>

#define OK 0
#define WARNING 1
#define CRITICAL 2
#define UNKNOWN 3

#define MAX_LINE_LENGTH 300
#define LINES_TO_CHECK_DEFAULT_VALUE 30


//	(From GNU docs)

struct arguments 
{
	unsigned short warning_threshold;
	unsigned short critical_threshold;
	unsigned short lines_to_check_default;
	char *check_string;
	char *log_file;
	bool warning_set;	// Track if --warning was set
	bool critical_set;	// Track if --critical was set
	bool file_set;		// Track if --file was set
	bool string_set;	// Track if --string was set 
};

// Default values
struct arguments args = 
{
	.warning_threshold = 0,
	.critical_threshold = 0,
	.lines_to_check_default = LINES_TO_CHECK_DEFAULT_VALUE,
	.check_string = "",
	.log_file = "",
	.warning_set = 0,
	.critical_set = 0,
	.file_set = 0,
	.string_set = 0
};

//--help stuff
static char doc[] = "Nagios monitoring check for string in specified file";
static char args_doc[] = "-w [WARNING number of errors] -c [CRITICAL number of errors] -f [FILE path to check] -l [LINES from the end to check] -s [STRING to check in file]";

static struct argp_option options[] = 
{
	{	"warning",	'w',	"WARNING",	0,	"Set WARNING threshold (mandatory)"					},
	{	"critical",	'c',	"CRITICAL",	0,	"Set CRITICAL threshold (mandatory)"					},
	{	"file",		'f',	"FILE",		0,	"Path to the log file (mandatory)"					},
	{	"string",	's',	"STRING",	0,	"String to check (mandatory)"						},
	{	"lines",	'l',	"LINES",	0,	"How many lines to check from end of file (optional, default: 30)"	},
	{0}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state);
static struct argp argp = {options, parse_opt, args_doc, doc};

//My stuff
int contains_string(const char *line); 
int get_last_lines(FILE *file, char lines[][MAX_LINE_LENGTH], int max_lines);


int main(int argc, char *argv[]) 
{
	FILE *file;
	int error_count = 0;
	int line_count = 0;

	// Parse command-line arguments
	argp_parse(&argp, argc, argv, 0, 0, &args);

	unsigned short lines_to_check = args.lines_to_check_default;
	char lines[lines_to_check][MAX_LINE_LENGTH];

	memset(lines, 0, lines_to_check * MAX_LINE_LENGTH * sizeof(char) );

	//ERROR handling
	// Validate mandatory arguments
	if (!args.warning_set || !args.critical_set || !args.file_set) 
	{
		printf("UNKNOWN: --warning --critical and --file options must be specified.\nUse --help option to see more information\n");

		return UNKNOWN;
	}

	//ERROR handling
	// Ensure critical threshold is greater than warning threshold
	if (args.critical_threshold < args.warning_threshold) 
	{
		printf("UNKNOWN: CRITICAL threshold must be greater than or equal to WARNING threshold.\nUse --help option to see more information\n");
		return UNKNOWN;
	}

	// Open the log file
	file = fopen(args.log_file, "r");
	//ERROR handling
	if (!file) 
	{
		printf("UNKNOWN: Unable to open log file: %s\n", args.log_file);
		return UNKNOWN;
	}

	// Get the last n lines
	line_count = get_last_lines(file, lines, lines_to_check);
	fclose(file);

	// Check for 500 errors in the last n lines
	for (int i = lines_to_check - line_count; i < lines_to_check; i++) 
	{
		if (contains_string(lines[i])) 
		{
		    error_count++;
		}
	}

	// Determine Nagios status based on thresholds
	if (error_count >= args.critical_threshold) 
	{
		printf("CRITICAL: Found %d \"%s\" in the last %d lines of %s\n", error_count, args.check_string, lines_to_check, args.log_file);
		return CRITICAL;
	} 

	else if (error_count >= args.warning_threshold) 
	{
		printf("WARNING: Found %d \"%s\" in the last %d lines of %s\n", error_count, args.check_string, lines_to_check, args.log_file);
		return WARNING;
	} 

	else 
	{
		printf("OK: Found %d \"%s\" in the last %d lines of %s\n", error_count, args.check_string, lines_to_check, args.log_file);
		return OK;
	}
}


//From GNU docs


static error_t parse_opt(int key, char *arg, struct argp_state *state) 
{
	struct arguments *arguments = state->input;

	switch (key) 
	{
		case 'w':
			arguments->warning_threshold = atoi(arg);
			arguments->warning_set = 1; // Mark --warning as set
			break;

		case 'c':
			arguments->critical_threshold = atoi(arg);
			arguments->critical_set = 1; // Mark --critical as set
			break;

		case 'f':
			arguments->log_file = arg;
			arguments->file_set = 1; // Mark --file as set
			break;

		case 's':
			arguments->check_string = arg;
			arguments->string_set = 1;
			break;

		case 'l':
			arguments->lines_to_check_default = atoi(arg);
			break;

		case ARGP_KEY_NO_ARGS:
		case ARGP_KEY_ARG:

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

//My stuff

// Check for string in a log line
int contains_string(const char *line) 
{
	char *status = strstr(line, args.check_string);

	if (status != NULL) return 1;
	
	else return 0;
}

// (Mess) Function to get the last n lines from a file
int get_last_lines(FILE *file, char lines[][MAX_LINE_LENGTH], int max_lines) 
{
	unsigned long pos;
	unsigned int line_count = 0;
	unsigned int index = max_lines - 1;

	fseek(file, 0, SEEK_END);
	pos = ftell(file);

	while (pos > 0) 
	{
		fseek(file, --pos, SEEK_SET);

		if (fgetc(file) == '\n') 
		{
			if (line_count < max_lines) 
			{
				fseek(file, pos + 1, SEEK_SET);
				fgets(lines[index--], MAX_LINE_LENGTH, file);
				line_count++;
			} 
			else 
			{
				break;
			}
		}
	}

	if (pos == 0) 
	{
		fseek(file, 0, SEEK_SET);
		fgets(lines[index--], MAX_LINE_LENGTH, file);
		line_count++;
	}

	return line_count;
}
