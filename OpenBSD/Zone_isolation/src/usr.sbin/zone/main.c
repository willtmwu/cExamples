#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <zones.h>


zoneid_t zonename_to_zoneid(char* );
zoneid_t* full_zoneid_list(size_t* nzs);
__dead void usage(void);

int 
main(int argc, char** argv)
{
	int error, child_id;
	if (argc < 2) {
		usage();
	} else {
		if (strcmp("create", argv[1]) == 0 && argc == 3) {
			error = zone_create(argv[2]);
		} else if (strcmp("destroy", argv[1]) == 0 && argc == 3) {
			size_t zone_id = 0;
			char *ptr;
			zone_id = strtoul(argv[2], &ptr, 10);
			if (zone_id == 0) {
				zone_id = zonename_to_zoneid(argv[2]);
			} 
			error = zone_destroy(zone_id);
		} else if (strcmp("list", argv[1]) == 0) {
			int i = 0;
			size_t nzs = 0;
			zoneid_t* zone_list;
			zone_list = full_zoneid_list(&nzs);
			if (nzs>0){
				printf("Zone ID: ");
				for (i = 0; i< nzs; i++){
					printf("[%d]", zone_list[i]);
				}
				printf("\n");
			}
		} else if (strcmp("exec", argv[1]) == 0 && argc > 3) {
			size_t zone_id = 0;
			char *ptr;
			zone_id = strtoul(argv[2], &ptr, 10);
			if (zone_id == 0) {
				zone_id = zonename_to_zoneid(argv[2]);
			} 
			
			child_id = fork();	
			if (child_id == 0) {
				error = zone_enter(zone_id);
				if (error != -1){
					argc -= 3;
					argv += 3;
					error = execvp(argv[0], argv);
					if (error == -1){
						perror("Execvp");
					}
				} else if (error == -1) {
					perror("Child Error");
				}
				exit(1);
			}

		} else {
			usage();
		}
		if (error == -1){
			perror("Error");
			exit(1);
		}
	}
	return 0;
}


/**
* Go through the list of returned zone_id_t 
* and check if any zone name matches the argument
*/
zoneid_t
zonename_to_zoneid(char* search_name)
{
	int error,i;
	size_t nzs = 0;
	zoneid_t* zone_list;
	zone_list = full_zoneid_list(&nzs);
	
	size_t string_len = MAXZONENAMELEN;
	char* string_buffer = malloc(sizeof(char)*MAXZONENAMELEN);
	memset(string_buffer, 0, sizeof(char)*MAXZONENAMELEN);
	for(i = 0; i<nzs; i++){
		zone_name(zone_list[i],string_buffer,string_len);
		if (strcmp(search_name, string_buffer)==0) {
			return zone_list[i];
		} else {
			memset(string_buffer, 0, sizeof(char)*MAXZONENAMELEN);
		}
	}
	return -1;
}

/*
*  Return a full list so ERANGE is not an issue. 
*/
zoneid_t*
full_zoneid_list(size_t* nzs)
{
	int error;
	size_t buffer_size = 1;
	zoneid_t* buffer = malloc(sizeof(zoneid_t)*buffer_size);
	while (1){
		error = zone_list(buffer, &buffer_size);

		if ((error == -1) && (errno == ERANGE)) {
			buffer_size = buffer_size*2;
			buffer = realloc(buffer, sizeof(zoneid_t)*buffer_size);
		} else {
			break;
		}
	}
	*nzs = buffer_size;
	return buffer;
}


/*
* The usage function
*/
__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: \
		%s create zonename \n \
		%s destroy zonename|zoneid \n \
		%s list \n \
		%s exec zonename|zoneid program [arg ...]\n", 
		__progname, __progname, __progname, __progname);
	exit(1);
}


