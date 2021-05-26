#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <string.h>

#include "global.hpp"
#include "input_journal.hpp"
#include "output_journal.hpp"

#define MIN_ARGS_COUNT 3

params_t g_params = { 
	std::string(""), 
	std::string(""), 
	std::vector< std::string >(), 
	std::vector< std::string >(),
	false, 
	false, 
	false, 
	false 
};

void
usage(const char* nm)
{
	ERROR_NOLINE("Usage: ", nm, " [-f|--input-file] <input file > " 			\
								"[-o|--output-file] <output file> " 			\
								"[-F|--field-name] <field specifier> " 			\
								"[-V|--field-value] <filed value specifier>"    \
								"[-p|--print-all] " 							\
								"[-P|--print-matches] " 						\
                                "[-c|--confirm-matches] " 						\
								"[-y|--yes] " 									\
								"[-d|--debug]");

	ERROR_NOLINE(" ");
	ERROR_NOLINE("[-f|--input-file]   <input file>                Input journal log file (compulsory)");
	ERROR_NOLINE("[-o|--output-file]  <output file>               Output journallog file (compulsory)");
	ERROR_NOLINE("[-F|--field-name]   <field specifier>           Field specifier, may be supplied multiple times");
    ERROR_NOLINE("[-V|--field-value]  <field value specifier>     Field value specifier, may be supplied multiple times");
    ERROR_NOLINE("[-p|--print-all]                                Print all log entries");
    ERROR_NOLINE("[-P|--print-matches]                            Print all log entries that match selected criterion");
    ERROR_NOLINE("[-c|--confirm-matches]                          Confirm all matching log entries with the user");
    ERROR_NOLINE("[-y|--yes]                                      Response to all confirmation dialogues affirmatively automatically");
	ERROR_NOLINE("[-d|--debug]                                    Enable debugging");

	_exit(EXIT_FAILURE);
	return;	
}

void
parse_arguments(signed int ac, char** av)
{
	std::size_t cnt(static_cast< std::size_t >(ac));

	if (0 > ac) {
		throw std::runtime_error("::parse_arguments(): invalid/impossible negative argc encountered");
		_exit(EXIT_FAILURE);
	}

	if (MIN_ARGS_COUNT > ac) 
		usage(av[0]);

	for (std::size_t idx = 1; idx < cnt; idx++) {
		if (! ::strncmp("-f", av[idx], ::strlen("-f")) || ! ::strncmp("--input-file", av[idx], ::strlen("--input-file"))) {
			if (true != g_params.input_file.empty() || idx+1 >= cnt) 
				usage(av[0]);

			g_params.input_file = av[++idx];
			
		} else if (! ::strncmp("-o", av[idx], ::strlen("-o")) || ! ::strncmp("--output-file", av[idx], ::strlen("--output-file"))) {
			if (true != g_params.output_file.empty() || idx+1 >= cnt)
				usage(av[0]);

			g_params.output_file = av[++idx];

		} else if (! ::strncmp("-F", av[idx], ::strlen("-F")) || ! ::strncmp("--field-name", av[idx], ::strlen("--field-name"))) {
			if (idx+1 >= cnt)
				usage(av[0]);

			g_params.fields.push_back(av[++idx]);
		
		} else if (! ::strncmp("-V", av[idx], ::strlen("-V")) || ! ::strncmp("--field-value", av[idx], ::strlen("--field-value"))) {
			if (idx+1 >= cnt)
				usage(av[0]);

			g_params.field_values.push_back(av[++idx]);

		} else if (! ::strncmp("-p", av[idx], ::strlen("-p")) || ! ::strncmp("--print-all", av[idx], ::strlen("--print-all"))) {
				g_params.print_all = true;

		} else if (! ::strncmp("-P", av[idx], ::strlen("-P")) || ! ::strncmp("--print-matches", av[idx], ::strlen("--print-matches"))) {
				g_params.print_matches = true;

		} else if (! ::strncmp("-c", av[idx], ::strlen("-c")) || ! ::strncmp("--confirm-matches", av[idx], ::strlen("--confirm-matches"))) {
				g_params.confirm_matches = true;

		} else if (! ::strncmp("-y", av[idx], ::strlen("-y")) || ! ::strncmp("--yes", av[idx], ::strlen("--yes"))) {
				g_params.yes = true;

		} else if (! ::strncmp("-d", av[idx], ::strlen("-d")) || ! ::strncmp("--debug", av[idx], ::strlen("--debug"))) {		
			g_params.debug = true;

		} else {
			ERROR_NOLINE("Invalid parameter: ", av[idx]);
			usage(av[0]);
		}
	}

	if (true == g_params.input_file.empty() || true == g_params.output_file.empty()) {
		ERROR_NOLINE("No input or output file specified");
		usage(av[0]);
	}

	return;
}


bool
read_yn(void)
{
	unsigned char val(0x00);

	std::cin >> val;

	if ('N' == val || 'n' == val)
		return false;

	return true;
}

signed int
main(signed int ac, char** av)
{
	signed int 				retval(EXIT_SUCCESS);
	std::vector< uint64_t > hashes, xor_hashes;

	parse_arguments(ac, av);

	try {
		input_journal_t 	ij;

		INFO("Parsing input file");
		ij.parse(g_params.input_file.c_str());

		if (0 != g_params.fields.size()) {
			const std::size_t       fnmax(g_params.fields.size());

			INFO("Locating specified fields");
			for (std::size_t idx = 0; idx < fnmax; idx++) {
				
				if (true == ij.has_field(g_params.fields[idx])) {
					const uint64_t hash(ij.get_field_hash(g_params.fields[idx]));
					
					hashes.push_back(hash);
				} else {
					ERROR("No such field located: ", g_params.fields[idx], " typo?");
					return EXIT_FAILURE;
				}
			}
		}

		if (0 != g_params.field_values.size()) {
			const std::size_t fvmax(g_params.field_values.size());
		
			INFO("Locating specified field values");
			for (std::size_t idx = 0; idx < fvmax; idx++) {
				if (true == ij.has_field_value(g_params.field_values[idx])) {
					const uint64_t hash(ij.get_field_value_hash(g_params.field_values[idx]));

					hashes.push_back(hash);
				} else {
					ERROR("No such field value located: ", g_params.field_values[idx], " typo?");
					return EXIT_FAILURE;
				}
			}
		}

		{
			const std::size_t 			hmax(hashes.size());
			const std::size_t 			emax(ij.entry_objects_size());
			std::vector< entry_obj_t >& entries(ij.entry_objects());

			INFO("Seaching for matches to specified criterion");
			for (std::size_t hidx = 0; hidx < hmax; hidx++) {
				for (std::size_t eidx = 0; eidx < emax; eidx++) {
					entry_obj_t& eobj(entries[eidx]);

					if (true == eobj.has_item_hash(hashes[hidx])) 
						xor_hashes.push_back(eobj.xor_hash());
				}
			}

		}

		INFO(xor_hashes.size(), " matches identified");
		{
			const std::size_t 					xmax(xor_hashes.size());
			std::vector< entry_obj_t >& 		evec(ij.entry_objects());
			std::list< entry_obj_t >			entries(evec.begin(), evec.end());
			std::list< entry_obj_t >::iterator 	itr(entries.begin());

			INFO("Removing matches");
			for (std::size_t idx = 0; idx < xmax; idx++) {
				std::list< entry_obj_t >::iterator itr(entries.begin());

				while (itr != entries.end()) {
					entry_obj_t& eobj(*itr);
					
					if (true == g_params.print_all)
						INFO(eobj.to_string());

					if (xor_hashes[idx] == eobj.xor_hash()) {
						if (true == g_params.confirm_matches) {
							INFO("MATCH: ", eobj.to_string());
							INFO_PROMPT("Delete? (Y/n): ");
							if (true == read_yn()) {
								INFO("Deleting match");
								itr = entries.erase(itr);
							} else {
								INFO("Skipping match");
								itr++;
							}
							continue;
						} 

						if (true == g_params.print_matches) {
							INFO("MATCH: ", eobj.to_string());
						}
				
						itr = entries.erase(itr);	
					} else
						itr++;
				}
			}

			{	
				output_journal_t oj(ij);
				input_journal_t  tmp;
				
				oj.name(g_params.output_file.c_str());
				INFO("Rewriting modified log into memory");
				oj.update(entries);
				INFO("Rewriting modified log to disk");
				oj.write();
				INFO("Verifiying written log file");
				tmp.name(g_params.output_file.c_str());
				ij.parse(g_params.output_file.c_str());
			}
		}

	} catch (std::exception& e)
	{
		ERROR_NOLINE("Exception Caught: ", e.what());
		retval = EXIT_FAILURE;
	}

	return retval;
}
